#include "apps_known.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace {

struct BlockerEntry {
    QString label;
    QString canonical;   // blocklist key
    QString iconCls;
};

// Curated blocklist candidates: password managers + terminals.
const BlockerEntry kBlockers[] = {
    {"KeePassXC", "keepassxc",       "kde"},
    {"1Password", "1password",       "kde"},
    {"Bitwarden", "bitwarden",       "kde"},
    {"Discord",   "discord",         "disc"},
    {"Konsole",   "org.kde.konsole", "kons"},
};

constexpr int kPollMs = 3000;

} // namespace

AppsKnown::AppsKnown(const QString &seenAppsPath, QObject *parent)
    : QObject(parent),
      _path(seenAppsPath.isEmpty()
                ? QDir::homePath() + "/.local/share/xtype/active_apps.json"
                : seenAppsPath) {
    poll();
    _timer.setInterval(kPollMs);
    connect(&_timer, &QTimer::timeout, this, &AppsKnown::poll);
    _timer.start();
}

void AppsKnown::poll() {
    const QFileInfo fi(_path);
    const QDateTime mtime = fi.exists() ? fi.lastModified() : QDateTime();
    if (mtime == _mtime) return;
    _mtime = mtime;

    QVariantList seen;
    QFile f(_path);
    if (f.open(QIODevice::ReadOnly))
        seen = parseSeenApps(f.readAll());
    if (seen != _seen) {
        _seen = seen;
        emit seenAppsChanged();
    }
}

QVariantList AppsKnown::parseSeenApps(const QByteArray &json) {
    QVariantList out;
    const auto apps = QJsonDocument::fromJson(json).object().value("apps").toArray();
    for (const auto &v : apps) {
        const auto o = v.toObject();
        const QString name = o.value("name").toString();
        if (name.isEmpty()) continue;
        out.append(QVariantMap{{"name", name},
                               {"last_seen", o.value("last_seen").toInteger()}});
    }
    std::stable_sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("last_seen").toLongLong() > b.toMap().value("last_seen").toLongLong();
    });
    return out;
}

AppsKnown::DesktopInfo AppsKnown::parseDesktopEntry(const QString &text) {
    DesktopInfo info;
    bool inEntry = false;
    const auto lines = text.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.startsWith('[')) {
            inEntry = (line == "[Desktop Entry]");
            continue;
        }
        if (!inEntry) continue;
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;
        const QString key = line.left(eq).trimmed();   // "Name[pt_BR]" is a different key
        const QString val = line.mid(eq + 1).trimmed();
        if (key == "Name" && info.name.isEmpty())                 info.name = val;
        else if (key == "Icon" && info.icon.isEmpty())            info.icon = val;
        else if (key == "StartupWMClass" && info.wmClass.isEmpty()) info.wmClass = val;
    }
    return info;
}

QString AppsKnown::findDesktopFile(const QString &program, const QStringList &dirs) {
    if (program.isEmpty()) return {};
    const QString wanted = program + ".desktop";
    // 1. <program>.desktop, case-insensitive (app ids like com.t3tools.T3Code).
    for (const QString &dir : dirs) {
        QDirIterator it(dir, {"*.desktop"}, QDir::Files);
        while (it.hasNext()) {
            it.next();
            if (it.fileName().compare(wanted, Qt::CaseInsensitive) == 0)
                return it.filePath();
        }
    }
    // 2. A .desktop whose StartupWMClass is the program name.
    for (const QString &dir : dirs) {
        QDirIterator it(dir, {"*.desktop"}, QDir::Files);
        while (it.hasNext()) {
            QFile f(it.next());
            if (!f.open(QIODevice::ReadOnly)) continue;
            const auto info = parseDesktopEntry(QString::fromUtf8(f.readAll()));
            if (info.wmClass.compare(program, Qt::CaseInsensitive) == 0)
                return it.filePath();
        }
    }
    return {};
}

QString AppsKnown::fallbackLabel(const QString &program) {
    // Reverse-DNS ids read best as their last part.
    const int dot = program.lastIndexOf('.');
    return (dot >= 0 && dot + 1 < program.size()) ? program.mid(dot + 1) : program;
}

AppsKnown::DesktopInfo AppsKnown::desktopInfo(const QString &program) const {
    auto it = _desktopCache.constFind(program);
    if (it != _desktopCache.constEnd()) return *it;
    DesktopInfo info;
    const QString path = findDesktopFile(
        program, QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation));
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            info = parseDesktopEntry(QString::fromUtf8(f.readAll()));
    }
    _desktopCache.insert(program, info);
    return info;
}

QString AppsKnown::labelFor(const QString &name) const {
    const auto info = desktopInfo(name);
    return info.name.isEmpty() ? fallbackLabel(name) : info.name;
}

QVariantList AppsKnown::appList(const QVariantMap &configured, const QVariantList &seen) const {
    QVariantList out;
    QSet<QString> listed;
    for (const auto &v : seen) {
        const auto m = v.toMap();
        const QString name = m.value("name").toString();
        if (name.isEmpty() || listed.contains(name)) continue;
        listed.insert(name);
        const auto info = desktopInfo(name);
        out.append(QVariantMap{
            {"name",       name},
            {"label",      info.name.isEmpty() ? fallbackLabel(name) : info.name},
            {"iconName",   info.icon},
            {"lastSeen",   m.value("last_seen").toLongLong()},
            {"configured", configured.contains(name)},
        });
    }
    // Configured but never seen (added by hand, or seen before a reset).
    QVariantList unseen;
    for (auto it = configured.constBegin(); it != configured.constEnd(); ++it) {
        if (listed.contains(it.key())) continue;
        const auto info = desktopInfo(it.key());
        unseen.append(QVariantMap{
            {"name",       it.key()},
            {"label",      info.name.isEmpty() ? fallbackLabel(it.key()) : info.name},
            {"iconName",   info.icon},
            {"lastSeen",   0LL},
            {"configured", true},
        });
    }
    std::sort(unseen.begin(), unseen.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("label").toString().compare(
                   b.toMap().value("label").toString(), Qt::CaseInsensitive) < 0;
    });
    out.append(unseen);
    return out;
}

QVariantList AppsKnown::curatedBlockerCandidates() const {
    QVariantList out;
    for (const BlockerEntry &e : kBlockers) {
        out.append(QVariantMap{
            {"id",      e.canonical},   // canonical used as blocklist key
            {"label",   e.label},
            {"iconCls", e.iconCls},
        });
    }
    return out;
}

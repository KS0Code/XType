#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Apps for the Per-app and Block-list pages. Nothing is hard-coded: the list
// is the apps configured in [apps.*] plus the apps the engine has seen the
// user type in (~/.local/share/xtype/active_apps.json, written by the
// engine's SeenApps). Names and icons come from the apps' .desktop files.
class AppsKnown : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList seenApps READ seenApps NOTIFY seenAppsChanged)

public:
    // seenAppsPath empty → ~/.local/share/xtype/active_apps.json.
    explicit AppsKnown(const QString &seenAppsPath = {}, QObject *parent = nullptr);

    // [{ name, last_seen }] from the engine, most recent first. Re-read every
    // few seconds while the settings app is open.
    QVariantList seenApps() const { return _seen; }

    // Configured apps plus seen apps, most recently used first, then by
    // label. Each: { name, label, iconName, lastSeen, configured }.
    // Takes both inputs so QML bindings re-run when either changes.
    Q_INVOKABLE QVariantList appList(const QVariantMap &configured,
                                     const QVariantList &seen) const;

    // Display name for a program name: its .desktop Name, or a tidied
    // program name ("com.t3tools.T3Code" → "T3Code").
    Q_INVOKABLE QString labelFor(const QString &name) const;

    // Ids are program names now, so this returns `id` unchanged. Kept for
    // pages keyed by id that still map it to the [apps] key.
    Q_INVOKABLE QString canonicalForId(const QString &id) const { return id; }

    // Curated blocklist candidates: password managers + terminals.
    Q_INVOKABLE QVariantList curatedBlockerCandidates() const;

    // ── Pure helpers (exposed for tests) ──
    struct DesktopInfo { QString name; QString icon; QString wmClass; };
    static QVariantList parseSeenApps(const QByteArray &json);
    static DesktopInfo  parseDesktopEntry(const QString &text);
    // The .desktop file for a program name in `dirs`: <name>.desktop
    // (case-insensitive), else one whose StartupWMClass is the name.
    static QString      findDesktopFile(const QString &program, const QStringList &dirs);
    static QString      fallbackLabel(const QString &program);

signals:
    void seenAppsChanged();

private:
    void        poll();
    DesktopInfo desktopInfo(const QString &program) const;   // cached

    QString                              _path;
    QDateTime                            _mtime;
    QVariantList                         _seen;
    QTimer                               _timer;
    mutable QHash<QString, DesktopInfo>  _desktopCache;
};

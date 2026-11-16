#include "corpus_stats.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

// Counts whitespace-separated tokens; byte-level so multi-MB corpora stay cheap.
static int countWords(const QByteArray &line) {
    int  n      = 0;
    bool inWord = false;
    for (char c : line) {
        const bool space = c == ' ' || c == '\t' || c == '\r' || c == '\n'
                        || c == '\v' || c == '\f';
        if (!space && !inWord) ++n;
        inWord = !space;
    }
    return n;
}

static QString timestampsPath(const QString &corpusPath) {
    const QFileInfo fi(corpusPath);
    return fi.dir().filePath(fi.completeBaseName() + "_timestamps.txt");
}

CorpusStats::CorpusStats(QObject *parent) : QObject(parent) {
    _counts.wordsPerDay = QList<int>(kDays, 0);
    _timer.setInterval(3000);
    connect(&_timer, &QTimer::timeout, this, &CorpusStats::refresh);
    _timer.start();
    setCorpusPath("~/.local/share/xtype/corpus.txt");
}

QVariantList CorpusStats::wordsPerDay() const {
    QVariantList out;
    for (int n : _counts.wordsPerDay) out.append(n);
    return out;
}

void CorpusStats::setCorpusPath(const QString &path) {
    QString p = path;
    if (p.startsWith("~/")) p = QDir::homePath() + p.mid(1);
    if (p == _path) return;
    _path = p;
    _signature.clear();
    refresh();
}

QStringList CorpusStats::corpusFiles(const QString &corpusPath) {
    const QFileInfo fi(corpusPath);
    QStringList out;
    if (fi.exists()) out << fi.absoluteFilePath();
    // Rotated siblings written by CorpusCollector::rotateIfNeeded(); skip the
    // transient .tmp the engine uses for atomic pruning.
    const QStringList rotated = fi.dir().entryList({fi.fileName() + ".*"}, QDir::Files, QDir::Name);
    for (const QString &name : rotated)
        if (!name.endsWith(".tmp")) out << fi.dir().filePath(name);
    return out;
}

QString CorpusStats::fileSignature() const {
    QString sig;
    QStringList inputs = corpusFiles(_path);
    inputs << timestampsPath(_path);
    for (const QString &f : inputs) {
        const QFileInfo fi(f);
        if (!fi.exists()) continue;
        sig += f + ':' + QString::number(fi.size()) + ':'
             + QString::number(fi.lastModified().toMSecsSinceEpoch()) + ';';
    }
    return sig;
}

void CorpusStats::refresh() {
    if (_path.isEmpty()) return;
    const QDate today = QDate::currentDate();
    const QString sig = fileSignature();
    if (sig == _signature && today == _countedOn) return;
    _signature = sig;
    _countedOn = today;

    const CorpusCounts c = count(_path, QDateTime::currentDateTime());
    if (c.sentences != _counts.sentences || c.words != _counts.words
        || c.bytes != _counts.bytes || c.wordsPerDay != _counts.wordsPerDay) {
        _counts = c;
        emit statsChanged();
    }
}

CorpusCounts CorpusStats::count(const QString &corpusPath, const QDateTime &now) {
    CorpusCounts c;
    c.wordsPerDay = QList<int>(kDays, 0);

    // The engine appends one timestamp line per corpus line (same order), so
    // line i of the live corpus file was harvested at timestamps[i]. Rotated
    // files have no timestamps and only contribute to the totals.
    QList<qint64> stamps;
    {
        QFile tf(timestampsPath(corpusPath));
        if (tf.open(QIODevice::ReadOnly)) {
            for (const QByteArray &l : tf.readAll().split('\n'))
                stamps.append(l.trimmed().toLongLong());   // malformed -> 0 (undated)
        }
    }

    const QString live  = QFileInfo(corpusPath).absoluteFilePath();
    const QDate   today = now.date();

    for (const QString &path : corpusFiles(corpusPath)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray data = f.readAll();
        c.bytes += data.size();

        const bool dated = (path == live);
        const QList<QByteArray> lines = data.split('\n');
        for (qsizetype i = 0; i < lines.size(); ++i) {
            const int w = countWords(lines[i]);
            if (w == 0) continue;
            ++c.sentences;
            c.words += w;

            if (!dated || i >= stamps.size() || stamps[i] <= 0) continue;
            const QDate day = QDateTime::fromSecsSinceEpoch(stamps[i]).date();
            const qint64 ago = day.daysTo(today);
            if (ago >= 0 && ago < kDays)
                c.wordsPerDay[kDays - 1 - static_cast<int>(ago)] += w;
        }
    }
    return c;
}

int CorpusStats::wipe() {
    const int removed = wipeFiles(_path);
    _signature.clear();
    refresh();
    return removed;
}

int CorpusStats::wipeFiles(const QString &corpusPath) {
    if (corpusPath.isEmpty()) return -1;
    const QFileInfo fi(corpusPath);
    // style_profile.json sits next to the corpus (XTypeEngine derives it from
    // corpus_path's parent directory).
    QStringList targets = corpusFiles(corpusPath);
    targets << timestampsPath(corpusPath)
            << fi.dir().filePath(fi.fileName() + ".tmp")          // pruneOldEntries() scratch files
            << timestampsPath(corpusPath) + ".tmp"
            << fi.dir().filePath("style_profile.json");

    int  removed = 0;
    bool failed  = false;
    for (const QString &t : targets) {
        if (!QFileInfo::exists(t)) continue;
        if (QFile::remove(t)) ++removed;
        else                  failed = true;
    }
    return failed ? -1 : removed;
}

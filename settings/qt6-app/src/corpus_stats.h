#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

struct CorpusCounts {
    int        sentences = 0;
    int        words     = 0;
    qint64     bytes     = 0;
    QList<int> wordsPerDay;   // kDays entries, oldest first; last entry is today
};

class CorpusStats : public QObject {
    Q_OBJECT
    Q_PROPERTY(int          sentences   READ sentences   NOTIFY statsChanged)
    Q_PROPERTY(int          words       READ words       NOTIFY statsChanged)
    Q_PROPERTY(double       mb          READ mb          NOTIFY statsChanged)
    Q_PROPERTY(QVariantList wordsPerDay READ wordsPerDay NOTIFY statsChanged)

public:
    static constexpr int kDays = 7;

    explicit CorpusStats(QObject *parent = nullptr);

    int          sentences()   const { return _counts.sentences; }
    int          words()       const { return _counts.words; }
    double       mb()          const { return _counts.bytes / (1024.0 * 1024.0); }
    QVariantList wordsPerDay() const;

    // Accepts a leading "~/" (as stored in config.toml).
    void setCorpusPath(const QString &path);

    Q_INVOKABLE void refresh();

    // Deletes everything learned from typing: the corpus, its rotated copies,
    // the timestamp file and style_profile.json. Returns the number of files
    // removed, or -1 if any existing file could not be removed.
    Q_INVOKABLE int wipe();
    static int wipeFiles(const QString &corpusPath);

    // Reads the corpus file plus its rotated siblings (corpus.txt.1, ...) and
    // the engine's per-line timestamp file. `now` anchors the 7-day window.
    static CorpusCounts count(const QString &corpusPath, const QDateTime &now);

signals:
    void statsChanged();

private:
    static QStringList corpusFiles(const QString &corpusPath);
    QString            fileSignature() const;

    QString      _path;
    QString      _signature;   // size+mtime of every input; recount only on change
    QDate        _countedOn;   // day buckets shift at midnight even without writes
    CorpusCounts _counts;
    QTimer       _timer;
};

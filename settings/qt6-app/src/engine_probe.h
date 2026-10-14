#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QProcess>
#include <cstdint>
#include <optional>

class EngineProbe : public QObject {
    Q_OBJECT
    Q_PROPERTY(int     cpuPct       READ cpuPct       NOTIFY cpuPctChanged)
    Q_PROPERTY(double  ramGb        READ ramGb        NOTIFY ramGbChanged)
    Q_PROPERTY(QString state        READ state        NOTIFY stateChanged)
    Q_PROPERTY(QString stateMessage READ stateMessage NOTIFY stateMessageChanged)
    Q_PROPERTY(int     latencyP50   READ latencyP50   NOTIFY latencyP50Changed)
    // Engine counters from metrics.json. They live in the fcitx5 process and
    // reset whenever fcitx5 restarts.
    Q_PROPERTY(qint64  suggestionsGenerated READ suggestionsGenerated NOTIFY metricsChanged)
    Q_PROPERTY(qint64  suggestionsAccepted  READ suggestionsAccepted  NOTIFY metricsChanged)
    Q_PROPERTY(qint64  charsAccepted        READ charsAccepted        NOTIFY metricsChanged)
    Q_PROPERTY(double  acceptRate           READ acceptRate           NOTIFY metricsChanged)  // 0..1
    Q_PROPERTY(double  secondsSaved         READ secondsSaved         NOTIFY metricsChanged)

public:
    // Typing speed assumed for the time-saved estimate: 40 WPM x 5 chars/word.
    static constexpr double kCharsPerSecond = 40.0 * 5.0 / 60.0;
    explicit EngineProbe(QObject *parent = nullptr);

    int     cpuPct()       const { return _cpuPct; }
    double  ramGb()        const { return _ramGb; }
    QString state()        const { return _state; }
    QString stateMessage() const { return _stateMessage; }
    int     latencyP50()   const { return _latencyP50; }

    qint64  suggestionsGenerated() const { return _generated; }
    qint64  suggestionsAccepted()  const { return _accepted; }
    qint64  charsAccepted()        const { return _charsAccepted; }
    double  acceptRate()   const { return _generated > 0 ? double(_accepted) / double(_generated) : 0.0; }
    double  secondsSaved() const { return double(_charsAccepted) / kCharsPerSecond; }

    struct Metrics {
        int    latencyP50    = 0;
        qint64 generated     = 0;
        qint64 accepted      = 0;
        qint64 charsAccepted = 0;
    };
    // Parses metrics.json as written by XTypeEngine::writeMetrics().
    // Returns nullopt for unreadable/partial JSON so the last good values stay.
    static std::optional<Metrics> parseMetrics(const QByteArray &json);

signals:
    void cpuPctChanged();
    void ramGbChanged();
    void stateChanged();
    void stateMessageChanged();
    void latencyP50Changed();
    void metricsChanged();

private slots:
    void poll();

private:
    int   findOllamaPid();
    void  sampleCpu(int pid);
    void  sampleRam(int pid);
    void  checkFcitx5State();

    QTimer   _timer;
    QProcess _stateProc;  // reused for fcitx5-remote -s

    int     _cpuPct        = 0;
    double  _ramGb         = 0.0;
    QString _state         = "paused";
    QString _stateMessage;
    int     _latencyP50    = 0;
    qint64  _generated     = 0;
    qint64  _accepted      = 0;
    qint64  _charsAccepted = 0;

    // CPU delta tracking
    int         _lastPid      = -1;
    uint64_t    _lastCpuTicks = 0;
    uint64_t    _lastWallNs   = 0;
    long        _clkTck       = 100;  // sysconf(_SC_CLK_TCK), set in ctor
    bool        _demoMetrics  = false;
};

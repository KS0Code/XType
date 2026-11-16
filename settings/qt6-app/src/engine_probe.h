#pragma once

#include <QHash>
#include <QList>
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
    // NVIDIA GPU stats via nvidia-smi. gpuAvailable stays false on machines
    // without an NVIDIA GPU or without nvidia-smi; the UI hides the rows then.
    Q_PROPERTY(bool    gpuAvailable READ gpuAvailable NOTIFY gpuChanged)
    Q_PROPERTY(int     gpuPct       READ gpuPct       NOTIFY gpuChanged)  // utilisation of the GPU(s) holding the model
    Q_PROPERTY(double  vramGb       READ vramGb       NOTIFY gpuChanged)  // VRAM used by the engine processes
    Q_PROPERTY(double  vramTotalGb  READ vramTotalGb  NOTIFY gpuChanged)
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
    bool    gpuAvailable() const { return _gpuAvailable; }
    int     gpuPct()       const { return _gpuPct; }
    double  vramGb()       const { return _vramGb; }
    double  vramTotalGb()  const { return _vramTotalGb; }
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

    // Given (pid, ppid, comm) for every process, returns the `ollama` processes
    // plus all their descendants. Ollama runs the model in a child process
    // (`llama-server` or `ollama runner`), so `ollama serve` alone is nearly idle.
    struct ProcInfo { int pid; int ppid; QByteArray comm; };
    static QList<int> engineProcessTree(const QList<ProcInfo> &procs);

    // Output of `nvidia-smi --query-gpu=uuid,utilization.gpu,memory.total` and
    // `nvidia-smi --query-compute-apps=gpu_uuid,pid,used_memory`, both with
    // `--format=csv,noheader,nounits`. "[N/A]" fields count as 0.
    struct GpuSample { bool valid = false; int pct = 0; double vramGb = 0.0; double vramTotalGb = 0.0; };
    static GpuSample summarizeGpu(const QByteArray &gpuCsv, const QByteArray &appsCsv,
                                  const QList<int> &enginePids);

signals:
    void cpuPctChanged();
    void ramGbChanged();
    void stateChanged();
    void stateMessageChanged();
    void latencyP50Changed();
    void metricsChanged();
    void gpuChanged();

private slots:
    void poll();

private:
    QList<int> findEnginePids();
    void  sampleCpu(const QList<int> &pids);
    void  sampleRam(const QList<int> &pids);
    void  checkFcitx5State();
    void  startGpuQuery();
    void  onGpuQueryFinished();
    void  applyGpuSample(const GpuSample &g);
    bool  nvidiaGpuAwake() const;

    QTimer   _timer;
    QProcess _stateProc;  // reused for fcitx5-remote -s
    QProcess _gpuProc;    // nvidia-smi, run asynchronously (two queries chained)

    int     _cpuPct        = 0;
    double  _ramGb         = 0.0;
    QString _state         = "paused";
    QString _stateMessage;
    int     _latencyP50    = 0;
    qint64  _generated     = 0;
    qint64  _accepted      = 0;
    qint64  _charsAccepted = 0;

    bool        _gpuAvailable  = false;
    int         _gpuPct        = 0;
    double      _vramGb        = 0.0;
    double      _vramTotalGb   = 0.0;
    bool        _gpuDisabled   = false;  // no NVIDIA device, or nvidia-smi missing
    QStringList _nvidiaPciDevs;          // /sys/bus/pci/devices/<addr> of NVIDIA GPUs
    QList<int>  _gpuQueryPids;           // engine PIDs at the time the query started
    QByteArray  _gpuCsv;                 // first query's output while the second runs
    int         _gpuBusyTicks  = 0;      // polls skipped because nvidia-smi was still running

    // CPU delta tracking, per PID so processes coming and going (model load /
    // unload spawns and reaps the runner) don't produce bogus spikes.
    QHash<int, uint64_t> _lastCpuTicks;
    uint64_t    _lastWallNs   = 0;
    long        _clkTck       = 100;  // sysconf(_SC_CLK_TCK), set in ctor
    long        _numCpus      = 1;    // sysconf(_SC_NPROCESSORS_ONLN), set in ctor
    bool        _demoMetrics  = false;
};

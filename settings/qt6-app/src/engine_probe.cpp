#include "engine_probe.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSet>
#include <QStandardPaths>

#include <ctime>
#include <unistd.h>

static uint64_t nowNs() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

EngineProbe::EngineProbe(QObject *parent)
    : QObject(parent)
{
    _clkTck      = sysconf(_SC_CLK_TCK);
    _numCpus     = qMax(1L, sysconf(_SC_NPROCESSORS_ONLN));
    _demoMetrics = qEnvironmentVariableIsSet("XTYPE_DEMO_METRICS");

    // GPU stats: only bother if there is an NVIDIA display/3D controller
    // (PCI vendor 0x10de, class 0x03xxxx) and nvidia-smi is installed.
    const QDir pciDir("/sys/bus/pci/devices");
    for (const QString &dev : pciDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = pciDir.filePath(dev);
        QFile vendor(base + "/vendor"), cls(base + "/class");
        if (!vendor.open(QIODevice::ReadOnly) || !cls.open(QIODevice::ReadOnly)) continue;
        if (vendor.readAll().trimmed() == "0x10de" && cls.readAll().startsWith("0x03"))
            _nvidiaPciDevs.append(base);
    }
    _gpuDisabled  = _nvidiaPciDevs.isEmpty() || QStandardPaths::findExecutable("nvidia-smi").isEmpty();
    _gpuAvailable = !_gpuDisabled;
    connect(&_gpuProc, &QProcess::finished, this, &EngineProbe::onGpuQueryFinished);
    connect(&_gpuProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart) return;
        _gpuDisabled = true;
        if (_gpuAvailable) { _gpuAvailable = false; emit gpuChanged(); }
    });

    _timer.setInterval(1500);
    _timer.setSingleShot(false);
    connect(&_timer, &QTimer::timeout, this, &EngineProbe::poll);
    _timer.start();
    poll();  // first tick immediately
}

// Reads /proc/<pid>/stat. The comm field is wrapped in parens and may contain
// spaces, so everything after the last ')' is split as the remaining fields
// (fields[0] = state, [1] = ppid, [11] = utime, [12] = stime).
static bool readStat(int pid, QByteArray *comm, QList<QByteArray> *fields) {
    QFile f(QString("/proc/%1/stat").arg(pid));
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray raw = f.readAll();
    const int lparen = raw.indexOf('(');
    const int rparen = raw.lastIndexOf(')');
    if (lparen < 0 || rparen < lparen || rparen + 2 >= raw.size()) return false;
    if (comm) *comm = raw.mid(lparen + 1, rparen - lparen - 1);
    *fields = raw.mid(rparen + 2).split(' ');
    return true;
}

QList<int> EngineProbe::engineProcessTree(const QList<ProcInfo> &procs) {
    QHash<int, QList<int>> children;
    QList<int> result;
    for (const ProcInfo &p : procs) {
        children[p.ppid].append(p.pid);
        if (p.comm == "ollama") result.append(p.pid);
    }
    // Breadth-first over descendants; `seen` guards against a root that is
    // itself a descendant of another root (e.g. `ollama runner` under serve).
    QSet<int> seen(result.begin(), result.end());
    for (int i = 0; i < result.size(); ++i) {
        for (int child : children.value(result[i])) {
            if (!seen.contains(child)) { seen.insert(child); result.append(child); }
        }
    }
    return result;
}

// PIDs of every ollama process plus their descendants (the model runner).
// Empty if ollama isn't running.
QList<int> EngineProbe::findEnginePids() {
    if (_demoMetrics) return { static_cast<int>(getpid()) };

    QList<ProcInfo> procs;
    const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool ok = false;
        const int pid = entry.toInt(&ok);
        if (!ok || pid <= 0) continue;
        QByteArray comm;
        QList<QByteArray> fields;
        if (!readStat(pid, &comm, &fields) || fields.size() < 2) continue;
        procs.append({ pid, fields[1].toInt(), comm });
    }
    return engineProcessTree(procs);
}

void EngineProbe::sampleCpu(const QList<int> &pids) {
    QHash<int, uint64_t> ticks;
    for (int pid : pids) {
        QList<QByteArray> fields;
        if (!readStat(pid, nullptr, &fields) || fields.size() < 13) continue;
        bool ok1, ok2;
        const uint64_t utime = fields[11].toULongLong(&ok1);
        const uint64_t stime = fields[12].toULongLong(&ok2);
        if (ok1 && ok2) ticks.insert(pid, utime + stime);
    }

    const uint64_t wallNs = nowNs();
    if (ticks.isEmpty()) {
        if (_cpuPct != 0) { _cpuPct = 0; emit cpuPctChanged(); }
    } else if (_lastWallNs > 0) {
        // Only processes present in both samples contribute; a freshly spawned
        // runner gets its baseline this tick and is counted from the next.
        double deltaTicks = 0.0;
        for (auto it = ticks.cbegin(); it != ticks.cend(); ++it) {
            const auto prev = _lastCpuTicks.constFind(it.key());
            if (prev != _lastCpuTicks.cend() && it.value() >= prev.value())
                deltaTicks += static_cast<double>(it.value() - prev.value());
        }
        const double deltaSecs = static_cast<double>(wallNs - _lastWallNs) / 1e9;
        if (deltaSecs > 0.0) {
            // Share of total machine CPU: the runner is multi-threaded, so
            // per-core % would routinely exceed 100.
            const int pct = static_cast<int>(
                (deltaTicks / static_cast<double>(_clkTck)) / deltaSecs
                / static_cast<double>(_numCpus) * 100.0 + 0.5
            );
            const int clamped = qBound(0, pct, 100);
            if (_cpuPct != clamped) { _cpuPct = clamped; emit cpuPctChanged(); }
        }
    }

    _lastCpuTicks = std::move(ticks);
    _lastWallNs   = wallNs;
}

void EngineProbe::sampleRam(const QList<int> &pids) {
    double totalKb = 0.0;
    for (int pid : pids) {
        QFile f(QString("/proc/%1/status").arg(pid));
        if (!f.open(QIODevice::ReadOnly)) continue;
        // Don't loop on f.atEnd(): procfs files report size 0, so QFile
        // considers them at EOF before anything is read. readAll() works.
        const QList<QByteArray> lines = f.readAll().split('\n');
        for (const QByteArray &line : lines) {
            if (line.startsWith("VmRSS:")) {
                // Format: "VmRSS:    123456 kB"
                const QList<QByteArray> parts = line.simplified().split(' ');
                bool ok = false;
                const double kb = parts.size() >= 2 ? parts[1].toDouble(&ok) : 0.0;
                if (ok) totalKb += kb;
                break;
            }
        }
    }

    const double gb = totalKb / (1024.0 * 1024.0);
    if (qAbs(_ramGb - gb) > 0.005 || (gb == 0.0 && _ramGb != 0.0)) {
        _ramGb = gb;
        emit ramGbChanged();
    }
}

void EngineProbe::checkFcitx5State() {
    // Kill any stale process from a previous tick that didn't finish
    if (_stateProc.state() != QProcess::NotRunning) {
        _stateProc.kill();
        _stateProc.waitForFinished(200);
    }

    _stateProc.start("fcitx5-remote", {"--check"});
    if (!_stateProc.waitForStarted(300)) {
        // Binary missing or not executable
        const QString msg = "fcitx5 not found";
        if (_state != "error" || _stateMessage != msg) {
            _state = "error"; _stateMessage = msg;
            emit stateChanged(); emit stateMessageChanged();
        }
        return;
    }

    const bool finished = _stateProc.waitForFinished(800);
    if (!finished) {
        _stateProc.kill();
        _stateProc.waitForFinished(200);
        // Don't change state — fcitx5 is probably starting up
        return;
    }

    const int code = _stateProc.exitCode();
    const QString newState = (code == 0) ? "ready" : "paused";
    if (_state != newState || !_stateMessage.isEmpty()) {
        _state = newState;
        _stateMessage = {};
        emit stateChanged();
        emit stateMessageChanged();
    }
}

void EngineProbe::poll() {
    const QList<int> pids = findEnginePids();
    sampleCpu(pids);
    sampleRam(pids);
    _gpuQueryPids = pids;
    startGpuQuery();
    checkFcitx5State();

    QFile mf(QDir::homePath() + "/.local/share/xtype/metrics.json");
    if (mf.open(QIODevice::ReadOnly)) {
        const auto m = parseMetrics(mf.readAll());
        mf.close();
        if (!m) return;
        if (m->latencyP50 != _latencyP50) {
            _latencyP50 = m->latencyP50;
            emit latencyP50Changed();
        }
        if (m->generated != _generated || m->accepted != _accepted
            || m->charsAccepted != _charsAccepted) {
            _generated     = m->generated;
            _accepted      = m->accepted;
            _charsAccepted = m->charsAccepted;
            emit metricsChanged();
        }
    }
}

// ── GPU (NVIDIA) ─────────────────────────────────────────────────────────────

static const QStringList kGpuQueryArgs  = { "--query-gpu=uuid,utilization.gpu,memory.total",
                                            "--format=csv,noheader,nounits" };
static const QStringList kAppsQueryArgs = { "--query-compute-apps=gpu_uuid,pid,used_memory",
                                            "--format=csv,noheader,nounits" };

// False when every NVIDIA GPU is runtime-suspended (hybrid laptops). Querying
// it would wake it up and burn battery, and a suspended GPU holds no model.
bool EngineProbe::nvidiaGpuAwake() const {
    for (const QString &dev : _nvidiaPciDevs) {
        QFile f(dev + "/power/runtime_status");
        if (!f.open(QIODevice::ReadOnly) || f.readAll().trimmed() != "suspended") return true;
    }
    return false;
}

void EngineProbe::startGpuQuery() {
    if (_gpuDisabled) return;
    if (_gpuProc.state() != QProcess::NotRunning) {
        // Previous query still going. nvidia-smi can hang when the driver is
        // wedged; give up on it after a few ticks so later polls can retry.
        if (++_gpuBusyTicks >= 4) { _gpuProc.kill(); _gpuBusyTicks = 0; }
        return;
    }
    _gpuBusyTicks = 0;

    if (!nvidiaGpuAwake()) {
        GpuSample idle;
        idle.valid = true;
        idle.vramTotalGb = _vramTotalGb;  // unknown while asleep; keep the bar scale
        applyGpuSample(idle);
        return;
    }
    _gpuCsv.clear();
    _gpuProc.start("nvidia-smi", kGpuQueryArgs);
}

void EngineProbe::onGpuQueryFinished() {
    const bool ok = _gpuProc.exitStatus() == QProcess::NormalExit && _gpuProc.exitCode() == 0;
    const QByteArray out = _gpuProc.readAllStandardOutput();
    if (!ok) return;  // keep last values; try again next tick

    if (_gpuProc.arguments() == kGpuQueryArgs) {
        _gpuCsv = out;
        _gpuProc.start("nvidia-smi", kAppsQueryArgs);
        return;
    }
    applyGpuSample(summarizeGpu(_gpuCsv, out, _gpuQueryPids));
}

void EngineProbe::applyGpuSample(const GpuSample &g) {
    if (!g.valid) return;
    if (_gpuPct != g.pct || qAbs(_vramGb - g.vramGb) > 0.005
        || qAbs(_vramTotalGb - g.vramTotalGb) > 0.005) {
        _gpuPct      = g.pct;
        _vramGb      = g.vramGb;
        _vramTotalGb = g.vramTotalGb;
        emit gpuChanged();
    }
}

// Splits one CSV line from nvidia-smi into trimmed fields.
static QList<QByteArray> csvFields(const QByteArray &line) {
    QList<QByteArray> fields = line.split(',');
    for (QByteArray &f : fields) f = f.trimmed();
    return fields;
}

EngineProbe::GpuSample EngineProbe::summarizeGpu(const QByteArray &gpuCsv, const QByteArray &appsCsv,
                                                 const QList<int> &enginePids) {
    struct Gpu { int util = 0; double totalMib = 0.0; };
    QHash<QByteArray, Gpu> gpus;
    QByteArray firstUuid;
    for (const QByteArray &line : gpuCsv.split('\n')) {
        const QList<QByteArray> f = csvFields(line);
        if (f.size() < 3 || f[0].isEmpty()) continue;
        if (firstUuid.isEmpty()) firstUuid = f[0];
        gpus.insert(f[0], { f[1].toInt(), f[2].toDouble() });  // "[N/A]" -> 0
    }

    GpuSample s;
    if (gpus.isEmpty()) return s;
    s.valid = true;

    const QSet<int> pids(enginePids.begin(), enginePids.end());
    QSet<QByteArray> hosting;  // GPUs that hold part of the model
    double usedMib = 0.0;
    for (const QByteArray &line : appsCsv.split('\n')) {
        const QList<QByteArray> f = csvFields(line);
        if (f.size() < 3 || !pids.contains(f[1].toInt())) continue;
        usedMib += f[2].toDouble();
        hosting.insert(f[0]);
    }

    double totalMib = 0.0;
    for (const QByteArray &uuid : hosting) {
        const Gpu g = gpus.value(uuid);
        s.pct = qMax(s.pct, g.util);
        totalMib += g.totalMib;
    }
    // Model not on any GPU: report 0% load and scale the bar to the first GPU.
    if (hosting.isEmpty()) totalMib = gpus.value(firstUuid).totalMib;

    s.pct         = qBound(0, s.pct, 100);
    s.vramGb      = usedMib / 1024.0;
    s.vramTotalGb = totalMib / 1024.0;
    return s;
}

std::optional<EngineProbe::Metrics> EngineProbe::parseMetrics(const QByteArray &json) {
    const QJsonObject obj = QJsonDocument::fromJson(json).object();
    if (obj.isEmpty()) return std::nullopt;
    Metrics m;
    m.latencyP50    = obj.value("latency_p50_ms").toInt();
    m.generated     = obj.value("suggestions_generated").toInteger();
    m.accepted      = obj.value("suggestions_accepted").toInteger();
    m.charsAccepted = obj.value("chars_accepted").toInteger();
    return m;
}

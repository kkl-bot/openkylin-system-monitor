#include "backend/process_collector.h"
#include "backend/process_control.h"

#include <QByteArrayList>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

using namespace monitor;

namespace {

void put(const QString &path, const QByteArray &data)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), qint64(data.size()));
}

quint32 currentUid()
{
#ifdef Q_OS_LINUX
    return quint32(::geteuid());
#else
    return 1000;
#endif
}

QByteArray statLine(qint64 pid, quint64 cpu, quint64 start, quint64 rss = 256,
                    const QByteArray &name = "fixture", const QByteArray &state = "S")
{
    QByteArrayList fields;
    for (int i = 0; i < 22; ++i) fields.append("0");
    fields[0] = state;
    fields[1] = "1";
    fields[11] = QByteArray::number(cpu);
    fields[12] = "0";
    fields[17] = "3";
    fields[19] = QByteArray::number(start);
    fields[20] = "2097152";
    fields[21] = QByteArray::number(rss);
    return QByteArray::number(pid) + " (" + name + ") " + fields.join(' ') + '\n';
}

void systemFixture(const QString &root, quint64 userTicks, quint64 idleTicks)
{
    put(root + "/stat", "cpu " + QByteArray::number(userTicks) + " 0 0 " + QByteArray::number(idleTicks)
        + " 0 0 0 0 " + QByteArray::number(userTicks) + " 0\n"); // guest time is already included.
    put(root + "/meminfo", "MemTotal: 16384 kB\n");
}

void processFixture(const QString &root, qint64 pid, quint64 cpu, quint64 start,
                    quint64 read = 0, quint64 write = 0)
{
    const auto directory = root + '/' + QString::number(pid);
    put(directory + "/stat", statLine(pid, cpu, start));
    const auto uid = QByteArray::number(currentUid());
    put(directory + "/status", "Name:\tfixture\nUid:\t" + uid + '\t' + uid + '\t' + uid + '\t' + uid + '\n');
    put(directory + "/cmdline", QByteArray("fixture\0--flag\0", 15));
    put(directory + "/io", "read_bytes: " + QByteArray::number(read) + "\nwrite_bytes: " + QByteArray::number(write) + '\n');
}

} // namespace

class ProcessBackendTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cpuUsesWholeMachineCapacityAndIoUsesElapsedTime();
    void zeroIsDifferentFromUnavailable();
    void pidReuseAndExitDiscardBaselines();
    void malformedAndMissingInputsAreHandled();
    void namesWithParenthesesAndMissingStatusAreSafe();
    void counterResetsAndNonIncreasingClockDiscardRates();
    void missingIoRequiresANewBaseline();
    void foreignUidAndZombieAreReadOnly();
    void controlRejectsProtectedAndStaleIdentities();
    void controlTerminatesOnlyOwnedTestChildren();
};

void ProcessBackendTests::initTestCase()
{
#ifndef Q_OS_LINUX
    QSKIP("The process backend uses Linux /proc and pidfd APIs");
#endif
}

void ProcessBackendTests::cpuUsesWholeMachineCapacityAndIoUsesElapsedTime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 10, 100, 1024, 2048);
    ProcessCollector collector(dir.path());
    const auto first = collector.collect(1000);
    QCOMPARE(first.processes.size(), 1);
    QVERIFY(!first.processes[0].cpuPercent);
    QVERIFY(!first.processes[0].readBytesPerSecond);
    systemFixture(dir.path(), 120, 1080); // 200 machine ticks, 20 process ticks => 10% total capacity.
    processFixture(dir.path(), 42, 30, 100, 3072, 6144);
    const auto next = collector.collect(3000);
    QVERIFY(next.error.isEmpty());
    const auto &process = next.processes[0];
    QCOMPARE(process.cpuPercent.value_or(-1), 10.0);
    QCOMPARE(process.readBytesPerSecond.value_or(-1), 1024.0);
    QCOMPARE(process.writeBytesPerSecond.value_or(-1), 2048.0);
    QCOMPARE(process.parentPid, qint64(1));
    QCOMPARE(process.threads, 3);
    QCOMPARE(process.commandLine, QStringLiteral("fixture --flag"));
#ifdef Q_OS_LINUX
    QCOMPARE(process.residentBytes, quint64(256) * quint64(::sysconf(_SC_PAGESIZE)));
    QCOMPARE(process.memoryPercent.value_or(-1), 100.0 * double(process.residentBytes) / (16384 * 1024));
#endif
}

void ProcessBackendTests::zeroIsDifferentFromUnavailable()
{
    QTemporaryDir dir;
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 0, 100);
    put(dir.path() + "/42/stat", statLine(42, 0, 100, 0));
    ProcessCollector collector(dir.path());
    const auto first = collector.collect(0).processes[0];
    QVERIFY(!first.cpuPercent);
    QCOMPARE(first.memoryPercent.value_or(-1), 0.0);
    QVERIFY(!first.readBytesPerSecond);
    systemFixture(dir.path(), 100, 1000);
    const auto second = collector.collect(1000).processes[0];
    QCOMPARE(second.cpuPercent.value_or(-1), 0.0);
    QCOMPARE(second.readBytesPerSecond.value_or(-1), 0.0);
    QCOMPARE(second.writeBytesPerSecond.value_or(-1), 0.0);
    QVERIFY(QFile::remove(dir.path() + "/meminfo"));
    const auto third = collector.collect(2000).processes[0];
    QVERIFY(!third.memoryPercent);
}

void ProcessBackendTests::pidReuseAndExitDiscardBaselines()
{
    QTemporaryDir dir;
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 100, 100, 400, 500);
    ProcessCollector collector(dir.path());
    collector.collect(1000);
    systemFixture(dir.path(), 200, 1000);
    processFixture(dir.path(), 42, 150, 200, 500, 600); // Reused PID with a different birth time.
    auto snapshot = collector.collect(2000);
    QCOMPARE(snapshot.processes.size(), 1);
    QVERIFY(!snapshot.processes[0].cpuPercent);
    QVERIFY(!snapshot.processes[0].readBytesPerSecond);
    QVERIFY(QDir(dir.path() + "/42").removeRecursively());
    QVERIFY(collector.collect(3000).processes.isEmpty());
    processFixture(dir.path(), 42, 155, 200, 510, 610);
    systemFixture(dir.path(), 300, 1100);
    snapshot = collector.collect(4000);
    QVERIFY(!snapshot.processes[0].cpuPercent);
    QVERIFY(!snapshot.processes[0].writeBytesPerSecond);
}

void ProcessBackendTests::malformedAndMissingInputsAreHandled()
{
    QTemporaryDir dir;
    ProcessCollector missing(dir.path() + "/does-not-exist");
    const auto absent = missing.collect(0);
    QVERIFY(!absent.error.isEmpty());
    QVERIFY(absent.processes.isEmpty());
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 10, 100);
    processFixture(dir.path(), 43, 20, 200);
    put(dir.path() + "/43/stat", "43 (malformed) S 1 2\n");
    ProcessCollector collector(dir.path());
    QCOMPARE(collector.collect(0).processes.size(), 1);
    put(dir.path() + "/stat", "cpu broken 0 0 100 0 0 0 0\n");
    put(dir.path() + "/42/io", "read_bytes: -1\nwrite_bytes: garbage\n");
    auto snapshot = collector.collect(1000);
    QVERIFY(!snapshot.error.isEmpty());
    QCOMPARE(snapshot.processes.size(), 1);
    QVERIFY(!snapshot.processes[0].cpuPercent);
    QVERIFY(!snapshot.processes[0].readBytesPerSecond);
    systemFixture(dir.path(), 200, 1000);
    processFixture(dir.path(), 42, 20, 100);
    QVERIFY(!collector.collect(2000).processes[0].cpuPercent);
    put(dir.path() + "/42/stat", statLine(44, 10, 100)); // PID in the file must match its directory.
    QVERIFY(collector.collect(3000).processes.isEmpty());
}

void ProcessBackendTests::namesWithParenthesesAndMissingStatusAreSafe()
{
    QTemporaryDir dir;
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 10, 100);
    put(dir.path() + "/42/stat", statLine(42, 10, 100, 256, "a)b (c)\nd"));
    ProcessCollector collector(dir.path());
    auto process = collector.collect(1000).processes[0];
    QCOMPARE(process.name, QStringLiteral("a)b (c)\nd"));
    QCOMPARE(process.identity.startTimeTicks, quint64(100));
    QCOMPARE(process.canTerminate, currentUid() != 0);
    QVERIFY(QFile::remove(dir.path() + "/42/status"));
    process = collector.collect(2000).processes[0];
    QCOMPARE(process.user, QStringLiteral("未知"));
    QCOMPARE(process.identity.uid, std::numeric_limits<quint32>::max());
    QVERIFY(!process.canTerminate);
}

void ProcessBackendTests::counterResetsAndNonIncreasingClockDiscardRates()
{
    QTemporaryDir dir;
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 100, 100, 300, 500);
    ProcessCollector collector(dir.path());
    collector.collect(1000);
    systemFixture(dir.path(), 10, 90);
    processFixture(dir.path(), 42, 10, 100, 30, 50);
    auto process = collector.collect(2000).processes[0];
    QVERIFY(!process.cpuPercent);
    QVERIFY(!process.readBytesPerSecond);
    systemFixture(dir.path(), 20, 180);
    processFixture(dir.path(), 42, 20, 100, 40, 60);
    process = collector.collect(2000).processes[0];
    QVERIFY(!process.cpuPercent);
    QVERIFY(!process.writeBytesPerSecond);
    systemFixture(dir.path(), 30, 270);
    processFixture(dir.path(), 42, 30, 100, 50, 70);
    process = collector.collect(3000).processes[0];
    QCOMPARE(process.cpuPercent.value_or(-1), 10.0);
    QCOMPARE(process.writeBytesPerSecond.value_or(-1), 10.0);
}

void ProcessBackendTests::missingIoRequiresANewBaseline()
{
    QTemporaryDir dir;
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 10, 100, 100, 100);
    ProcessCollector collector(dir.path());
    collector.collect(1000);
    QVERIFY(QFile::remove(dir.path() + "/42/io"));
    QVERIFY(!collector.collect(2000).processes[0].readBytesPerSecond);
    put(dir.path() + "/42/io", "read_bytes: 500\nwrite_bytes: 600\n");
    QVERIFY(!collector.collect(3000).processes[0].readBytesPerSecond);
    put(dir.path() + "/42/io", "read_bytes: 600\nwrite_bytes: 800\n");
    const auto process = collector.collect(4000).processes[0];
    QCOMPARE(process.readBytesPerSecond.value_or(-1), 100.0);
    QCOMPARE(process.writeBytesPerSecond.value_or(-1), 200.0);
}

void ProcessBackendTests::foreignUidAndZombieAreReadOnly()
{
    QTemporaryDir dir;
    systemFixture(dir.path(), 100, 900);
    processFixture(dir.path(), 42, 10, 100);
    const auto own = QByteArray::number(currentUid());
    const auto foreign = QByteArray::number(currentUid() + 1);
    put(dir.path() + "/42/status", "Uid:\t" + own + '\t' + foreign + '\t' + own + '\t' + own + '\n');
    ProcessCollector collector(dir.path());
    QVERIFY(!collector.collect(0).processes[0].canTerminate);
    put(dir.path() + "/42/status", "Uid:\t0\t0\t0\t0\n");
    QVERIFY(!collector.collect(500).processes[0].canTerminate);
    processFixture(dir.path(), 42, 10, 100);
    put(dir.path() + "/42/stat", statLine(42, 10, 100, 0, "fixture", "Z"));
    QVERIFY(!collector.collect(1000).processes[0].canTerminate);
}

void ProcessBackendTests::controlRejectsProtectedAndStaleIdentities()
{
    QVERIFY(!ProcessControl::sendSignal({1, 1, currentUid()}).isEmpty());
    QVERIFY(!ProcessControl::sendSignal({0, 1, currentUid()}).isEmpty());
    QVERIFY(!ProcessControl::sendSignal({42, 1, 0}).isEmpty());
#ifdef Q_OS_LINUX
    QVERIFY(!ProcessControl::sendSignal({qint64(::getpid()), 1, currentUid()}).isEmpty());
#endif
    if (currentUid() == 0) QSKIP("Process actions deliberately remain disabled when running as root");
    const auto sleepPath = QStandardPaths::findExecutable("sleep");
    QVERIFY(!sleepPath.isEmpty());
    QProcess child;
    child.start(sleepPath, {"30"});
    QVERIFY(child.waitForStarted());
    const auto process = ProcessCollector::inspect(QStringLiteral("/proc/%1").arg(child.processId()), child.processId());
    QVERIFY(process);
    auto stale = process->identity;
    ++stale.startTimeTicks;
    QVERIFY(ProcessControl::sendSignal(stale).contains(QStringLiteral("身份")));
    auto otherOwner = process->identity;
    ++otherOwner.uid;
    QVERIFY(ProcessControl::sendSignal(otherOwner).contains(QStringLiteral("用户")));
    QVERIFY(!child.waitForFinished(30));
    QCOMPARE(child.state(), QProcess::Running);
    // The test owns this QProcess. Cleanup never targets any preexisting process.
    child.kill();
    QVERIFY(child.waitForFinished());
}

void ProcessBackendTests::controlTerminatesOnlyOwnedTestChildren()
{
    if (currentUid() == 0) QSKIP("Process actions deliberately remain disabled when running as root");
    const auto sleepPath = QStandardPaths::findExecutable("sleep");
    QVERIFY(!sleepPath.isEmpty());
    for (const bool force : {false, true}) {
        QProcess child;
        child.start(sleepPath, {"30"});
        QVERIFY(child.waitForStarted());
        const auto process = ProcessCollector::inspect(QStringLiteral("/proc/%1").arg(child.processId()), child.processId());
        QVERIFY(process);
        QVERIFY(process->canTerminate);
        const auto result = ProcessControl::sendSignal(process->identity, force);
        if (result.contains(QStringLiteral("不支持"))) {
            child.kill();
            child.waitForFinished();
            QSKIP("Kernel has no pidfd support; process termination safely refuses to fall back");
        }
        QVERIFY2(result.isEmpty(), qPrintable(result));
        QVERIFY(child.waitForFinished());
        QVERIFY(!ProcessControl::sendSignal(process->identity, force).isEmpty());
    }
}

QTEST_GUILESS_MAIN(ProcessBackendTests)
#include "process_backend_tests.moc"

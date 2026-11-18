#include "backend/process_service.h"
#include <QByteArrayList>
#include <QDir>
#include <QElapsedTimer>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace monitor;

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size()
        && file.commit();
}

class ProcFixture final {
public:
    bool initialize()
    {
        if (!directory.isValid() || !QDir().mkpath(directory.path() + "/321"))
            return false;
        return writeFile(directory.path() + "/meminfo", "MemTotal: 1000000 kB\n")
            && writeFile(directory.path() + "/321/status", "Uid:\t1000\t1000\t1000\t1000\n")
            && writeFile(directory.path() + "/321/cmdline", "service-test\0")
            && update(1000, 100, 1000);
    }

    bool update(quint64 total, quint64 processTicks, quint64 ioBytes)
    {
        QList<QByteArray> fields;
        for (int i = 0; i < 22; ++i)
            fields.append("0");
        fields[0] = "S";
        fields[1] = "1";
        fields[11] = QByteArray::number(processTicks);
        fields[17] = "1";
        fields[19] = "100";
        fields[21] = "10";
        return writeFile(directory.path() + "/stat", "cpu " + QByteArray::number(total)
                         + " 0 0 0 0 0 0 0\n")
            && writeFile(directory.path() + "/321/stat", "321 (service-test) "
                         + fields.join(' ') + "\n")
            && writeFile(directory.path() + "/321/io", "read_bytes: "
                         + QByteArray::number(ioBytes) + "\nwrite_bytes: "
                         + QByteArray::number(ioBytes) + "\n");
    }

    QTemporaryDir directory;
};

ProcessSnapshot snapshotAt(const QSignalSpy &spy, int index)
{
    return qvariant_cast<ProcessSnapshot>(spy.at(index).at(0));
}

} // namespace

class ProcessServiceTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void inactiveServiceDoesNotPublishOrRefresh();
    void pauseAllowsExactlyOneRequestedRefresh();
    void queuedSamplesAreDiscardedAfterStateChanges();
    void rateBaselinesResetAfterPauseAndHide();
    void invalidIntervalCannotCauseFastPolling();
    void shutdownWakesInactiveAndPausedWorkers();
};

void ProcessServiceTests::initTestCase()
{
#ifndef Q_OS_LINUX
    QSKIP("Process collection requires Linux /proc semantics");
#endif
}

void ProcessServiceTests::inactiveServiceDoesNotPublishOrRefresh()
{
    ProcFixture fixture;
    QVERIFY(fixture.initialize());
    ProcessService service(fixture.directory.path());
    QSignalSpy snapshots(&service, &ProcessService::snapshotReady);
    service.setInterval(500);
    service.start();
    service.requestRefresh();
    QTest::qWait(150);
    QCOMPARE(snapshots.size(), 0);

    service.setActive(true);
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    QCOMPARE(snapshotAt(snapshots, 0).processes.size(), 1);
    service.setActive(false);
    snapshots.clear();
    service.requestRefresh();
    QTest::qWait(650);
    QCOMPARE(snapshots.size(), 0);
}

void ProcessServiceTests::pauseAllowsExactlyOneRequestedRefresh()
{
    ProcFixture fixture;
    QVERIFY(fixture.initialize());
    ProcessService service(fixture.directory.path());
    QSignalSpy snapshots(&service, &ProcessService::snapshotReady);
    service.setInterval(5000);
    service.setActive(true);
    service.start();
    service.start(); // Repeated start must not create an additional worker.
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    service.setPaused(true);
    snapshots.clear();
    QTest::qWait(100);
    QCOMPARE(snapshots.size(), 0);

    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    QTest::qWait(100);
    QCOMPARE(snapshots.size(), 1);
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 2, 1500);
    service.setPaused(false);
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 3, 1500);
}

void ProcessServiceTests::queuedSamplesAreDiscardedAfterStateChanges()
{
    ProcFixture fixture;
    QVERIFY(fixture.initialize());
    ProcessService service(fixture.directory.path());
    QSignalSpy snapshots(&service, &ProcessService::snapshotReady);
    service.setActive(true);
    service.start();
    // Let the worker finish while this thread deliberately does not dispatch
    // its queued signal. Pausing must invalidate the already queued result.
    QTest::qSleep(150);
    service.setPaused(true);
    QTest::qWait(50);
    QCOMPARE(snapshots.size(), 0);

    service.requestRefresh();
    QTest::qSleep(150);
    service.setActive(false);
    QTest::qWait(50);
    QCOMPARE(snapshots.size(), 0);
}

void ProcessServiceTests::rateBaselinesResetAfterPauseAndHide()
{
    ProcFixture fixture;
    QVERIFY(fixture.initialize());
    ProcessService service(fixture.directory.path());
    QSignalSpy snapshots(&service, &ProcessService::snapshotReady);
    service.setInterval(5000);
    service.setActive(true);
    service.start();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    QVERIFY(!snapshotAt(snapshots, 0).processes.first().cpuPercent);

    QVERIFY(fixture.update(2000, 200, 2000));
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 2, 1500);
    const auto second = snapshotAt(snapshots, 1).processes.first();
    QVERIFY(second.cpuPercent);
    QCOMPARE(*second.cpuPercent, 10.0);
    QVERIFY(second.readBytesPerSecond);

    service.setPaused(true);
    QVERIFY(fixture.update(4000, 600, 9000));
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 3, 1500);
    const auto paused = snapshotAt(snapshots, 2).processes.first();
    QVERIFY(!paused.cpuPercent);
    QVERIFY(!paused.readBytesPerSecond);

    QVERIFY(fixture.update(5000, 700, 10000));
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 4, 1500);
    QVERIFY(snapshotAt(snapshots, 3).processes.first().cpuPercent);
    service.setPaused(false);
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 5, 1500);
    QVERIFY(!snapshotAt(snapshots, 4).processes.first().cpuPercent);
    service.setActive(false);
    QVERIFY(fixture.update(9000, 1000, 20000));
    service.setActive(true);
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 6, 1500);
    const auto restored = snapshotAt(snapshots, 5).processes.first();
    QVERIFY(!restored.cpuPercent);
    QVERIFY(!restored.readBytesPerSecond);
}

void ProcessServiceTests::invalidIntervalCannotCauseFastPolling()
{
    ProcFixture fixture;
    QVERIFY(fixture.initialize());
    ProcessService service(fixture.directory.path());
    QSignalSpy snapshots(&service, &ProcessService::snapshotReady);
    service.setInterval(5000);
    service.setInterval(0);
    service.setInterval(-1);
    service.setInterval(501);
    service.setActive(true);
    service.start();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    QTest::qWait(650);
    QCOMPARE(snapshots.size(), 1);
    service.setInterval(500);
    QTRY_VERIFY_WITH_TIMEOUT(snapshots.size() >= 2, 1500);
}

void ProcessServiceTests::shutdownWakesInactiveAndPausedWorkers()
{
    ProcFixture fixture;
    QVERIFY(fixture.initialize());
    QElapsedTimer elapsed;
    elapsed.start();
    {
        ProcessService service(fixture.directory.path());
        service.start();
    }
    QVERIFY(elapsed.elapsed() < 1500);
    {
        ProcessService service(fixture.directory.path());
        QSignalSpy snapshots(&service, &ProcessService::snapshotReady);
        service.setActive(true);
        service.setInterval(5000);
        service.start();
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
        service.setPaused(true);
        elapsed.restart();
    }
    QVERIFY(elapsed.elapsed() < 1500);
}

QTEST_GUILESS_MAIN(ProcessServiceTests)
#include "process_service_tests.moc"

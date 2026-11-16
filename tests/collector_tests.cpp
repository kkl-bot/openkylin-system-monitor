#include "backend/cpu_collector.h"
#include "backend/memory_collector.h"
#include "backend/gpu_collector.h"
#include "backend/hwmon_reader.h"
#include "backend/monitor_service.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace monitor;
class CollectorTests final : public QObject {
    Q_OBJECT
private slots:
    void cpuUsesDeltasAndDoesNotDoubleCountGuests();
    void cpuHandlesResetAndMissingData();
    void memoryUsesAvailableNotFree();
    void memoryFallbackAndInvalidInput();
    void temperaturesRespectDeviceKinds();
    void temperatureFaultAndThermalFallback();
    void nvidiaParsesMultipleDevicesAndUnavailableValues();
    void drmReadsAmdAndIgnoresConnectors();
    void noGpuIsMissingNotZero();
    void nvidiaTimeoutIsBounded();
    void servicePauseResumeAndShutdown();
};

namespace {
void put(const QString &path, const QByteArray &data)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), qint64(data.size()));
}
SourcePaths pathsFor(QTemporaryDir &dir)
{
    return {dir.path() + "/proc", dir.path() + "/sys", dir.path() + "/no-nvidia-smi"};
}
} // namespace

void CollectorTests::cpuUsesDeltasAndDoesNotDoubleCountGuests()
{
    QTemporaryDir dir; const auto paths = pathsFor(dir);
    put(paths.procRoot + "/stat", "cpu 100 0 50 800 50 0 0 0 20 0\n");
    CpuCollector collector(paths); QVERIFY(!collector.collect()[0].utilization.value);
    put(paths.procRoot + "/stat", "cpu 150 0 80 820 50 0 0 0 50 0\n");
    const auto sample = collector.collect()[0]; QVERIFY(sample.utilization.value);
    QCOMPARE(*sample.utilization.value, 80.0);
}
void CollectorTests::cpuHandlesResetAndMissingData()
{
    QTemporaryDir dir; const auto paths = pathsFor(dir); CpuCollector collector(paths);
    put(paths.procRoot + "/stat", "cpu 100 0 0 100 0 0 0 0\n"); collector.collect();
    put(paths.procRoot + "/stat", "cpu 10 0 0 10 0 0 0 0\n"); QVERIFY(!collector.collect()[0].utilization.value);
    put(paths.procRoot + "/stat", "cpu 30 0 0 30 0 0 0 0\n"); QCOMPARE(*collector.collect()[0].utilization.value, 50.0);
    put(paths.procRoot + "/stat", "cpu broken 0 0 30\n"); QVERIFY(!collector.collect()[0].utilization.value);
    put(paths.procRoot + "/stat", "cpu 50 0 0 50 0 0 0 0\n"); QVERIFY(!collector.collect()[0].utilization.value);
}
void CollectorTests::memoryUsesAvailableNotFree()
{
    QTemporaryDir dir; const auto paths = pathsFor(dir);
    put(paths.procRoot + "/meminfo", "MemTotal: 10000 kB\nMemFree: 100 kB\nMemAvailable: 7500 kB\n");
    MemoryCollector collector(paths); const auto sample = collector.collect()[0];
    QCOMPARE(*sample.utilization.value, 25.0); QCOMPARE(*sample.usedBytes, quint64(2500 * 1024));
    QVERIFY(!sample.temperature.value);
}
void CollectorTests::memoryFallbackAndInvalidInput()
{
    QTemporaryDir dir; const auto paths = pathsFor(dir); MemoryCollector collector(paths);
    put(paths.procRoot + "/meminfo", "MemTotal: 10000 kB\nMemFree: 1000 kB\nBuffers: 1000 kB\nCached: 4000 kB\nSReclaimable: 1000 kB\nShmem: 2000 kB\n");
    QCOMPARE(*collector.collect()[0].utilization.value, 50.0);
    put(paths.procRoot + "/meminfo", "MemTotal: 100 kB\nMemAvailable: 200 kB\n");
    QCOMPARE(*collector.collect()[0].utilization.value, 0.0);
    put(paths.procRoot + "/meminfo", "MemTotal: 0 kB\n"); QVERIFY(!collector.collect()[0].utilization.value);
    put(paths.procRoot + "/meminfo", "MemTotal: 100 kB\nMemAvailable: -1 kB\n"); QVERIFY(!collector.collect()[0].utilization.value);
}
void CollectorTests::temperaturesRespectDeviceKinds()
{
    QTemporaryDir dir; const auto paths = pathsFor(dir); const auto root = paths.sysRoot + "/class/hwmon/";
    put(root + "hwmon0/name", "nvme"); put(root + "hwmon0/temp1_input", "90000");
    put(root + "hwmon1/name", "coretemp"); put(root + "hwmon1/temp1_input", "51000"); put(root + "hwmon1/temp2_input", "62000");
    put(root + "hwmon2/name", "jc42"); put(root + "hwmon2/temp1_input", "42000");
    const HwmonReader reader(paths);
    QCOMPARE(*reader.temperature(ResourceKind::Cpu).value, 62.0);
    QCOMPARE(*reader.temperature(ResourceKind::Memory).value, 42.0);
}
void CollectorTests::temperatureFaultAndThermalFallback()
{
    QTemporaryDir dir; const auto paths = pathsFor(dir); const auto root = paths.sysRoot + "/class/hwmon/hwmon0/";
    put(root + "name", "coretemp"); put(root + "temp1_input", "80000"); put(root + "temp1_fault", "1");
    put(root + "temp2_input", "999000");
    put(paths.sysRoot + "/class/thermal/thermal_zone0/type", "acpitz");
    put(paths.sysRoot + "/class/thermal/thermal_zone0/temp", "72000");
    HwmonReader reader(paths); QVERIFY(!reader.temperature(ResourceKind::Cpu).value);
    put(paths.sysRoot + "/class/thermal/thermal_zone1/type", "x86_pkg_temp");
    put(paths.sysRoot + "/class/thermal/thermal_zone1/temp", "57000");
    QCOMPARE(*reader.temperature(ResourceKind::Cpu).value, 57.0);
}
void CollectorTests::nvidiaParsesMultipleDevicesAndUnavailableValues()
{
    const auto samples = GpuCollector::parseNvidiaOutput("GPU-a, \"NVIDIA, Test\", 42, 67, 1024, 8192\nGPU-b, Test 2, [N/A], N/A, [Not Supported], 4096\nmalformed\n");
    QCOMPARE(samples.size(), 2); QCOMPARE(samples[0].name, QString("NVIDIA, Test"));
    QCOMPARE(*samples[0].utilization.value, 42.0); QCOMPARE(*samples[0].totalBytes, quint64(8192ULL * 1024 * 1024));
    QVERIFY(!samples[1].utilization.value); QVERIFY(!samples[1].temperature.value); QVERIFY(!samples[1].usedBytes);
    const auto invalid = GpuCollector::parseNvidiaOutput("GPU-a, Test, 101, nan, 9000, 4096");
    QVERIFY(!invalid[0].utilization.value); QVERIFY(!invalid[0].temperature.value); QVERIFY(!invalid[0].usedBytes);
}
void CollectorTests::drmReadsAmdAndIgnoresConnectors()
{
    QTemporaryDir dir; const auto paths = pathsFor(dir); const auto root = paths.sysRoot + "/class/drm/card0/device/";
    put(root + "vendor", "0x1002"); put(root + "device", "0x744c"); put(root + "gpu_busy_percent", "37");
    put(root + "mem_info_vram_used", "1024"); put(root + "mem_info_vram_total", "4096");
    put(root + "hwmon/hwmon0/temp1_input", "65000");
    put(paths.sysRoot + "/class/drm/card0-HDMI-A-1/status", "connected");
    GpuCollector collector(paths); const auto samples = collector.collect();
    QCOMPARE(samples.size(), 1); QCOMPARE(*samples[0].utilization.value, 37.0);
    QCOMPARE(*samples[0].temperature.value, 65.0); QCOMPARE(*samples[0].usedBytes, quint64(1024));
}
void CollectorTests::noGpuIsMissingNotZero()
{
    QTemporaryDir dir; GpuCollector collector(pathsFor(dir)); const auto sample = collector.collect()[0];
    QCOMPARE(sample.id, QString("gpu-unavailable")); QVERIFY(!sample.utilization.value); QVERIFY(!sample.temperature.value);
    QVERIFY(!sample.utilization.reason.isEmpty());
}
void CollectorTests::nvidiaTimeoutIsBounded()
{
#ifdef Q_OS_UNIX
    QTemporaryDir dir; auto paths = pathsFor(dir); paths.nvidiaSmi = dir.path() + "/fake-smi";
    put(paths.nvidiaSmi, "#!/bin/sh\nexec sleep 10\n");
    QVERIFY(QFile::setPermissions(paths.nvidiaSmi, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    GpuCollector collector(paths); QElapsedTimer timer; timer.start();
    QVERIFY(!collector.collect()[0].utilization.value); QVERIFY(timer.elapsed() < 2500);
#endif
}
void CollectorTests::servicePauseResumeAndShutdown()
{
    MonitorService service; QSignalSpy spy(&service, &MonitorService::snapshotReady);
    service.setInterval(500); service.start(); QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 2, 5000);
    service.setPaused(true); QTest::qWait(300); const auto count = spy.count();
    QTest::qWait(700); QCOMPARE(spy.count(), count);
    service.setPaused(false); QTRY_VERIFY_WITH_TIMEOUT(spy.count() > count, 3000);
}

QTEST_GUILESS_MAIN(CollectorTests)
#include "collector_tests.moc"

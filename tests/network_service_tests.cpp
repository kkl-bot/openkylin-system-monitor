// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/network_service.h"
#include <QElapsedTimer>
#include <QSemaphore>
#include <QSignalSpy>
#include <QtTest>
#include <atomic>
#include <memory>
#include <thread>

using namespace monitor;

namespace {

struct ProviderState {
    std::atomic<int> factories{0};
    std::atomic<int> samples{0};
    std::atomic<int> forcedConfigurations{0};
    std::atomic<int> proxyReads{0};
    std::atomic<bool> workerOnly{true};
    std::atomic<bool> blockNextSample{false};
    QSemaphore sampleEntered;
    QSemaphore releaseSample;
    QSemaphore sampleCompleted;
    QThread *guiThread = QThread::currentThread();
};

NetworkSamplerFactory samplerFactory(const std::shared_ptr<ProviderState> &state)
{
    return [state]() -> NetworkSampler {
        ++state->factories;
        if (QThread::currentThread() == state->guiThread)
            state->workerOnly = false;
        return [state, first = true](qint64, bool refreshConfiguration) mutable {
            ++state->samples;
            if (QThread::currentThread() == state->guiThread)
                state->workerOnly = false;
            if (refreshConfiguration)
                ++state->forcedConfigurations;
            if (state->blockNextSample.exchange(false)) {
                state->sampleEntered.release();
                state->releaseSample.tryAcquire(1, 2000);
            }
            NetworkSnapshot snapshot;
            snapshot.timestamp = QDateTime::currentDateTimeUtc();
            NetworkInterfaceInfo interface;
            interface.name = QStringLiteral("test0");
            interface.receivedBytes = quint64(state->samples.load()) * 1000;
            interface.transmittedBytes = quint64(state->samples.load()) * 100;
            if (!first) {
                interface.downloadBytesPerSecond = 1000;
                interface.uploadBytesPerSecond = 100;
            }
            first = false;
            snapshot.interfaces.append(interface);
            state->sampleCompleted.release();
            return snapshot;
        };
    };
}

NetworkProxyProvider proxyProvider(const std::shared_ptr<ProviderState> &state)
{
    return [state]() {
        if (QThread::currentThread() == state->guiThread)
            state->workerOnly = false;
        ProxyInfo proxy;
        proxy.mode = ProxyMode::Manual;
        proxy.source = QStringLiteral("injected test provider");
        proxy.detail = QString::number(++state->proxyReads);
        return proxy;
    };
}

NetworkSnapshot snapshotAt(const QSignalSpy &spy, int index)
{
    return qvariant_cast<NetworkSnapshot>(spy.at(index).at(0));
}

} // namespace

class NetworkServiceTests final : public QObject {
    Q_OBJECT
private slots:
    void inactiveServiceDoesNotReadProviders();
    void pauseAllowsExactlyOneRequestedRefresh();
    void queuedSamplesAreDiscardedAfterStateChanges();
    void rateBaselinesResetAfterPauseAndHide();
    void proxyCacheExpiresAndManualRefreshBypassesIt();
    void invalidIntervalsDoNotCausePolling();
    void shutdownWakesSleepingWorkersAndWaitsForInflightRead();
};

void NetworkServiceTests::inactiveServiceDoesNotReadProviders()
{
    const auto state = std::make_shared<ProviderState>();
    NetworkService service(samplerFactory(state), proxyProvider(state));
    QSignalSpy snapshots(&service, &NetworkService::snapshotReady);
    service.setInterval(5000);
    service.start();
    service.requestRefresh();
    QTest::qWait(100);
    QCOMPARE(state->factories.load(), 0);
    QCOMPARE(state->samples.load(), 0);
    QCOMPARE(state->proxyReads.load(), 0);
    service.setActive(true);
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    QVERIFY(state->workerOnly.load());
    QCOMPARE(snapshotAt(snapshots, 0).interfaces.first().name, QStringLiteral("test0"));
    service.setActive(false);
    service.requestRefresh();
    QTest::qWait(100);
    QCOMPARE(snapshots.size(), 1);
    QCOMPARE(state->samples.load(), 1);
    QCOMPARE(state->proxyReads.load(), 1);
}

void NetworkServiceTests::pauseAllowsExactlyOneRequestedRefresh()
{
    const auto state = std::make_shared<ProviderState>();
    NetworkService service(samplerFactory(state), proxyProvider(state));
    QSignalSpy snapshots(&service, &NetworkService::snapshotReady);
    service.setInterval(5000);
    service.setActive(true);
    service.start();
    service.start();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    service.setPaused(true);
    QTest::qWait(100);
    QCOMPARE(state->samples.load(), 1);
    QCOMPARE(state->proxyReads.load(), 1);
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 2, 1500);
    QCOMPARE(state->forcedConfigurations.load(), 1);
    QCOMPARE(state->proxyReads.load(), 2);
    QTest::qWait(100);
    QCOMPARE(state->samples.load(), 2);
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 3, 1500);
    QCOMPARE(state->forcedConfigurations.load(), 2);
    QCOMPARE(state->proxyReads.load(), 3);
}

void NetworkServiceTests::queuedSamplesAreDiscardedAfterStateChanges()
{
    const auto state = std::make_shared<ProviderState>();
    NetworkService service(samplerFactory(state), proxyProvider(state));
    QSignalSpy snapshots(&service, &NetworkService::snapshotReady);
    service.setInterval(5000);
    service.setActive(true);
    service.start();
    // Wait without dispatching the GUI event queue, then invalidate its pending sample.
    QVERIFY(state->sampleCompleted.tryAcquire(1, 1500));
    QTest::qSleep(30);
    service.setPaused(true);
    QTest::qWait(30);
    QCOMPARE(snapshots.size(), 0);
    service.requestRefresh();
    QVERIFY(state->sampleCompleted.tryAcquire(1, 1500));
    QTest::qSleep(30);
    service.setActive(false);
    QTest::qWait(30);
    QCOMPARE(snapshots.size(), 0);

    state->blockNextSample = true;
    service.setPaused(false);
    service.setActive(true);
    QVERIFY(state->sampleEntered.tryAcquire(1, 1500));
    const int proxyReads = state->proxyReads.load();
    service.setActive(false);
    state->releaseSample.release();
    QVERIFY(state->sampleCompleted.tryAcquire(1, 1500));
    QTest::qWait(30);
    QCOMPARE(snapshots.size(), 0);
    QCOMPARE(state->proxyReads.load(), proxyReads); // Hiding aborts the subsequent proxy query too.
}

void NetworkServiceTests::rateBaselinesResetAfterPauseAndHide()
{
    const auto state = std::make_shared<ProviderState>();
    NetworkService service(samplerFactory(state), proxyProvider(state));
    QSignalSpy snapshots(&service, &NetworkService::snapshotReady);
    service.setInterval(5000);
    service.setActive(true);
    service.start();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    QVERIFY(!snapshotAt(snapshots, 0).interfaces.first().downloadBytesPerSecond);
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 2, 1500);
    QVERIFY(snapshotAt(snapshots, 1).interfaces.first().downloadBytesPerSecond);
    QCOMPARE(state->factories.load(), 1);
    service.setPaused(true);
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 3, 1500);
    QVERIFY(!snapshotAt(snapshots, 2).interfaces.first().downloadBytesPerSecond);
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 4, 1500);
    QVERIFY(snapshotAt(snapshots, 3).interfaces.first().downloadBytesPerSecond);
    service.setPaused(false);
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 5, 1500);
    QVERIFY(!snapshotAt(snapshots, 4).interfaces.first().downloadBytesPerSecond);
    service.setActive(false);
    service.setActive(true);
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 6, 1500);
    QVERIFY(!snapshotAt(snapshots, 5).interfaces.first().downloadBytesPerSecond);
    QCOMPARE(state->factories.load(), 4);
}

void NetworkServiceTests::proxyCacheExpiresAndManualRefreshBypassesIt()
{
    const auto state = std::make_shared<ProviderState>();
    NetworkService service(samplerFactory(state), proxyProvider(state));
    QSignalSpy snapshots(&service, &NetworkService::snapshotReady);
    service.setInterval(500);
    service.setActive(true);
    service.start();
    QTRY_VERIFY_WITH_TIMEOUT(snapshots.size() >= 3, 2000);
    QCOMPARE(state->proxyReads.load(), 1);
    QCOMPARE(snapshotAt(snapshots, 2).proxy.detail, QStringLiteral("1"));
    service.requestRefresh();
    QTRY_COMPARE_WITH_TIMEOUT(state->proxyReads.load(), 2, 1500);
    QCOMPARE(state->forcedConfigurations.load(), 1);
    // Automatic traffic polling must eventually renew the five-second proxy cache.
    QTRY_VERIFY_WITH_TIMEOUT(state->proxyReads.load() >= 3, 6500);
    QVERIFY(state->workerOnly.load());
}

void NetworkServiceTests::invalidIntervalsDoNotCausePolling()
{
    const auto state = std::make_shared<ProviderState>();
    NetworkService service(samplerFactory(state), proxyProvider(state));
    QSignalSpy snapshots(&service, &NetworkService::snapshotReady);
    service.setInterval(5000);
    service.setInterval(0);
    service.setInterval(-1);
    service.setInterval(501);
    service.setActive(true);
    service.start();
    QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
    QTest::qWait(600);
    QCOMPARE(state->samples.load(), 1);
    service.setInterval(500);
    QTRY_VERIFY_WITH_TIMEOUT(snapshots.size() >= 2, 1500);
}

void NetworkServiceTests::shutdownWakesSleepingWorkersAndWaitsForInflightRead()
{
    const auto state = std::make_shared<ProviderState>();
    QElapsedTimer elapsed;
    elapsed.start();
    {
        NetworkService service(samplerFactory(state), proxyProvider(state));
        service.start();
    }
    QVERIFY(elapsed.elapsed() < 1000);
    {
        NetworkService service(samplerFactory(state), proxyProvider(state));
        QSignalSpy snapshots(&service, &NetworkService::snapshotReady);
        service.setActive(true);
        service.setInterval(5000);
        service.start();
        QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 1500);
        service.setPaused(true);
        elapsed.restart();
    }
    QVERIFY(elapsed.elapsed() < 1000);

    state->blockNextSample = true;
    auto service = std::make_unique<NetworkService>(samplerFactory(state), proxyProvider(state));
    QSignalSpy snapshots(service.get(), &NetworkService::snapshotReady);
    service->setActive(true);
    service->start();
    QVERIFY(state->sampleEntered.tryAcquire(1, 1500));
    const int proxyReads = state->proxyReads.load();
    std::thread release([state]() { QThread::msleep(100); state->releaseSample.release(); });
    elapsed.restart();
    service.reset();
    release.join();
    QVERIFY(elapsed.elapsed() < 1500);
    QTest::qWait(30);
    QCOMPARE(snapshots.size(), 0);
    QCOMPARE(state->proxyReads.load(), proxyReads);
}

QTEST_GUILESS_MAIN(NetworkServiceTests)
#include "network_service_tests.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitor_backend.h"
#include "resource_bars.h"
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QQuickWindow>
#include <QtTest>

using namespace monitor;
using namespace monitor::widget;
class WidgetBackendTests : public QObject {
    Q_OBJECT
private slots:
    void absentValuesStayUnavailable();
    void gpuSelectionSurvivesDisconnection();
    void validatesIntervalsAndPausesSampling();
    void barsRespectThemeAndMissingValues();
    void doubleClickDoesNotConsumeHostEvents();
};
void WidgetBackendTests::absentValuesStayUnavailable()
{
    MonitorBackend backend;
    Snapshot snapshot;
    ResourceSample cpu; cpu.id = "cpu"; cpu.name = "Test CPU";
    cpu.utilization = Metric::available(0, "fixture"); cpu.temperature = Metric::unavailable("no sensor");
    snapshot.resources.append(cpu); backend.acceptSnapshot(snapshot);
    const auto map = backend.resources()[0].toMap();
    QVERIFY(map.value("utilization").isValid()); QCOMPARE(map.value("utilization").toDouble(), 0.0);
    QVERIFY(!map.value("temperature").isValid()); QVERIFY(backend.tooltip().contains("no sensor"));
}
void WidgetBackendTests::gpuSelectionSurvivesDisconnection()
{
    MonitorBackend backend; Snapshot snapshot;
    for (const auto &id : {"gpu-a", "gpu-b"}) {
        ResourceSample gpu; gpu.kind = ResourceKind::Gpu; gpu.id = id; gpu.name = id;
        snapshot.resources.append(gpu);
    }
    backend.setSelectedGpu("gpu-b"); backend.acceptSnapshot(snapshot);
    QCOMPARE(backend.resources()[1].toMap().value("id").toString(), QString("gpu-b"));
    snapshot.resources.removeLast(); backend.acceptSnapshot(snapshot);
    QCOMPARE(backend.resources()[1].toMap().value("id").toString(), QString("gpu-a"));
    QCOMPARE(backend.selectedGpu(), QString("gpu-b"));
}
void WidgetBackendTests::validatesIntervalsAndPausesSampling()
{
    MonitorBackend backend; QSignalSpy spy(&backend, &MonitorBackend::resourcesChanged);
    backend.setInterval(-1); QCOMPARE(backend.interval(), 1000);
    backend.setInterval(500); backend.setActive(true);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 1, 5000);
    backend.setPaused(true); const auto count = spy.count(); QTest::qWait(1100);
    QCOMPARE(spy.count(), count);
    backend.setPaused(false); QTRY_VERIFY_WITH_TIMEOUT(spy.count() > count, 3000);
    backend.setActive(false);
}
void WidgetBackendTests::barsRespectThemeAndMissingValues()
{
    ResourceBars bars; bars.setWidth(136); bars.setHeight(136);
    QImage light(136, 136, QImage::Format_ARGB32_Premultiplied); light.fill(Qt::transparent);
    { QPainter painter(&light); bars.paint(&painter); }
    bars.setDark(true);
    QImage dark(136, 136, QImage::Format_ARGB32_Premultiplied); dark.fill(Qt::transparent);
    { QPainter painter(&dark); bars.paint(&painter); }
    QVERIFY(light != dark);
    QVariantMap zero{{"utilization", 0.0}, {"temperature", 0.0}};
    bars.setResources({zero, zero, zero});
    QImage empty(136, 136, QImage::Format_ARGB32_Premultiplied); empty.fill(Qt::transparent);
    { QPainter painter(&empty); bars.paint(&painter); }
    QVERIFY(empty != dark);
}
void WidgetBackendTests::doubleClickDoesNotConsumeHostEvents()
{
    class HostWindow : public QQuickWindow {
    public:
        int clicks = 0;
    protected:
        void mouseDoubleClickEvent(QMouseEvent *event) override { ++clicks; QQuickWindow::mouseDoubleClickEvent(event); }
    } window;
    window.resize(136, 136);
    ResourceBars bars(window.contentItem()); bars.setSize(QSizeF(136, 136)); bars.setInteractive(true);
    QSignalSpy spy(&bars, &ResourceBars::doubleClicked); window.show();
    QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
    QCOMPARE(spy.count(), 1); QCOMPARE(window.clicks, 1);
    bars.setInteractive(false);
    QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
    QCOMPARE(spy.count(), 1); QCOMPARE(window.clicks, 2);
}
QTEST_MAIN(WidgetBackendTests)
#include "widget_backend_tests.moc"

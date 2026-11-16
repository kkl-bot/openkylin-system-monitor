#include "ui/detail_window.h"
#include "ui/mini_window.h"
#include "ui/resource_tile.h"
#include "ui/theme.h"
#include <QComboBox>
#include <QSignalSpy>
#include <QtTest>

using namespace monitor;
using namespace monitor::ui;
class UiTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void historyExpiresByTimeAndRemovesDevices();
    void detailSwitchesResourceAndEmitsControls();
    void miniHasOnlyPaintedBarsAndOpensDetail();
};
namespace {
Snapshot sampleSnapshot()
{
    Snapshot snapshot; snapshot.timestamp = QDateTime::currentDateTimeUtc();
    for (const auto kind : {ResourceKind::Cpu, ResourceKind::Gpu, ResourceKind::Memory}) {
        ResourceSample sample; sample.kind = kind;
        sample.id = kind == ResourceKind::Cpu ? "cpu" : kind == ResourceKind::Gpu ? "gpu-test" : "memory";
        sample.name = "Test resource"; sample.utilization = Metric::available(35, "fixture");
        sample.temperature = Metric::unavailable(QStringLiteral("测试：无传感器"));
        snapshot.resources.append(sample);
    }
    return snapshot;
}
} // namespace
void UiTests::initTestCase() { applyTheme(*qobject_cast<QApplication *>(qApp)); }
void UiTests::historyExpiresByTimeAndRemovesDevices()
{
    MonitorModel model; auto snapshot = sampleSnapshot(); const auto start = snapshot.timestamp;
    for (int i = 0; i < 160; ++i) { snapshot.timestamp = start.addMSecs(i * 500); model.acceptSnapshot(snapshot); }
    QCOMPARE(model.history("cpu").size(), 121);
    QVERIFY(!model.history("cpu").last().temperature);
    snapshot.resources.remove(1); snapshot.timestamp = snapshot.timestamp.addSecs(1); model.acceptSnapshot(snapshot);
    QVERIFY(model.history("gpu-test").isEmpty());
}
void UiTests::detailSwitchesResourceAndEmitsControls()
{
    MonitorModel model; model.acceptSnapshot(sampleSnapshot()); DetailWindow window(&model); window.show();
    QCOMPARE(window.findChildren<ResourceTile *>().size(), 3);
    auto *gpu = window.findChildren<ResourceTile *>()[1]; QTest::mouseClick(gpu, Qt::LeftButton); QVERIFY(gpu->isChecked());
    QSignalSpy mini(&window, &DetailWindow::miniRequested); QSignalSpy pause(&window, &DetailWindow::pauseRequested);
    QSignalSpy interval(&window, &DetailWindow::intervalRequested);
    for (auto *button : window.findChildren<QPushButton *>()) {
        if (button->objectName() == "primary") QTest::mouseClick(button, Qt::LeftButton);
        if (button->text() == QStringLiteral("暂停")) QTest::mouseClick(button, Qt::LeftButton);
    }
    QCOMPARE(mini.count(), 1); QCOMPARE(pause.count(), 1);
    window.findChild<QComboBox *>()->setCurrentIndex(2); QCOMPARE(interval.count(), 1); QCOMPARE(interval.first().first().toInt(), 2000);
    QVERIFY(!window.grab().isNull());
}
void UiTests::miniHasOnlyPaintedBarsAndOpensDetail()
{
    MonitorModel model; model.acceptSnapshot(sampleSnapshot()); MiniWindow window(&model); window.show();
    QVERIFY(window.findChildren<QWidget *>().isEmpty());
    QVERIFY(window.windowFlags().testFlag(Qt::FramelessWindowHint)); QVERIFY(window.alwaysOnTop());
    QSignalSpy detail(&window, &MiniWindow::detailRequested);
    QTest::mouseDClick(&window, Qt::LeftButton); QCOMPARE(detail.count(), 1);
    QTest::keyClick(&window, Qt::Key_Escape); QCOMPARE(detail.count(), 2);
    QVERIFY(!window.grab().isNull());
}
QTEST_MAIN(UiTests)
#include "ui_tests.moc"

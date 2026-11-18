#include "ui/detail_window.h"
#include "ui/mini_window.h"
#include "ui/resource_tile.h"
#include "ui/theme.h"
#include "ui/flow_layout.h"
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
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
    void narrowWindowKeepsLongPerformanceTextReachable();
    void controlsReflowWithoutShrinkingText();
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
        if (button->objectName() == "performancePause") QTest::mouseClick(button, Qt::LeftButton);
    }
    QCOMPARE(mini.count(), 1); QCOMPARE(pause.count(), 1);
    window.findChild<QComboBox *>("performanceInterval")->setCurrentIndex(2); QCOMPARE(interval.count(), 1); QCOMPARE(interval.first().first().toInt(), 2000);
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
void UiTests::narrowWindowKeepsLongPerformanceTextReachable()
{
    MonitorModel model; auto snapshot = sampleSnapshot();
    snapshot.resources[0].name = QString(180, 'W') + QStringLiteral("处理器完整名称");
    snapshot.resources[0].details.insert(QStringLiteral("路径"), QString(200, 'X'));
    snapshot.resources[0].temperature = Metric::unavailable(QStringLiteral("没有温度传感器，") .repeated(20));
    model.acceptSnapshot(snapshot);
    DetailWindow window(&model);
    QFont larger = window.font(); larger.setPointSizeF(larger.pointSizeF() * 1.5); window.setFont(larger);
    window.resize(760, 500); window.show();
    QTest::qWait(40);
    QCOMPARE(window.size(), QSize(760, 500));
    for (auto *tile : window.findChildren<ResourceTile *>())
        QVERIFY(tile->height() >= tile->heightForWidth(tile->width()));
    auto *scroll = window.findChild<QScrollArea *>("performanceScroll"); QVERIFY(scroll);
    QVERIFY(scroll->widget()->width() <= scroll->viewport()->width());
    QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
    for (auto *label : scroll->widget()->findChildren<QLabel *>()) {
        if (!label->isVisible() || label->text().isEmpty()) continue;
        QVERIFY2(label->width() > 0, qPrintable(label->objectName()));
        QVERIFY2(label->height() >= label->heightForWidth(label->width()), qPrintable(label->objectName()));
    }
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    auto *pause = window.findChild<QPushButton *>("performancePause");
    const auto position = pause->mapTo(scroll->viewport(), QPoint(0, 0));
    QVERIFY(scroll->viewport()->rect().contains(QRect(position, pause->size())));
}
void UiTests::controlsReflowWithoutShrinkingText()
{
    QWidget host; auto *layout = new FlowLayout(&host);
    auto *first = new QPushButton(QStringLiteral("结束进程"));
    auto *second = new QPushButton(QStringLiteral("强制结束进程"));
    layout->addWidget(first); layout->addWidget(second);
    host.resize(qMax(first->sizeHint().width(), second->sizeHint().width()) + 10, 200); host.show();
    QTest::qWait(10);
    QVERIFY(second->y() >= first->geometry().bottom());
    QVERIFY(first->width() >= first->sizeHint().width());
    QVERIFY(second->width() >= second->sizeHint().width());
    host.resize(600, 200); QTest::qWait(10);
    QCOMPARE(second->y(), first->y());
}
QTEST_MAIN(UiTests)
#include "ui_tests.moc"

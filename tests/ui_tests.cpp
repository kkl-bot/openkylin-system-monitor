#include "ui/detail_window.h"
#include "ui/mini_window.h"
#include "ui/resource_tile.h"
#include "ui/theme.h"
#include "ui/flow_layout.h"
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTableView>
#include <QtTest>

using namespace monitor;
using namespace monitor::ui;
class UiTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void historyExpiresByTimeAndRemovesDevices();
    void detailSwitchesResourceAndEmitsControls();
    void resourceSelectionSurvivesResizeNavigationAndDeviceChanges();
    void miniHasOnlyPaintedBarsAndOpensDetail();
    void narrowWindowKeepsLongPerformanceTextReachable();
    void controlsReflowWithoutShrinkingText();
    void resizingKeepsOneReadableScaleAcrossPages();
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
        if (kind == ResourceKind::Memory) {
            sample.usedBytes = 1024ULL * 1024 * 1024;
            sample.totalBytes = 4 * *sample.usedBytes;
            sample.utilization = Metric::available(25, "fixture");
        }
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
    ResourceTile *gpu = nullptr;
    for (auto *tile : window.findChildren<ResourceTile *>())
        if (tile->resourceId() == "gpu-test") gpu = tile;
    QVERIFY2(gpu && gpu->isVisible(), qPrintable(QStringLiteral("window=%1 sidebar=%2 workspace=%3 fontHeight=%4 tileCount=%5")
        .arg(window.width()).arg(window.findChild<QWidget *>("sidebar")->width())
        .arg(window.findChild<QScrollArea *>("resourceSelector")->parentWidget()->width())
        .arg(window.fontMetrics().height()).arg(window.findChildren<ResourceTile *>().size())));
    QTest::mouseClick(gpu, Qt::LeftButton); QVERIFY(gpu->isChecked());
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
void UiTests::resourceSelectionSurvivesResizeNavigationAndDeviceChanges()
{
    MonitorModel model;
    auto snapshot = sampleSnapshot();
    model.acceptSnapshot(snapshot);
    DetailWindow window(&model);
    window.show();
    auto *picker = window.findChild<QComboBox *>("performanceResourcePicker");
    auto *list = window.findChild<QScrollArea *>("resourceSelector");
    auto *toggle = window.findChild<QPushButton *>("navigationToggle");
    auto *process = window.findChild<QPushButton *>("processNavigation");
    auto *performance = window.findChild<QPushButton *>("performanceNavigation");
    auto *composition = window.findChild<QWidget *>("memoryCompositionBar");
    QVERIFY(picker && list && toggle && process && performance && composition);
    QTRY_VERIFY(list->isVisible() && !picker->isVisible());
    window.resize(760, 500);
    QTRY_VERIFY(picker->isVisible() && !list->isVisible());
    picker->setCurrentIndex(picker->findData("memory"));
    QCOMPARE(window.selectedResource(), QStringLiteral("memory"));
    QVERIFY(composition->isVisible());
    QVERIFY(composition->accessibleDescription().contains("25.0%"));
    QVERIFY(composition->accessibleDescription().contains("75.0%"));
    QTest::mouseClick(toggle, Qt::LeftButton);
    QVERIFY(window.navigationCollapsed());
    QTest::mouseClick(process, Qt::LeftButton);
    QVERIFY(window.showingProcesses() && !picker->isVisible() && !list->isVisible());
    QTest::mouseClick(performance, Qt::LeftButton);
    QVERIFY(performance->isChecked());
    window.resize(1180, 780);
    QTRY_VERIFY(list->isVisible());
    QCOMPARE(window.selectedResource(), QStringLiteral("memory"));
    for (auto *tile : list->findChildren<ResourceTile *>())
        QCOMPARE(tile->isChecked(), tile->resourceId() == QStringLiteral("memory"));
    snapshot.resources.removeLast();
    model.acceptSnapshot(snapshot);
    QCOMPARE(window.selectedResource(), QStringLiteral("cpu"));
    QVERIFY(!composition->isVisible());
}
void UiTests::narrowWindowKeepsLongPerformanceTextReachable()
{
    MonitorModel model; auto snapshot = sampleSnapshot();
    snapshot.resources[0].name = QString(180, 'W') + QStringLiteral("处理器完整名称");
    snapshot.resources[0].details.insert(QStringLiteral("路径"), QString(200, 'X'));
    snapshot.resources[0].temperature = Metric::unavailable(QStringLiteral("没有温度传感器，") .repeated(20));
    model.acceptSnapshot(snapshot);
    DetailWindow window(&model);
    window.resize(760, 500); window.show();
    const QFont desktopFont = QApplication::font();
    const auto restoreFont = qScopeGuard([&] { QApplication::setFont(desktopFont); });
    QFont larger = desktopFont; larger.setPointSize(18); QApplication::setFont(larger);
    QTest::qWait(40);
    QCOMPARE(window.font().pointSizeF(), 18.0);
    QCOMPARE(window.size(), QSize(760, 500));
    for (auto *tile : window.findChildren<ResourceTile *>())
        if (tile->isVisible()) QVERIFY(tile->height() >= tile->heightForWidth(tile->width()));
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
    QVERIFY(pause->isVisible());
    QVERIFY(window.rect().contains(QRect(pause->mapTo(&window, QPoint()), pause->size())));
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
void UiTests::resizingKeepsOneReadableScaleAcrossPages()
{
    MonitorModel model;
    DetailWindow window(&model); window.resize(760, 500); window.show();
    model.acceptSnapshot(sampleSnapshot());
    auto *table = window.findChild<QTableView *>("processTable");
    auto *proxy = window.findChild<QLabel *>("networkSystemProxy");
    auto *axis = window.findChild<QWidget *>("networkHistoryChart");
    auto *card = window.findChild<QLabel *>("cardValue");
    QVERIFY(table && proxy && axis && card);
    const int small = window.fontMetrics().height();
    QFont minimumBodyFont = window.font();
    minimumBodyFont.setPointSize(12);
    const int minimum = QFontMetrics(minimumBodyFont, &window).height();
    QVERIFY(small >= minimum);
    for (const QSize size : {QSize(1180, 780), QSize(1600, 980), QSize(760, 500)}) {
        window.resize(size);
        QCoreApplication::processEvents();
        const int body = window.fontMetrics().height();
        if (size.width() > 760) QVERIFY(body > small);
        else QCOMPARE(body, small);
        window.showProcesses(); QCOMPARE(table->fontMetrics().height(), body);
        window.showNetwork(); QCOMPARE(proxy->fontMetrics().height(), body);
        QCOMPARE(axis->fontMetrics().height(), body);
        window.selectResource("cpu"); QVERIFY(card->fontMetrics().height() > body);
        for (const auto *tile : window.findChildren<ResourceTile *>())
            QCOMPARE(tile->fontMetrics().height(), body);
        for (const auto *title : window.findChildren<QLabel *>("pageTitle"))
            QVERIFY(title->fontMetrics().height() >= card->fontMetrics().height());
    }
}
QTEST_MAIN(UiTests)
#include "ui_tests.moc"

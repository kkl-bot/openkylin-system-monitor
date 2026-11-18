#include "ui/process_details_dialog.h"
#include "ui/process_filter_model.h"
#include "ui/process_page.h"
#include "ui/process_table_model.h"
#include "ui/process_table_view.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableView>
#include <QTimer>
#include <QTextDocument>
#include <QtTest>
#include <algorithm>

using namespace monitor;
using namespace monitor::ui;
namespace {
ProcessInfo process(qint64 pid, const QString &name, double cpu, quint64 bytes)
{
    ProcessInfo result;
    result.identity = {pid, static_cast<quint64>(pid * 100), 1000};
    result.name = name; result.user = QStringLiteral("openkylin"); result.state = QStringLiteral("S");
    result.cpuPercent = cpu; result.residentBytes = bytes; result.memoryPercent = 0.5; result.threads = 2;
    result.readBytesPerSecond = 12; result.writeBytesPerSecond = 30; result.canTerminate = true;
    return result;
}
ProcessSnapshot snapshot()
{
    ProcessSnapshot result; result.timestamp = QDateTime::currentDateTimeUtc();
    result.processes = {process(902001, QStringLiteral("Alpha"), 2, 900), process(902002, QStringLiteral("beta"), 10, 1024 * 1024),
        process(902003, QStringLiteral("Gamma"), 8, 1024)};
    return result;
}
QTableView *table(ProcessPage &page) { return page.findChild<QTableView *>(QStringLiteral("processTable")); }
ProcessFilterModel *filter(ProcessPage &page) { return page.findChild<ProcessFilterModel *>(); }
void selectPid(ProcessPage &page, qint64 pid)
{
    for (int row = 0; row < table(page)->model()->rowCount(); ++row)
        if (table(page)->model()->index(row, ProcessTableModel::Pid).data().toLongLong() == pid) { table(page)->selectRow(row); return; }
    QFAIL("Expected fixture process was not found");
}
qint64 selectedPid(ProcessPage &page)
{
    const auto rows = table(page)->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().sibling(rows.first().row(), ProcessTableModel::Pid).data().toLongLong();
}
QPushButton *button(ProcessPage &page, const char *name) { return page.findChild<QPushButton *>(QString::fromLatin1(name)); }
} // namespace

class ProcessUiTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void sortsNumbersAndUnavailableMetrics();
    void filtersNamePidUserAndOwner();
    void preservesIdentityAcrossRefreshAndClearsPidReuse();
    void disablesProtectedAndDisappearingProcesses();
    void controlsAndHeaderState();
    void confirmationDefaultsToCancelAndKeepsOriginalTarget();
    void rejectsReusedPidDuringConfirmation();
    void detailsAndErrorsArePlainText();
    void tableAdaptsToFontAndPreservesManualWidths();
    void narrowPageKeepsControlsReachable();
    void detailsFitScreenAndWrapLongCommands();
};
void ProcessUiTests::initTestCase() { qRegisterMetaType<ProcessIdentity>(); }
void ProcessUiTests::sortsNumbersAndUnavailableMetrics()
{
    ProcessTableModel model; ProcessFilterModel proxy; proxy.setSourceModel(&model);
    auto sample = snapshot(); sample.processes[2].cpuPercent.reset(); model.acceptSnapshot(sample);
    proxy.sort(ProcessTableModel::Cpu, Qt::DescendingOrder);
    QCOMPARE(proxy.index(0, ProcessTableModel::Pid).data().toLongLong(), 902002LL);
    QCOMPARE(proxy.index(1, ProcessTableModel::Pid).data().toLongLong(), 902001LL);
    QCOMPARE(proxy.index(2, ProcessTableModel::Cpu).data().toString(), QStringLiteral("—"));
    proxy.sort(ProcessTableModel::Cpu, Qt::AscendingOrder);
    QCOMPARE(proxy.index(0, ProcessTableModel::Pid).data().toLongLong(), 902001LL);
    QCOMPARE(proxy.index(2, ProcessTableModel::Cpu).data().toString(), QStringLiteral("—"));
    proxy.sort(ProcessTableModel::Memory, Qt::AscendingOrder);
    QCOMPARE(proxy.index(0, ProcessTableModel::Pid).data().toLongLong(), 902001LL);
    QCOMPARE(proxy.index(1, ProcessTableModel::Pid).data().toLongLong(), 902003LL);
    QCOMPARE(proxy.index(2, ProcessTableModel::Pid).data().toLongLong(), 902002LL);
}
void ProcessUiTests::filtersNamePidUserAndOwner()
{
    ProcessTableModel model; ProcessFilterModel proxy; proxy.setSourceModel(&model); proxy.setCurrentUserId(1000);
    auto sample = snapshot(); sample.processes[2].identity.uid = 1001; sample.processes[2].user = QStringLiteral("guest"); model.acceptSnapshot(sample);
    proxy.setSearchText(QStringLiteral(" ALPHA ")); QCOMPARE(proxy.rowCount(), 1);
    proxy.setSearchText(QStringLiteral("902002")); QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, ProcessTableModel::Name).data().toString(), QStringLiteral("beta"));
    proxy.setSearchText(QStringLiteral("GUEST")); QCOMPARE(proxy.rowCount(), 1);
    proxy.setOwnProcessesOnly(true); QCOMPARE(proxy.rowCount(), 0);
    proxy.setSearchText({}); QCOMPARE(proxy.rowCount(), 2);
}
void ProcessUiTests::preservesIdentityAcrossRefreshAndClearsPidReuse()
{
    ProcessPage page; auto sample = snapshot(); page.acceptSnapshot(sample); selectPid(page, 902001);
    sample.processes[0].cpuPercent = 90; std::reverse(sample.processes.begin(), sample.processes.end()); page.acceptSnapshot(sample);
    QCOMPARE(selectedPid(page), 902001LL);
    table(page)->sortByColumn(ProcessTableModel::Name, Qt::DescendingOrder); QCOMPARE(selectedPid(page), 902001LL);
    for (auto &entry : sample.processes) if (entry.identity.pid == 902001) ++entry.identity.startTimeTicks;
    page.acceptSnapshot(sample); QCOMPARE(selectedPid(page), -1LL);
    QVERIFY(!button(page, "processTerminate")->isEnabled()); QVERIFY(!button(page, "processDetails")->isEnabled());
}
void ProcessUiTests::disablesProtectedAndDisappearingProcesses()
{
    ProcessPage page; filter(page)->setCurrentUserId(1000); auto sample = snapshot();
    sample.processes[0].identity.pid = QCoreApplication::applicationPid();
    sample.processes[1].identity.uid = 0; sample.processes[2].identity.uid = 1001;
    page.acceptSnapshot(sample);
    for (const auto &entry : sample.processes) {
        selectPid(page, entry.identity.pid); QVERIFY(button(page, "processDetails")->isEnabled());
        QVERIFY(!button(page, "processTerminate")->isEnabled()); QVERIFY(!button(page, "processForceTerminate")->isEnabled());
    }
    sample = snapshot(); page.acceptSnapshot(sample); selectPid(page, 902001);
    QVERIFY(button(page, "processTerminate")->isEnabled());
    sample.error = QStringLiteral("partial read failed"); page.acceptSnapshot(sample); QVERIFY(!button(page, "processTerminate")->isEnabled());
    sample.error.clear(); sample.processes.removeAt(0); page.acceptSnapshot(sample);
    QCOMPARE(selectedPid(page), -1LL); QVERIFY(!button(page, "processDetails")->isEnabled());
}
void ProcessUiTests::controlsAndHeaderState()
{
    ProcessPage page; page.acceptSnapshot(snapshot());
    QSignalSpy paused(&page, &ProcessPage::pauseRequested), interval(&page, &ProcessPage::intervalRequested), refreshed(&page, &ProcessPage::refreshRequested);
    page.setPaused(true); page.setInterval(2000); QCOMPARE(paused.count(), 0); QCOMPARE(interval.count(), 0);
    button(page, "processPause")->click(); QCOMPARE(paused.count(), 1); QCOMPARE(paused.first().first().toBool(), false);
    page.findChild<QComboBox *>(QStringLiteral("processInterval"))->setCurrentIndex(3);
    QCOMPARE(interval.count(), 1); QCOMPARE(interval.first().first().toInt(), 5000);
    button(page, "processRefresh")->click(); QCOMPARE(refreshed.count(), 1);
    table(page)->setColumnWidth(ProcessTableModel::Name, 222); table(page)->sortByColumn(ProcessTableModel::Memory, Qt::AscendingOrder);
    const auto saved = page.headerState(); ProcessPage restored; restored.restoreHeaderState(saved); restored.acceptSnapshot(snapshot());
    QCOMPARE(table(restored)->columnWidth(ProcessTableModel::Name), 222);
    QCOMPARE(table(restored)->horizontalHeader()->sortIndicatorSection(), int(ProcessTableModel::Memory));
    QCOMPARE(table(restored)->horizontalHeader()->sortIndicatorOrder(), Qt::AscendingOrder);
    QCOMPARE(table(restored)->model()->index(0, ProcessTableModel::Pid).data().toLongLong(), 902001LL);
    page.findChild<QLineEdit *>(QStringLiteral("processSearch"))->setText(QStringLiteral("no matches"));
    QCOMPARE(table(page)->model()->rowCount(), 0);
    QVERIFY(page.findChild<QLabel *>(QStringLiteral("processSummary"))->text().contains(QStringLiteral("没有匹配项")));
}
void ProcessUiTests::confirmationDefaultsToCancelAndKeepsOriginalTarget()
{
    ProcessPage page; filter(page)->setCurrentUserId(1000); auto sample = snapshot(); page.acceptSnapshot(sample); selectPid(page, 902001);
    QSignalSpy terminated(&page, &ProcessPage::terminateRequested);
    QTimer::singleShot(0, &page, [&page]() {
        auto *confirmation = page.findChild<QMessageBox *>(); QVERIFY(confirmation);
        QCOMPARE(confirmation->textFormat(), Qt::PlainText);
        QCOMPARE(confirmation->standardButton(confirmation->defaultButton()), QMessageBox::Cancel);
        confirmation->button(QMessageBox::Cancel)->click();
    });
    button(page, "processTerminate")->click(); QCOMPARE(terminated.count(), 0);
    QTimer::singleShot(0, &page, [&page, &sample]() {
        auto *confirmation = page.findChild<QMessageBox *>(); QVERIFY(confirmation);
        sample.processes[0].cpuPercent = 95; page.acceptSnapshot(sample); selectPid(page, 902003);
        confirmation->findChild<QPushButton *>(QStringLiteral("processConfirmTermination"))->click();
    });
    button(page, "processForceTerminate")->click(); QCOMPARE(terminated.count(), 1);
    QCOMPARE(terminated.first()[0].value<ProcessIdentity>().pid, 902001LL);
    QCOMPARE(terminated.first()[1].toBool(), true);
}
void ProcessUiTests::rejectsReusedPidDuringConfirmation()
{
    ProcessPage page; filter(page)->setCurrentUserId(1000); auto sample = snapshot(); page.acceptSnapshot(sample); selectPid(page, 902001);
    QSignalSpy terminated(&page, &ProcessPage::terminateRequested);
    QTimer::singleShot(0, &page, [&page, &sample]() {
        auto *confirmation = page.findChild<QMessageBox *>(); QVERIFY(confirmation);
        ++sample.processes[0].identity.startTimeTicks; page.acceptSnapshot(sample);
        confirmation->findChild<QPushButton *>(QStringLiteral("processConfirmTermination"))->click();
    });
    button(page, "processTerminate")->click(); QCOMPARE(terminated.count(), 0);
    QVERIFY(page.findChild<QLabel *>(QStringLiteral("processActionResult"))->text().contains(QStringLiteral("未发送")));
}
void ProcessUiTests::detailsAndErrorsArePlainText()
{
    auto sample = snapshot(); sample.processes[0].name = QStringLiteral("<b>literal %2</b>");
    sample.processes[0].commandLine = QStringLiteral("app --name '<img src=x>'");
    ProcessDetailsDialog dialog(sample.processes[0], sample.timestamp);
    const auto *text = dialog.findChild<QPlainTextEdit *>(); QVERIFY(text); QVERIFY(text->isReadOnly());
    QVERIFY(text->toPlainText().contains(sample.processes[0].name)); QVERIFY(text->toPlainText().contains(sample.processes[0].commandLine));
    QVERIFY(text->toPlainText().contains(QStringLiteral("状态：睡眠")));
    ProcessTableModel model; model.acceptSnapshot(sample);
    QCOMPARE(model.index(0, ProcessTableModel::State).data().toString(), QStringLiteral("睡眠"));
    QCOMPARE(model.index(0, ProcessTableModel::State).data(ProcessTableModel::SortRole).toString(), QStringLiteral("S"));
    const QString tooltip = model.index(0, ProcessTableModel::Name).data(Qt::ToolTipRole).toString();
    QVERIFY(tooltip.contains(QStringLiteral("&lt;b&gt;"))); QVERIFY(!tooltip.contains(QStringLiteral("<b>literal")));
    QTextDocument tooltipDocument; tooltipDocument.setHtml(tooltip);
    QCOMPARE(tooltipDocument.toPlainText(), sample.processes[0].name);
    ProcessPage page; sample.error = QStringLiteral("<b>not markup</b>"); page.acceptSnapshot(sample);
    auto *status = page.findChild<QLabel *>(QStringLiteral("processStatus")); QCOMPARE(status->textFormat(), Qt::PlainText);
    QVERIFY(status->text().contains(sample.error)); page.showActionResult(sample.error);
    QCOMPARE(page.findChild<QLabel *>(QStringLiteral("processActionResult"))->textFormat(), Qt::PlainText);
}
void ProcessUiTests::tableAdaptsToFontAndPreservesManualWidths()
{
    ProcessPage page; page.resize(920, 720); page.acceptSnapshot(snapshot()); page.show();
    auto *view = table(page); QTest::qWait(20);
    const int oldRowHeight = view->rowHeight(0), oldHeaderHeight = view->horizontalHeader()->height();
    const int oldCpuWidth = view->columnWidth(ProcessTableModel::Cpu);
    view->setColumnWidth(ProcessTableModel::Name, 241);
    auto larger = page.font(); larger.setPointSizeF(qMax(12.0, larger.pointSizeF()) * 1.5); page.setFont(larger);
    QTRY_VERIFY(view->rowHeight(0) > oldRowHeight);
    QTRY_VERIFY(view->horizontalHeader()->height() > oldHeaderHeight);
    QTRY_VERIFY(view->columnWidth(ProcessTableModel::Cpu) > oldCpuWidth);
    QVERIFY(view->columnWidth(ProcessTableModel::Cpu) >= view->horizontalHeader()->sectionSizeHint(ProcessTableModel::Cpu));
    QCOMPARE(view->columnWidth(ProcessTableModel::Name), 241);
    page.resize(520, 520);
    QTRY_VERIFY(view->horizontalScrollBar()->maximum() > 0);
    QCOMPARE(view->columnWidth(ProcessTableModel::Name), 241);
    QVERIFY(view->columnWidth(ProcessTableModel::Cpu) >= view->fontMetrics().horizontalAdvance(QStringLiteral("100.0 %")) + 8);
    const QByteArray saved = page.headerState();
    ProcessPage restored; restored.restoreHeaderState(saved); restored.acceptSnapshot(snapshot());
    restored.resize(520, 520); restored.show();
    auto *restoredView = table(restored); const int beforeGrowth = restoredView->columnWidth(ProcessTableModel::Cpu);
    restored.setFont(larger);
    QTRY_VERIFY(restoredView->columnWidth(ProcessTableModel::Cpu) > beforeGrowth);
    QCOMPARE(restoredView->columnWidth(ProcessTableModel::Name), 241);
    // Existing users' raw Qt header settings remain usable after the format gains per-column intent.
    ProcessPage legacy; legacy.restoreHeaderState(view->horizontalHeader()->saveState());
    QCOMPARE(table(legacy)->columnWidth(ProcessTableModel::Name), 241);
}
void ProcessUiTests::narrowPageKeepsControlsReachable()
{
    ProcessPage page; auto font = page.font(); font.setPointSize(20); page.setFont(font);
    page.resize(520, 500); page.acceptSnapshot(snapshot()); page.show(); QTest::qWait(20);
    auto *scroll = page.findChild<QScrollArea *>(QStringLiteral("processContentScroll")); QVERIFY(scroll);
    QCOMPARE(page.width(), 520);
    const QList<QWidget *> controls = {button(page, "processRefresh"), button(page, "processDetails"),
        button(page, "processTerminate"), button(page, "processForceTerminate"), button(page, "processPause"),
        page.findChild<QComboBox *>(QStringLiteral("processInterval"))};
    for (int i = 0; i < controls.size(); ++i) {
        auto *control = controls[i]; QVERIFY(control);
        QVERIFY2(control->width() >= control->minimumSizeHint().width(), qPrintable(control->objectName()));
        const QRect area(control->mapTo(scroll->widget(), QPoint()), control->size());
        QVERIFY(area.left() >= 0); QVERIFY(area.right() < scroll->widget()->width());
        for (int j = 0; j < i; ++j) {
            const QRect other(controls[j]->mapTo(scroll->widget(), QPoint()), controls[j]->size());
            QVERIFY(!area.intersects(other));
        }
    }
    scroll->ensureWidgetVisible(button(page, "processPause"));
    QTRY_VERIFY(scroll->viewport()->rect().contains(button(page, "processPause")->mapTo(scroll->viewport(), button(page, "processPause")->rect().center())));
}
void ProcessUiTests::detailsFitScreenAndWrapLongCommands()
{
    auto sample = snapshot(); sample.processes[0].commandLine = QString(4000, QLatin1Char('X'));
    ProcessDetailsDialog dialog(sample.processes[0], sample.timestamp);
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    dialog.resize(available.size() + QSize(500, 300)); dialog.show(); QTest::qWait(20);
    QVERIFY(dialog.width() <= available.width()); QVERIFY(dialog.height() <= available.height());
    auto *text = dialog.findChild<QPlainTextEdit *>(); QVERIFY(text);
    QCOMPARE(text->lineWrapMode(), QPlainTextEdit::WidgetWidth);
    QCOMPARE(text->wordWrapMode(), QTextOption::WrapAtWordBoundaryOrAnywhere);
    QVERIFY(text->toPlainText().contains(sample.processes[0].commandLine));
    QCOMPARE(text->horizontalScrollBar()->maximum(), 0);
}
QTEST_MAIN(ProcessUiTests)
#include "process_ui_tests.moc"

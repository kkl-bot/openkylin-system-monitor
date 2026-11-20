// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/application_controller.h"
#include "ui/process_table_model.h"
#include "ui/resource_tile.h"
#include "ui/theme.h"
#include <QAbstractButton>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

using namespace monitor;
using namespace monitor::ui;

class TaskManagerWorkflowTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void findInspectAndTerminateOnlyOurChild();
private:
    QTemporaryDir settingsDirectory_;
};

void TaskManagerWorkflowTests::initTestCase()
{
    QVERIFY(settingsDirectory_.isValid());
    QCoreApplication::setOrganizationName("KylinMonitorWorkflowTests");
    QCoreApplication::setApplicationName("isolated-task-manager");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory_.path());
    QApplication::setQuitOnLastWindowClosed(false);
    applyTheme(*qobject_cast<QApplication *>(qApp));
}

void TaskManagerWorkflowTests::findInspectAndTerminateOnlyOurChild()
{
#ifndef Q_OS_LINUX
    QSKIP("The workflow requires the Linux /proc backend");
#else
    if (::geteuid() == 0) QSKIP("Root processes are deliberately read-only; run this workflow as an ordinary user");
    QProcess child;
    child.start("/bin/sleep", {"60"});
    QVERIFY(child.waitForStarted());
    const qint64 childPid = child.processId();

    ApplicationController controller;
    controller.start(false, true);
    controller.showProcesses();
    DetailWindow *window = nullptr;
    for (auto *widget : QApplication::topLevelWidgets()) {
        if (auto *detail = qobject_cast<DetailWindow *>(widget)) window = detail;
    }
    QVERIFY(window);
    QVERIFY(window->showingProcesses());
    auto *table = window->findChild<QTableView *>("processTable");
    auto *search = window->findChild<QLineEdit *>("processSearch");
    auto *terminate = window->findChild<QPushButton *>("processTerminate");
    auto *details = window->findChild<QPushButton *>("processDetails");
    QVERIFY(table && search && terminate && details);
    search->setText(QString::number(childPid));
    QTRY_VERIFY_WITH_TIMEOUT(table->model()->rowCount() > 0, 5000);
    int childRow = -1;
    for (int row = 0; row < table->model()->rowCount(); ++row) {
        if (table->model()->index(row, ProcessTableModel::Pid).data().toLongLong() == childPid) childRow = row;
    }
    QVERIFY(childRow >= 0);
    table->selectRow(childRow);
    QVERIFY(details->isEnabled());
    // These checks exercise the real controller, worker, UI, confirmation and
    // pidfd signal path, exclusively against the subprocess created above.
    QVERIFY(terminate->isEnabled());
    bool confirmed = false;
    QTimer::singleShot(0, window, [&]() {
        auto *box = window->findChild<QMessageBox *>("processTerminationConfirmation");
        if (!box) return;
        auto *button = box->findChild<QPushButton *>("processConfirmTermination");
        if (box->defaultButton() != box->button(QMessageBox::Cancel) || !button) {
            box->reject();
            return;
        }
        confirmed = true;
        button->click();
    });
    QTest::mouseClick(terminate, Qt::LeftButton);
    QVERIFY(confirmed);
    QTRY_COMPARE_WITH_TIMEOUT(child.state(), QProcess::NotRunning, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!terminate->isEnabled(), 5000);
    auto *result = window->findChild<QLabel *>("processActionResult");
    QVERIFY(result && !result->text().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!window->findChildren<ResourceTile *>().isEmpty(), 5000);
    auto *performance = window->findChild<QPushButton *>("performanceNavigation");
    QVERIFY(performance && performance->isVisible());
    QTest::mouseClick(performance, Qt::LeftButton);
    QVERIFY(!window->showingProcesses());
    auto *resource = window->findChildren<ResourceTile *>().first();
    QVERIFY(resource->isVisible());
    QTest::mouseClick(resource, Qt::LeftButton);
    QCOMPARE(window->selectedResource(), resource->resourceId());
    controller.showMini();
    QVERIFY(!window->isVisible());
    controller.showProcesses();
    QVERIFY(window->isVisible() && window->showingProcesses());
    QSignalSpy active(window, &DetailWindow::processPageActiveChanged);
    window->showMinimized();
    QVERIFY(!active.isEmpty());
    QCOMPARE(active.last().first().toBool(), false);
    controller.showProcesses();
    QVERIFY(!window->isMinimized());
    QCOMPARE(active.last().first().toBool(), true);
#endif
}

QTEST_MAIN(TaskManagerWorkflowTests)
#include "task_manager_workflow_tests.moc"

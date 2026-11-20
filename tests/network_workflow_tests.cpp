// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/application_controller.h"
#include "backend/network_collector.h"
#include "ui/network_page.h"
#include "ui/theme.h"
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUdpSocket>
#include <QtTest>

using namespace monitor;
using namespace monitor::ui;
class NetworkWorkflowTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void actualLocalTrafficHasReceiveAndTransmitRates();
    void networkPageShowsRealInterfaceAndDetails();
    void selectedInterfaceAndPageSurviveRestart();
private:
    QTemporaryDir settingsDirectory_;
};
void NetworkWorkflowTests::initTestCase()
{
    QVERIFY(settingsDirectory_.isValid());
    QCoreApplication::setOrganizationName("KylinMonitorNetworkWorkflowTests");
    QCoreApplication::setApplicationName("isolated-network-workflow");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory_.path());
    QApplication::setQuitOnLastWindowClosed(false);
    applyTheme(*qobject_cast<QApplication *>(qApp));
}
void NetworkWorkflowTests::actualLocalTrafficHasReceiveAndTransmitRates()
{
#ifndef Q_OS_LINUX
    QSKIP("Linux interface statistics are required");
#else
    NetworkSourcePaths paths; paths.useNetworkManager = false;
    NetworkCollector collector(paths);
    QElapsedTimer elapsed; elapsed.start();
    const auto before = collector.collect(elapsed.elapsed());
    QString name;
    for (const auto &interface : before.interfaces)
        if (interface.loopback && interface.up && interface.running) name = interface.name;
    QVERIFY(!name.isEmpty());
    QUdpSocket receiver, sender;
    QVERIFY(receiver.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)));
    const QByteArray payload(2048, 'n');
    for (int i = 0; i < 8; ++i) {
        QCOMPARE(sender.writeDatagram(payload, QHostAddress::LocalHost, receiver.localPort()), qint64(payload.size()));
        if (!receiver.hasPendingDatagrams()) QVERIFY(receiver.waitForReadyRead(1000));
        QByteArray data(4096, '\0');
        QCOMPARE(receiver.readDatagram(data.data(), data.size()), qint64(payload.size()));
    }
    QTest::qWait(40);
    const auto after = collector.collect(elapsed.elapsed());
    bool found = false;
    for (const auto &interface : after.interfaces) {
        if (interface.name != name) continue;
        found = true;
        QVERIFY(interface.downloadBytesPerSecond && *interface.downloadBytesPerSecond > 0);
        QVERIFY(interface.uploadBytesPerSecond && *interface.uploadBytesPerSecond > 0);
    }
    QVERIFY(found);
#endif
}
void NetworkWorkflowTests::networkPageShowsRealInterfaceAndDetails()
{
    ApplicationController controller; controller.start(false, true); controller.showNetwork();
    DetailWindow *window = nullptr;
    for (auto *widget : QApplication::topLevelWidgets())
        if (auto *detail = qobject_cast<DetailWindow *>(widget)) window = detail;
    QVERIFY(window && window->showingNetwork());
    auto *page = window->networkPage();
    QTRY_VERIFY_WITH_TIMEOUT(!page->selectedInterface().isEmpty(), 10000);
    const QString selected = page->selectedInterface();
    QPushButton *details = nullptr;
    for (auto *button : page->findChildren<QPushButton *>())
        if (button->text() == QStringLiteral("详细信息")) details = button;
    QVERIFY(details && details->isEnabled());
    bool complete = false;
    QTimer::singleShot(0, page, [&]() {
        auto *dialog = page->findChild<QDialog *>();
        if (!dialog) return;
        auto *text = dialog->findChild<QPlainTextEdit *>();
        complete = text && text->isReadOnly() && text->toPlainText().contains(selected)
            && text->toPlainText().contains(QStringLiteral("DNS"))
            && text->toPlainText().contains(QStringLiteral("网关"));
        dialog->reject();
    });
    details->click(); QVERIFY(complete);
    QSignalSpy active(window, &DetailWindow::networkPageActiveChanged);
    controller.showProcesses();
    QVERIFY(!window->showingNetwork());
    QVERIFY(!active.isEmpty() && !active.last().first().toBool());
    controller.showNetwork();
    QCOMPARE(page->selectedInterface(), selected);
    window->showMinimized();
    QCOMPARE(active.last().first().toBool(), false);
    controller.showNetwork();
    QCOMPARE(active.last().first().toBool(), true);
}
void NetworkWorkflowTests::selectedInterfaceAndPageSurviveRestart()
{
    const QString selected = QStringLiteral("saved-test-interface");
    {
        ApplicationController controller; controller.showNetwork();
        DetailWindow *window = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *detail = qobject_cast<DetailWindow *>(widget)) window = detail;
        QVERIFY(window);
        window->networkPage()->setSelectedInterface(selected);
        // Exercise the same persistence hook as a normal application exit,
        // with QSettings redirected to this test's temporary directory.
        QVERIFY(QMetaObject::invokeMethod(qApp, "aboutToQuit", Qt::DirectConnection));
    }
    QSettings saved; saved.sync();
    QCOMPARE(saved.value("detailPage").toString(), QStringLiteral("network"));
    QCOMPARE(saved.value("networkInterface").toString(), selected);
    {
        ApplicationController restored;
        DetailWindow *window = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *detail = qobject_cast<DetailWindow *>(widget)) window = detail;
        QVERIFY(window && window->showingNetwork());
        QCOMPARE(window->networkPage()->selectedInterface(), selected);
    }
}
QTEST_MAIN(NetworkWorkflowTests)
#include "network_workflow_tests.moc"

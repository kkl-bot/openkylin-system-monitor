// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_details_dialog.h"
#include "ui/network_history_chart.h"
#include "ui/network_page.h"
#include "ui/network_presentation.h"
#include <QComboBox>
#include <QAbstractItemView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPersistentModelIndex>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>
#include <limits>

using namespace monitor;
using namespace monitor::ui;
namespace {
NetworkSnapshot snapshot()
{
    NetworkSnapshot result; result.timestamp = QDateTime::fromMSecsSinceEpoch(1700000000000LL, Qt::UTC);
    NetworkInterfaceInfo loopback; loopback.name = QStringLiteral("lo"); loopback.index = 1;
    loopback.loopback = true; loopback.up = true; loopback.running = true;
    NetworkInterfaceInfo ethernet; ethernet.name = QStringLiteral("eth0"); ethernet.displayName = QStringLiteral("以太网");
    ethernet.index = 2; ethernet.up = true; ethernet.running = true;
    ethernet.addresses = {{QStringLiteral("192.0.2.15"), QStringLiteral("255.255.255.0"), 24, false},
        {QStringLiteral("2001:db8::15"), QStringLiteral("ffff:ffff:ffff:ffff::"), 64, true}};
    ethernet.gateways = QStringList{QStringLiteral("192.0.2.1"), QStringLiteral("2001:db8::1")};
    ethernet.dnsServers = QStringList{QStringLiteral("192.0.2.53"), QStringLiteral("2001:db8::53")};
    ethernet.dnsSource = QStringLiteral("NetworkManager · per interface");
    NetworkInterfaceInfo wifi; wifi.name = QStringLiteral("wlan0"); wifi.index = 3; wifi.up = true;
    wifi.running = true; wifi.defaultRoute = true; wifi.downloadBytesPerSecond = 0; wifi.uploadBytesPerSecond = 1024;
    result.interfaces = {loopback, ethernet, wifi};
    return result;
}
QLabel *label(NetworkPage &page, const char *name) { return page.findChild<QLabel *>(QString::fromLatin1(name)); }
QPushButton *button(NetworkPage &page, const char *name) { return page.findChild<QPushButton *>(QString::fromLatin1(name)); }
}
class NetworkUiTests final : public QObject {
    Q_OBJECT
private slots:
    void choosesDefaultRouteBeforeOtherInterfaces();
    void restoresSelectionAndKeepsMissingInterfaceExplicit();
    void samplingPreservesInterfaceMenuModel();
    void distinguishesUnavailableFromZero();
    void distinguishesSystemAndEnvironmentProxyConfiguration();
    void controlsAndDetailsPreserveSnapshot();
    void detailsShowEveryAddressAndSourceAsPlainText();
    void detailsDistinguishMissingRoutesFromReadErrors();
    void historyRetainsGapsAcrossUnknownAndPause();
    void narrowPageKeepsControlsReachable();
};
void NetworkUiTests::choosesDefaultRouteBeforeOtherInterfaces()
{
    NetworkPage page; page.acceptSnapshot(snapshot()); QCOMPARE(page.selectedInterface(), QStringLiteral("wlan0"));
    auto sample = snapshot(); sample.interfaces[2].up = false;
    NetworkPage noDefault; noDefault.acceptSnapshot(sample); QCOMPARE(noDefault.selectedInterface(), QStringLiteral("eth0"));
    sample.interfaces[1].up = false;
    NetworkPage onlyLoopback; onlyLoopback.acceptSnapshot(sample); QCOMPARE(onlyLoopback.selectedInterface(), QStringLiteral("lo"));
    NetworkPage empty; empty.acceptSnapshot({}); QVERIFY(empty.selectedInterface().isEmpty());
    QVERIFY(!button(empty, "networkDetails")->isEnabled());
}
void NetworkUiTests::restoresSelectionAndKeepsMissingInterfaceExplicit()
{
    NetworkPage page; page.setSelectedInterface(QStringLiteral("eth0")); auto sample = snapshot(); page.acceptSnapshot(sample);
    QCOMPARE(page.selectedInterface(), QStringLiteral("eth0"));
    QVERIFY(label(page, "networkInterfaceName")->isHidden());
    QSignalSpy changed(&page, &NetworkPage::selectedInterfaceChanged);
    sample.interfaces.removeAt(1); sample.timestamp = sample.timestamp.addSecs(1); page.acceptSnapshot(sample);
    QCOMPARE(page.selectedInterface(), QStringLiteral("eth0")); QCOMPARE(changed.count(), 0);
    QVERIFY(label(page, "networkInterfaceName")->text().contains(QStringLiteral("接口不可用")));
    QVERIFY(!label(page, "networkInterfaceName")->isHidden());
    QVERIFY(!button(page, "networkDetails")->isEnabled()); QCOMPARE(label(page, "networkDownload")->text(), QStringLiteral("—"));
    page.acceptSnapshot(snapshot()); QVERIFY(button(page, "networkDetails")->isEnabled());
    auto *selector = page.findChild<QComboBox *>(QStringLiteral("networkInterface")); selector->setCurrentIndex(selector->findData(QStringLiteral("wlan0")));
    QCOMPARE(page.selectedInterface(), QStringLiteral("wlan0")); QCOMPARE(changed.count(), 1);
    NetworkPage restored; restored.setSelectedInterface(page.selectedInterface()); restored.acceptSnapshot(snapshot());
    QCOMPARE(restored.selectedInterface(), QStringLiteral("wlan0"));
}
void NetworkUiTests::distinguishesUnavailableFromZero()
{
    NetworkPage page; page.setSelectedInterface(QStringLiteral("eth0")); auto sample = snapshot(); page.acceptSnapshot(sample);
    QCOMPARE(label(page, "networkDownload")->text(), QStringLiteral("—")); QCOMPARE(label(page, "networkUpload")->text(), QStringLiteral("—"));
    auto *downloadCard = label(page, "networkDownload")->parentWidget(); const int initialCardWidth = downloadCard->minimumWidth();
    QVERIFY(initialCardWidth >= 180);
    QVERIFY(initialCardWidth >= label(page, "networkDownload")->fontMetrics().horizontalAdvance(QStringLiteral("999.9 MiB/s")) + 32);
    sample.timestamp = sample.timestamp.addSecs(1); sample.interfaces[1].downloadBytesPerSecond = 0;
    sample.interfaces[1].uploadBytesPerSecond = 1024 * 1024; page.acceptSnapshot(sample);
    QCOMPARE(label(page, "networkDownload")->text(), QStringLiteral("0 B/s"));
    QCOMPARE(label(page, "networkUpload")->text(), QStringLiteral("1.0 MiB/s"));
    QCOMPARE(downloadCard->minimumWidth(), initialCardWidth);
    auto larger = page.font(); larger.setPointSize(20); page.setFont(larger);
    QVERIFY(downloadCard->minimumWidth() > initialCardWidth);
    QCOMPARE(networkRateLabel(std::numeric_limits<double>::infinity()), QStringLiteral("—"));
    QCOMPARE(networkRateLabel(-1), QStringLiteral("—"));
    QVERIFY(label(page, "networkLinkNotice")->text().contains(QStringLiteral("不代表互联网可达")));
    sample.interfaces[1].running = false; page.acceptSnapshot(sample);
    QVERIFY(label(page, "networkLinkStatus")->text().contains(QStringLiteral("等待链路")));
}
void NetworkUiTests::samplingPreservesInterfaceMenuModel()
{
    NetworkPage page; auto sample = snapshot(); page.acceptSnapshot(sample);
    auto *selector = page.findChild<QComboBox *>(QStringLiteral("networkInterface"));
    auto *model = selector->model();
    QSignalSpy removed(model, &QAbstractItemModel::rowsRemoved), inserted(model, &QAbstractItemModel::rowsInserted), reset(model, &QAbstractItemModel::modelReset);
    const QPersistentModelIndex pending(model->index(selector->findData(QStringLiteral("eth0")), 0));
    selector->view()->setCurrentIndex(pending);
    sample.timestamp = sample.timestamp.addSecs(1); sample.interfaces[1].downloadBytesPerSecond = 4096;
    sample.interfaces[1].running = false; sample.proxy.mode = ProxyMode::Manual; page.acceptSnapshot(sample);
    QCOMPARE(removed.count(), 0); QCOMPARE(inserted.count(), 0); QCOMPARE(reset.count(), 0);
    QVERIFY(pending.isValid()); QCOMPARE(selector->view()->currentIndex(), QModelIndex(pending));
    QCOMPARE(page.selectedInterface(), QStringLiteral("wlan0"));
    sample.interfaces[1].displayName = QStringLiteral("重新命名网卡"); sample.timestamp = sample.timestamp.addSecs(1); page.acceptSnapshot(sample);
    QVERIFY(removed.count() > 0); QVERIFY(selector->itemText(selector->findData(QStringLiteral("eth0"))).contains(QStringLiteral("重新命名网卡")));
}
void NetworkUiTests::distinguishesSystemAndEnvironmentProxyConfiguration()
{
    NetworkPage page; auto sample = snapshot(); sample.proxy.mode = ProxyMode::Disabled;
    sample.proxy.environmentVariables = QStringList{QStringLiteral("HTTPS_PROXY")}; page.acceptSnapshot(sample);
    QCOMPARE(label(page, "networkSystemProxy")->text(), QStringLiteral("系统代理：未启用"));
    QVERIFY(label(page, "networkEnvironmentProxy")->text().contains(QStringLiteral("已配置：HTTPS_PROXY")));
    sample.proxy.mode = ProxyMode::Manual; sample.proxy.environmentVariables.clear(); page.acceptSnapshot(sample);
    QCOMPARE(label(page, "networkSystemProxy")->text(), QStringLiteral("系统代理：已启用（手动）"));
    QVERIFY(label(page, "networkEnvironmentProxy")->text().endsWith(QStringLiteral("未配置")));
    sample.proxy.mode = ProxyMode::Automatic; page.acceptSnapshot(sample);
    QCOMPARE(label(page, "networkSystemProxy")->text(), QStringLiteral("系统代理：已启用（自动）"));
    sample.proxy.mode = ProxyMode::Unknown; page.acceptSnapshot(sample);
    QVERIFY(label(page, "networkSystemProxy")->text().contains(QStringLiteral("未知")));
    QVERIFY(label(page, "networkProxyNotice")->text().contains(QStringLiteral("由应用决定")));
    QCOMPARE(label(page, "networkSystemProxy")->textFormat(), Qt::PlainText);
}
void NetworkUiTests::controlsAndDetailsPreserveSnapshot()
{
    NetworkPage page; page.setSelectedInterface(QStringLiteral("eth0")); auto sample = snapshot(); page.acceptSnapshot(sample);
    QSignalSpy paused(&page, &NetworkPage::pauseRequested), interval(&page, &NetworkPage::intervalRequested), refresh(&page, &NetworkPage::refreshRequested);
    page.setPaused(true); page.setInterval(2000); QCOMPARE(paused.count(), 0); QCOMPARE(interval.count(), 0);
    button(page, "networkPause")->click(); QCOMPARE(paused.count(), 1); QCOMPARE(paused.first().first().toBool(), false);
    page.findChild<QComboBox *>(QStringLiteral("networkInterval"))->setCurrentIndex(3);
    QCOMPARE(interval.count(), 1); QCOMPARE(interval.first().first().toInt(), 5000);
    button(page, "networkRefresh")->click(); QCOMPARE(refresh.count(), 1);
    QTimer::singleShot(0, &page, [&page, &sample]() {
        auto *dialog = page.findChild<NetworkDetailsDialog *>(); QVERIFY(dialog);
        auto *details = dialog->findChild<QPlainTextEdit *>(QStringLiteral("networkDetailsText")); QVERIFY(details);
        const auto captured = details->toPlainText();
        sample.interfaces[1].addresses.clear(); page.acceptSnapshot(sample); page.setSelectedInterface(QStringLiteral("wlan0"));
        QCOMPARE(details->toPlainText(), captured); QVERIFY(captured.contains(QStringLiteral("192.0.2.15")));
        dialog->reject();
    });
    button(page, "networkDetails")->click();
}
void NetworkUiTests::detailsShowEveryAddressAndSourceAsPlainText()
{
    auto sample = snapshot(); auto interface = sample.interfaces[1];
    interface.displayName = QStringLiteral("<b>external %2</b>"); sample.proxy.detail = QStringLiteral("<img src=x> literal");
    NetworkDetailsDialog dialog(interface, sample.proxy, sample.timestamp);
    auto *text = dialog.findChild<QPlainTextEdit *>(QStringLiteral("networkDetailsText")); QVERIFY(text); QVERIFY(text->isReadOnly());
    const QString details = text->toPlainText();
    for (const auto &address : interface.addresses) { QVERIFY(details.contains(address.address)); QVERIFY(details.contains(address.subnetMask)); }
    for (const auto &gateway : interface.gateways) QVERIFY(details.contains(gateway));
    for (const auto &dns : interface.dnsServers) QVERIFY(details.contains(dns));
    QVERIFY(details.contains(interface.dnsSource)); QVERIFY(details.contains(interface.name));
    QVERIFY(details.contains(interface.displayName)); QVERIFY(details.contains(sample.proxy.detail));
    QVERIFY(details.contains(QStringLiteral("前缀长度：24"))); QVERIFY(details.contains(QStringLiteral("前缀长度：64")));
    QCOMPARE(text->wordWrapMode(), QTextOption::WrapAtWordBoundaryOrAnywhere);
    sample.proxy.detail = QString(2048, QLatin1Char('X')) + QStringLiteral("<b>原文</b>");
    NetworkDetailsDialog narrow(interface, sample.proxy, sample.timestamp); narrow.resize(360, 400); narrow.show(); QTest::qWait(20);
    auto *narrowText = narrow.findChild<QPlainTextEdit *>(QStringLiteral("networkDetailsText"));
    QVERIFY(narrowText->toPlainText().contains(sample.proxy.detail));
    QCOMPARE(narrowText->horizontalScrollBar()->maximum(), 0);
}
void NetworkUiTests::historyRetainsGapsAcrossUnknownAndPause()
{
    NetworkHistoryChart chart; auto sample = snapshot(); const auto start = sample.timestamp;
    chart.setInterface(QStringLiteral("eth0")); chart.acceptSnapshot(sample);
    QVERIFY(!chart.history(QStringLiteral("eth0")).last().download);
    sample.timestamp = start.addSecs(1); sample.interfaces[1].downloadBytesPerSecond = 0; chart.acceptSnapshot(sample);
    QVERIFY(chart.history(QStringLiteral("eth0")).last().download.has_value()); QCOMPARE(*chart.history(QStringLiteral("eth0")).last().download, 0.0);
    chart.setPaused(true); chart.setPaused(false);
    sample.timestamp = start.addSecs(2); sample.interfaces[1].downloadBytesPerSecond = 1024; chart.acceptSnapshot(sample);
    QVERIFY(chart.history(QStringLiteral("eth0")).last().breakBefore);
    sample.timestamp = start.addSecs(3); sample.interfaces[1].downloadBytesPerSecond.reset(); chart.acceptSnapshot(sample);
    QVERIFY(!chart.history(QStringLiteral("eth0")).last().download);
    for (int second = 4; second <= 80; ++second) {
        sample.timestamp = start.addSecs(second); sample.interfaces[1].downloadBytesPerSecond = second; chart.acceptSnapshot(sample);
    }
    QCOMPARE(chart.history(QStringLiteral("eth0")).size(), 61);
    QVERIFY(chart.history(QStringLiteral("eth0")).first().timestamp >= start.addSecs(20));
    sample.timestamp = start.addSecs(81); ++sample.interfaces[1].index; chart.acceptSnapshot(sample);
    QCOMPARE(chart.history(QStringLiteral("eth0")).size(), 1);
    chart.resize(460, 250); chart.show(); QVERIFY(!chart.grab().isNull());
}
void NetworkUiTests::detailsDistinguishMissingRoutesFromReadErrors()
{
    auto sample = snapshot(); auto interface = sample.interfaces[1];
    interface.gateways.clear(); interface.routeError = QStringLiteral("IPv4 路由表读取失败");
    NetworkDetailsDialog unavailable(interface, sample.proxy, sample.timestamp);
    const auto missingText = unavailable.findChild<QPlainTextEdit *>(QStringLiteral("networkDetailsText"))->toPlainText();
    QVERIFY(missingText.contains(QStringLiteral("默认路由：未知（读取失败）")));
    QVERIFY(missingText.contains(QStringLiteral("无法读取网关"))); QVERIFY(missingText.contains(interface.routeError));
    interface.defaultRoute = true; interface.gateways = QStringList{QStringLiteral("2001:db8::1")};
    NetworkDetailsDialog partial(interface, sample.proxy, sample.timestamp);
    const auto partialText = partial.findChild<QPlainTextEdit *>(QStringLiteral("networkDetailsText"))->toPlainText();
    QVERIFY(partialText.contains(QStringLiteral("默认路由：是")));
    QVERIFY(partialText.contains(QStringLiteral("2001:db8::1"))); QVERIFY(partialText.contains(interface.routeError));
    QVERIFY(!partialText.contains(QStringLiteral("无法读取网关")));
}
void NetworkUiTests::narrowPageKeepsControlsReachable()
{
    NetworkPage page; auto font = page.font(); font.setPointSize(20); page.setFont(font);
    page.resize(520, 500); page.acceptSnapshot(snapshot()); page.show(); QTest::qWait(20);
    auto *scroll = page.findChild<QScrollArea *>(QStringLiteral("networkContentScroll")); QVERIFY(scroll); QCOMPARE(page.width(), 520);
    const int chartTop = page.findChild<NetworkHistoryChart *>()->mapTo(scroll->widget(), QPoint()).y();
    QVERIFY(label(page, "networkSystemProxy")->mapTo(scroll->widget(), QPoint()).y() < chartTop);
    QVERIFY(label(page, "networkEnvironmentProxy")->mapTo(scroll->widget(), QPoint()).y() < chartTop);
    QVERIFY(button(page, "networkPause")->mapTo(scroll->widget(), QPoint()).y()
        < label(page, "networkDownload")->mapTo(scroll->widget(), QPoint()).y());
    for (const char *name : {"networkRefresh", "networkDetails", "networkPause"}) {
        auto *control = button(page, name); QVERIFY(control);
        QVERIFY(control->width() >= control->minimumSizeHint().width());
        const QRect area(control->mapTo(scroll->widget(), QPoint()), control->size());
        QVERIFY(area.left() >= 0); QVERIFY(area.right() < scroll->widget()->width());
        scroll->ensureWidgetVisible(control);
        QTRY_VERIFY(scroll->viewport()->rect().contains(control->mapTo(scroll->viewport(), control->rect().center())));
    }
}
QTEST_MAIN(NetworkUiTests)
#include "network_ui_tests.moc"

// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/network_collector.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

using namespace monitor;

namespace {
void put(const QString &path, const QByteArray &contents)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), qint64(contents.size()));
}

NetworkSourcePaths fixturePaths(const QTemporaryDir &directory)
{
    NetworkSourcePaths paths;
    paths.sysClassNetRoot = directory.path() + "/net";
    paths.procNetRoot = directory.path() + "/proc-net";
    paths.resolvConf = directory.path() + "/resolv.conf";
    paths.kernelRelease = directory.path() + "/osrelease";
    paths.useNetworkManager = false;
    return paths;
}

NetworkDevice device(const QString &name = QStringLiteral("eth0"), int index = 2)
{
    NetworkDevice result;
    result.name = name;
    result.displayName = QStringLiteral("测试网卡");
    result.index = index;
    result.flags = QNetworkInterface::IsUp | QNetworkInterface::IsRunning;
    return result;
}

void counters(const NetworkSourcePaths &paths, const NetworkDevice &interface, quint64 received, quint64 sent)
{
    const QString root = paths.sysClassNetRoot + '/' + interface.name;
    put(root + "/ifindex", QByteArray::number(interface.index) + '\n');
    put(root + "/statistics/rx_bytes", QByteArray::number(received) + '\n');
    put(root + "/statistics/tx_bytes", QByteArray::number(sent) + '\n');
}

QNetworkAddressEntry address(const QString &ip, int prefix)
{
    QNetworkAddressEntry result;
    result.setIp(QHostAddress(ip));
    result.setPrefixLength(prefix);
    return result;
}

QByteArray ipv4Route(const QByteArray &name, const QString &gateway, const QByteArray &flags = "0003")
{
    const auto encoded = QByteArray::number(qToBigEndian(QHostAddress(gateway).toIPv4Address()), 16).rightJustified(8, '0');
    return name + "\t00000000\t" + encoded + '\t' + flags + "\t0\t0\t100\t00000000\t0\t0\t0\n";
}

void executable(const QString &path, const QByteArray &script)
{
    put(path, script);
    QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
}

void nmcliOutput(const QString &path, const QByteArray &output)
{
    executable(path, "#!/bin/sh\ncat <<'NETWORK_FIXTURE_EOF'\n" + output + "\nNETWORK_FIXTURE_EOF\n");
}
} // namespace

class NetworkCollectorTests final : public QObject {
    Q_OBJECT
private slots:
    void ipv4Ipv6AddressesMasksRoutesAndGlobalDns();
    void ratesUseCounterDeltaAndActualElapsedTime();
    void resetMissingAndInvalidCountersStayUnavailable();
    void disappearanceRenameAndIndexChangeResetBaselines();
    void disconnectAndClockRegressionResetRates();
    void stubDnsIsLabelledGlobalAndInvalidAddressesAreIgnored();
    void networkManagerDnsIsPerInterfaceCachedAndManuallyRefreshed();
    void networkManagerTimeoutIsBoundedAndFallsBack();
    void missingRouteTablesRemainUnknownAndPreserveObservedRoutes();
    void routeReadFailureClearsStaleGatewaysAndRecovers();
};

void NetworkCollectorTests::ipv4Ipv6AddressesMasksRoutesAndGlobalDns()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = fixturePaths(directory);
    auto ethernet = device();
    ethernet.addresses = {address("192.168.10.2", 24), address("2001:db8::2", 64)};
    auto loopback = device("lo", 1);
    loopback.flags |= QNetworkInterface::IsLoopBack;
    loopback.addresses = {address("127.0.0.1", 8)};
    counters(paths, ethernet, 100, 200);
    counters(paths, loopback, 10, 20);
    put(paths.procNetRoot + "/route", "Iface Destination Gateway Flags RefCnt Use Metric Mask MTU Window IRTT\n"
        + ipv4Route("eth0", "192.168.10.1") + ipv4Route("lo", "127.0.0.1", "0201")
        + "eth0 FFFFFF00 0100000A 0003 0 0 100 FFFFFF00 0 0 0\n");
    put(paths.procNetRoot + "/ipv6_route",
        "00000000000000000000000000000000 00 00000000000000000000000000000000 00 fe800000000000000000000000000001 00000064 00000000 00000000 00000003 eth0\n"
        "00000000000000000000000000000000 00 00000000000000000000000000000000 00 00000000000000000000000000000000 ffffffff 00000000 00000000 00200200 lo\n");
    put(paths.resolvConf, "# WSL generated resolver\nnameserver 10.255.255.254\nnameserver 2001:db8::53\nnameserver 10.255.255.254\n");
    put(paths.kernelRelease, "6.6.87.2-microsoft-standard-WSL2\n");
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{loopback, ethernet, ethernet}; });
    const auto snapshot = collector.collect(0);
    QVERIFY(snapshot.error.isEmpty());
    QVERIFY(snapshot.isWsl);
    QCOMPARE(snapshot.interfaces.size(), 2); // Duplicate provider rows are never counted twice.
    const auto &info = snapshot.interfaces.first();
    QCOMPARE(info.name, QStringLiteral("eth0"));
    QVERIFY(info.up && info.running && !info.loopback && info.defaultRoute);
    QCOMPARE(info.addresses.size(), 2);
    QCOMPARE(info.addresses[0].subnetMask, QStringLiteral("255.255.255.0"));
    QCOMPARE(info.addresses[0].prefixLength, 24);
    QVERIFY(!info.addresses[0].ipv6);
    QCOMPARE(info.addresses[1].subnetMask, QStringLiteral("ffff:ffff:ffff:ffff::"));
    QCOMPARE(info.addresses[1].prefixLength, 64);
    QVERIFY(info.addresses[1].ipv6);
    QCOMPARE(info.gateways, QStringList({"192.168.10.1", "fe80::1%eth0"}));
    QCOMPARE(info.dnsServers, QStringList({"10.255.255.254", "2001:db8::53"}));
    QVERIFY(info.dnsSource.contains(QStringLiteral("全局")));
    QVERIFY(info.dnsSource.contains(QStringLiteral("非网卡专属")));
    QVERIFY(!snapshot.interfaces.last().defaultRoute);
    QVERIFY(info.routeError.isEmpty());
    QCOMPARE(int(snapshot.proxy.mode), int(ProxyMode::Unknown));
}

void NetworkCollectorTests::missingRouteTablesRemainUnknownAndPreserveObservedRoutes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = fixturePaths(directory);
    const auto ethernet = device();
    counters(paths, ethernet, 100, 100);
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{ethernet}; });
    auto info = collector.collect(0).interfaces.first();
    QVERIFY(!info.defaultRoute);
    QVERIFY(info.gateways.isEmpty());
    QVERIFY(info.routeError.contains(QStringLiteral("IPv4")));
    QVERIFY(info.routeError.contains(QStringLiteral("IPv6")));
    QVERIFY(info.error.isEmpty()); // Route availability is independent of valid traffic counters.

    put(paths.procNetRoot + "/route", ipv4Route("eth0", "192.0.2.1"));
    collector.requestDetailsRefresh();
    info = collector.collect(1000).interfaces.first();
    QVERIFY(info.defaultRoute);
    QCOMPARE(info.gateways, QStringList{QStringLiteral("192.0.2.1")});
    QVERIFY(info.routeError.contains(QStringLiteral("IPv6")));
    QVERIFY(!info.routeError.contains(QStringLiteral("IPv4")));
    QCOMPARE(info.downloadBytesPerSecond.value_or(-1), 0.0);

    QVERIFY(QFile::remove(paths.procNetRoot + "/route"));
    put(paths.procNetRoot + "/ipv6_route",
        "00000000000000000000000000000000 00 00000000000000000000000000000000 00 fe800000000000000000000000000001 00000064 00000000 00000000 00000003 eth0\n");
    collector.requestDetailsRefresh();
    info = collector.collect(2000).interfaces.first();
    QVERIFY(info.defaultRoute);
    QCOMPARE(info.gateways, QStringList{QStringLiteral("fe80::1%eth0")});
    QVERIFY(info.routeError.contains(QStringLiteral("IPv4")));
    QVERIFY(!info.routeError.contains(QStringLiteral("IPv6")));
}

void NetworkCollectorTests::routeReadFailureClearsStaleGatewaysAndRecovers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto paths = fixturePaths(directory);
    const auto ethernet = device();
    counters(paths, ethernet, 100, 100);
    put(paths.procNetRoot + "/route", ipv4Route("eth0", "192.0.2.1"));
    put(paths.procNetRoot + "/ipv6_route", {});
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{ethernet}; });
    auto info = collector.collect(0).interfaces.first();
    QVERIFY(info.defaultRoute);
    QVERIFY(info.routeError.isEmpty());

    // Oversized/unreadable data must not be treated as a valid empty route table,
    // or retain the now unverified gateway from the previous configuration.
    put(paths.procNetRoot + "/route", QByteArray(1024 * 1024 + 1, 'x'));
    collector.requestDetailsRefresh();
    info = collector.collect(1000).interfaces.first();
    QVERIFY(!info.defaultRoute);
    QVERIFY(info.gateways.isEmpty());
    QVERIFY(info.routeError.contains(QStringLiteral("IPv4")));
    QVERIFY(!info.routeError.contains(QStringLiteral("IPv6")));

    put(paths.procNetRoot + "/route", "Iface Destination Gateway Flags RefCnt Use Metric Mask MTU Window IRTT\n");
    collector.requestDetailsRefresh();
    info = collector.collect(2000).interfaces.first();
    QVERIFY(!info.defaultRoute);
    QVERIFY(info.gateways.isEmpty());
    QVERIFY(info.routeError.isEmpty()); // Both tables are readable and contain no default route.
    QCOMPARE(info.downloadBytesPerSecond.value_or(-1), 0.0);

    put(paths.procNetRoot + "/route", ipv4Route("eth0", "192.0.2.254"));
    collector.requestDetailsRefresh();
    info = collector.collect(3000).interfaces.first();
    QVERIFY(info.defaultRoute);
    QCOMPARE(info.gateways, QStringList{QStringLiteral("192.0.2.254")});
    QVERIFY(info.routeError.isEmpty());
}

void NetworkCollectorTests::ratesUseCounterDeltaAndActualElapsedTime()
{
    QTemporaryDir directory;
    const auto paths = fixturePaths(directory);
    const auto ethernet = device();
    counters(paths, ethernet, 1000, 2000);
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{ethernet}; });
    auto info = collector.collect(1000).interfaces.first();
    QVERIFY(!info.downloadBytesPerSecond && !info.uploadBytesPerSecond);
    counters(paths, ethernet, 5096, 3024);
    info = collector.collect(3000).interfaces.first();
    QCOMPARE(info.downloadBytesPerSecond.value_or(-1), 2048.0);
    QCOMPARE(info.uploadBytesPerSecond.value_or(-1), 512.0);
    QCOMPARE(info.receivedBytes.value_or(0), quint64(5096));
    info = collector.collect(4000).interfaces.first();
    QCOMPARE(info.downloadBytesPerSecond.value_or(-1), 0.0);
    QCOMPARE(info.uploadBytesPerSecond.value_or(-1), 0.0);
}

void NetworkCollectorTests::resetMissingAndInvalidCountersStayUnavailable()
{
    QTemporaryDir directory;
    const auto paths = fixturePaths(directory);
    const auto ethernet = device();
    counters(paths, ethernet, 1000, 2000);
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{ethernet}; });
    collector.collect(0);
    counters(paths, ethernet, 100, 200);
    auto info = collector.collect(1000).interfaces.first();
    QVERIFY(!info.downloadBytesPerSecond && !info.uploadBytesPerSecond);
    const auto rx = paths.sysClassNetRoot + "/eth0/statistics/rx_bytes";
    put(rx, "18446744073709551616\n");
    info = collector.collect(2000).interfaces.first();
    QVERIFY(!info.receivedBytes && !info.downloadBytesPerSecond);
    QCOMPARE(info.uploadBytesPerSecond.value_or(-1), 0.0);
    QVERIFY(!info.error.isEmpty());
    put(rx, "-20\n");
    QVERIFY(!collector.collect(3000).interfaces.first().receivedBytes);
    QVERIFY(QFile::remove(rx));
    QVERIFY(!collector.collect(4000).interfaces.first().receivedBytes);
    counters(paths, ethernet, 500, 300);
    QVERIFY(!collector.collect(5000).interfaces.first().downloadBytesPerSecond);
    QCOMPARE(collector.collect(6000).interfaces.first().downloadBytesPerSecond.value_or(-1), 0.0);
    put(paths.sysClassNetRoot + "/eth0/ifindex", "99\n");
    info = collector.collect(7000).interfaces.first();
    QVERIFY(!info.receivedBytes && !info.transmittedBytes);
    counters(paths, ethernet, 500, 300);
    QVERIFY(!collector.collect(8000).interfaces.first().downloadBytesPerSecond);
}

void NetworkCollectorTests::disappearanceRenameAndIndexChangeResetBaselines()
{
    QTemporaryDir directory;
    const auto paths = fixturePaths(directory);
    QVector<NetworkDevice> devices{device()};
    counters(paths, devices.first(), 1000, 2000);
    NetworkCollector collector(paths, [&]() { return devices; });
    collector.collect(0);
    devices.clear();
    QVERIFY(collector.collect(1000).interfaces.isEmpty());
    devices = {device()};
    counters(paths, devices.first(), 2000, 3000);
    QVERIFY(!collector.collect(2000).interfaces.first().downloadBytesPerSecond);
    devices[0].index = 3;
    counters(paths, devices.first(), 3000, 4000);
    QVERIFY(!collector.collect(3000).interfaces.first().downloadBytesPerSecond);
    devices[0].name = "enp0s3";
    counters(paths, devices.first(), 4000, 5000);
    QVERIFY(!collector.collect(4000).interfaces.first().downloadBytesPerSecond);
}

void NetworkCollectorTests::disconnectAndClockRegressionResetRates()
{
    QTemporaryDir directory;
    const auto paths = fixturePaths(directory);
    auto ethernet = device();
    counters(paths, ethernet, 100, 100);
    NetworkCollector collector(paths, [&]() { return QVector<NetworkDevice>{ethernet}; });
    collector.collect(1000);
    ethernet.flags &= ~QNetworkInterface::IsRunning;
    counters(paths, ethernet, 200, 200);
    QVERIFY(!collector.collect(2000).interfaces.first().downloadBytesPerSecond);
    ethernet.flags |= QNetworkInterface::IsRunning;
    counters(paths, ethernet, 400, 400);
    QVERIFY(!collector.collect(3000).interfaces.first().downloadBytesPerSecond);
    counters(paths, ethernet, 500, 500);
    QCOMPARE(collector.collect(4000).interfaces.first().downloadBytesPerSecond.value_or(-1), 100.0);
    QVERIFY(!collector.collect(4000).interfaces.first().downloadBytesPerSecond);
    QVERIFY(!collector.collect(3000).interfaces.first().downloadBytesPerSecond);
    QVERIFY(!collector.collect(-1).interfaces.first().downloadBytesPerSecond);
    QVERIFY(!collector.collect(0).interfaces.first().downloadBytesPerSecond);
}

void NetworkCollectorTests::stubDnsIsLabelledGlobalAndInvalidAddressesAreIgnored()
{
    QTemporaryDir directory;
    const auto paths = fixturePaths(directory);
    auto ethernet = device();
    ethernet.addresses = {address("192.0.2.4", -1), address("192.0.2.4", -1)};
    counters(paths, ethernet, 0, 0);
    put(paths.resolvConf, "nameserver 127.0.0.53\nnameserver ::1\nnameserver garbage\nnameserver 0.0.0.0\nnameserver ::\n");
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{ethernet}; });
    auto info = collector.collect(0).interfaces.first();
    QCOMPARE(info.dnsServers, QStringList({"127.0.0.53", "::1"}));
    QVERIFY(info.dnsSource.contains(QStringLiteral("存根")));
    QVERIFY(info.dnsSource.contains(QStringLiteral("全局")));
    QCOMPARE(info.addresses.size(), 1);
    QCOMPARE(info.addresses.first().prefixLength, -1);
    QVERIFY(info.addresses.first().subnetMask.isEmpty());
    put(paths.resolvConf, "# no DNS servers\n");
    collector.requestDetailsRefresh();
    info = collector.collect(1000).interfaces.first();
    QVERIFY(info.dnsServers.isEmpty());
    QCOMPARE(info.dnsSource, QStringLiteral("未提供 DNS 信息"));
}

void NetworkCollectorTests::networkManagerDnsIsPerInterfaceCachedAndManuallyRefreshed()
{
#ifndef Q_OS_UNIX
    QSKIP("The fake nmcli executable fixture uses a POSIX script");
#endif
    QTemporaryDir directory;
    auto paths = fixturePaths(directory);
    paths.useNetworkManager = true;
    paths.nmcliExecutable = directory.path() + "/nmcli-fixture";
    const auto ethernet = device();
    const auto wifi = device("wlan0", 3);
    counters(paths, ethernet, 100, 100);
    counters(paths, wifi, 0, 0);
    put(paths.resolvConf, "nameserver 10.255.255.254\n");
    nmcliOutput(paths.nmcliExecutable, "GENERAL.DEVICE:eth0\nIP4.DNS[1]:1.1.1.1\nIP6.DNS[1]:2001:db8::53\nGENERAL.DEVICE:wlan0\n");
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{ethernet, wifi}; });
    auto snapshot = collector.collect(0);
    QCOMPARE(snapshot.interfaces.first().dnsServers, QStringList({"1.1.1.1", "2001:db8::53"}));
    QVERIFY(snapshot.interfaces.first().dnsSource.contains("NetworkManager"));
    QCOMPARE(snapshot.interfaces.last().dnsServers, QStringList({"10.255.255.254"}));
    QVERIFY(snapshot.interfaces.last().dnsSource.contains(QStringLiteral("全局")));
    nmcliOutput(paths.nmcliExecutable, "GENERAL.DEVICE:eth0\nIP4.DNS[1]:9.9.9.9\n");
    QCOMPARE(collector.collect(1000).interfaces.first().dnsServers.first(), QStringLiteral("1.1.1.1"));
    collector.requestDetailsRefresh();
    counters(paths, ethernet, 1100, 1100);
    snapshot = collector.collect(2000);
    QCOMPARE(snapshot.interfaces.first().dnsServers, QStringList({"9.9.9.9"}));
    QCOMPARE(snapshot.interfaces.first().downloadBytesPerSecond.value_or(-1), 1000.0);
    nmcliOutput(paths.nmcliExecutable, "GENERAL.DEVICE:eth0\nIP4.DNS[1]:8.8.8.8\n");
    QCOMPARE(collector.collect(6000).interfaces.first().dnsServers, QStringList({"9.9.9.9"}));
    QCOMPARE(collector.collect(7000).interfaces.first().dnsServers, QStringList({"8.8.8.8"}));
}

void NetworkCollectorTests::networkManagerTimeoutIsBoundedAndFallsBack()
{
#ifndef Q_OS_UNIX
    QSKIP("The fake nmcli executable fixture uses a POSIX script");
#endif
    QTemporaryDir directory;
    auto paths = fixturePaths(directory);
    paths.useNetworkManager = true;
    paths.networkManagerTimeoutMs = 150;
    paths.nmcliExecutable = directory.path() + "/nmcli-timeout-fixture";
    executable(paths.nmcliExecutable, "#!/bin/sh\nexec sleep 5\n");
    const auto ethernet = device();
    counters(paths, ethernet, 0, 0);
    put(paths.resolvConf, "nameserver 10.255.255.254\n");
    NetworkCollector collector(paths, [=]() { return QVector<NetworkDevice>{ethernet}; });
    QElapsedTimer timer;
    timer.start();
    const auto info = collector.collect(0).interfaces.first();
    QVERIFY2(timer.elapsed() < 1500, "A timed-out nmcli process must not stall network sampling");
    QCOMPARE(info.dnsServers, QStringList({"10.255.255.254"}));
    QVERIFY(info.dnsSource.contains(QStringLiteral("全局")));
}

QTEST_GUILESS_MAIN(NetworkCollectorTests)
#include "network_collector_tests.moc"

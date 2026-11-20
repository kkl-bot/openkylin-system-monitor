// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/network_collector.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>
#include <limits>
#include <utility>

namespace monitor {
namespace {

std::optional<QByteArray> readFile(const QString &path, qint64 maximum = 1024 * 1024)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    const auto data = file.read(maximum + 1);
    if (data.size() > maximum || file.error() != QFileDevice::NoError) return std::nullopt;
    return data;
}

std::optional<quint64> unsignedValue(const QByteArray &data, int base = 10)
{
    const auto text = data.trimmed();
    if (text.isEmpty() || text.startsWith('-') || text.startsWith('+')) return std::nullopt;
    bool ok = false;
    const auto value = text.toULongLong(&ok, base);
    return ok ? std::optional<quint64>(value) : std::nullopt;
}

std::optional<quint64> counter(const QString &path)
{
    const auto data = readFile(path, 128);
    return data ? unsignedValue(*data) : std::nullopt;
}

QString identity(const NetworkDevice &device)
{
    return QString::number(device.index) + ':' + device.name;
}

bool safeDevice(const NetworkDevice &device)
{
    // Interface names are also used as one sysfs path component.
    return device.index > 0 && !device.name.isEmpty() && device.name != "." && device.name != ".."
        && !device.name.contains('/') && !device.name.contains('\0');
}

void appendUnique(QStringList &values, const QString &value)
{
    if (!value.isEmpty() && !values.contains(value)) values.append(value);
}

QString dnsAddress(const QString &text)
{
    const QHostAddress address(text.trimmed());
    if (address.isNull() || address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6)
        return {};
    return address.toString();
}

QStringList resolverDns(const QByteArray &contents)
{
    QStringList result;
    for (const auto &line : contents.split('\n')) {
        const auto fields = line.simplified().split(' ');
        if (fields.size() >= 2 && fields[0] == "nameserver")
            appendUnique(result, dnsAddress(QString::fromLatin1(fields[1])));
    }
    return result;
}

std::optional<QHostAddress> ipv6Hex(const QByteArray &text)
{
    if (text.size() != 32) return std::nullopt;
    Q_IPV6ADDR bytes{};
    for (int i = 0; i < 16; ++i) {
        const auto part = unsignedValue(text.mid(i * 2, 2), 16);
        if (!part || *part > 255) return std::nullopt;
        bytes[i] = quint8(*part);
    }
    return QHostAddress(bytes);
}

std::optional<double> bytesPerSecond(const std::optional<quint64> &current,
                                      const std::optional<quint64> &previous, qint64 elapsed)
{
    if (!current || !previous || *current < *previous || elapsed <= 0) return std::nullopt;
    return double(*current - *previous) * 1000.0 / double(elapsed);
}

} // namespace

NetworkCollector::NetworkCollector(const NetworkSourcePaths &paths, InterfaceProvider provider)
    : paths_(paths), provider_(provider ? std::move(provider) : InterfaceProvider(systemInterfaces))
{
    paths_.networkManagerTimeoutMs = std::clamp(paths_.networkManagerTimeoutMs, 50, 1000);
    if (paths_.useNetworkManager) {
        nmcliExecutable_ = paths_.nmcliExecutable.isEmpty()
            ? QStandardPaths::findExecutable(QStringLiteral("nmcli")) : paths_.nmcliExecutable;
    }
}

QVector<NetworkDevice> NetworkCollector::systemInterfaces()
{
    QVector<NetworkDevice> devices;
    for (const auto &interface : QNetworkInterface::allInterfaces())
        devices.append({interface.name(), interface.humanReadableName(), interface.index(),
                        interface.flags(), interface.addressEntries()});
    return devices;
}

void NetworkCollector::requestDetailsRefresh()
{
    configurationTimestamp_.reset();
}

QHash<QString, QStringList> NetworkCollector::networkManagerDns() const
{
    QHash<QString, QStringList> result;
    if (nmcliExecutable_.isEmpty()) return result;
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(environment);
    process.setStandardErrorFile(QProcess::nullDevice());
    QElapsedTimer timer;
    timer.start();
    process.start(nmcliExecutable_, {"--terse", "--escape", "no", "--colors", "no", "--fields",
                                   "GENERAL.DEVICE,IP4.DNS,IP6.DNS", "device", "show"});
    const int budget = paths_.networkManagerTimeoutMs;
    const int cleanupBudget = qMin(100, budget / 5);
    const bool started = process.waitForStarted(qMin(200, budget - cleanupBudget));
    const int remaining = qMax(0, budget - int(timer.elapsed()) - cleanupBudget);
    if (!started || !process.waitForFinished(remaining)) {
        process.kill();
        process.waitForFinished(qMax(0, budget - int(timer.elapsed())));
        return result;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return result;
    const auto output = process.readAllStandardOutput();
    if (output.size() > 1024 * 1024) return result;
    QString device;
    for (const auto &line : output.split('\n')) {
        const auto colon = line.indexOf(':');
        if (colon < 0) continue;
        const auto key = line.left(colon);
        const auto value = QString::fromUtf8(line.mid(colon + 1)).trimmed();
        if (key == "GENERAL.DEVICE") device = value;
        else if (!device.isEmpty() && (key == "IP4.DNS" || key.startsWith("IP4.DNS[")
                                       || key == "IP6.DNS" || key.startsWith("IP6.DNS[")))
            appendUnique(result[device], dnsAddress(value));
    }
    return result;
}

void NetworkCollector::refreshDetails(const QVector<NetworkDevice> &devices, qint64 milliseconds)
{
    QStringList topology;
    for (const auto &device : devices)
        topology.append(identity(device) + ':' + QString::number(int(device.flags)));
    topology.sort();
    const QString signature = topology.join('\n');
    if (configurationTimestamp_ && *configurationTimestamp_ >= 0 && milliseconds >= *configurationTimestamp_
        && milliseconds - *configurationTimestamp_ < 5000 && signature == configurationTopology_) return;
    configuration_.clear();
    configurationTimestamp_ = milliseconds;
    configurationTopology_ = signature;
    for (const auto &device : devices) configuration_.insert(device.name, {});

    QStringList routeErrors;
    if (const auto routes = readFile(paths_.procNetRoot + "/route")) {
        for (const auto &line : routes->split('\n')) {
            const auto fields = line.simplified().split(' ');
            if (fields.size() < 8 || fields[1] != "00000000" || fields[7] != "00000000") continue;
            const auto flags = unsignedValue(fields[3], 16);
            const auto gateway = unsignedValue(fields[2], 16);
            const QString name = QString::fromLocal8Bit(fields[0]);
            if (!flags || !gateway || !(*flags & 0x1) || (*flags & 0x200)
                || *gateway > std::numeric_limits<quint32>::max() || !configuration_.contains(name)) continue;
            auto &config = configuration_[name];
            config.defaultRoute = true;
            // /proc/net/route prints the kernel's network-order integer using host byte order.
            if (*gateway != 0) appendUnique(config.gateways, QHostAddress(qFromBigEndian(quint32(*gateway))).toString());
        }
    } else {
        routeErrors.append(QStringLiteral("IPv4 路由表缺失或读取失败"));
    }
    if (const auto routes = readFile(paths_.procNetRoot + "/ipv6_route")) {
        for (const auto &line : routes->split('\n')) {
            const auto fields = line.simplified().split(' ');
            if (fields.size() != 10 || fields[0] != QByteArray(32, '0') || fields[1] != "00") continue;
            const auto flags = unsignedValue(fields[8], 16);
            auto gateway = ipv6Hex(fields[4]);
            const QString name = QString::fromLocal8Bit(fields[9]);
            if (!flags || !gateway || !(*flags & 0x1) || (*flags & 0x200) || !configuration_.contains(name)) continue;
            auto &config = configuration_[name];
            config.defaultRoute = true;
            if (*gateway != QHostAddress::AnyIPv6) {
                if (gateway->isLinkLocal()) gateway->setScopeId(name);
                appendUnique(config.gateways, gateway->toString());
            }
        }
    } else {
        routeErrors.append(QStringLiteral("IPv6 路由表缺失或读取失败"));
    }
    const auto managedDns = networkManagerDns();
    const auto resolver = readFile(paths_.resolvConf);
    const QStringList globalDns = resolver ? resolverDns(*resolver) : QStringList{};
    const bool hasStub = std::any_of(globalDns.cbegin(), globalDns.cend(), [](const QString &value) {
        return QHostAddress(value).isLoopback();
    });
    for (auto entry = configuration_.begin(); entry != configuration_.end(); ++entry) {
        if (!routeErrors.isEmpty())
            entry->routeError = routeErrors.join(QStringLiteral("；"))
                + QStringLiteral("，默认路由和网关信息可能不完整。");
        entry->dnsServers = managedDns.value(entry.key());
        if (!entry->dnsServers.isEmpty()) entry->dnsSource = QStringLiteral("NetworkManager · 此网卡");
        else {
            entry->dnsServers = globalDns;
            entry->dnsSource = globalDns.isEmpty() ? QStringLiteral("未提供 DNS 信息")
                : hasStub ? QStringLiteral("系统全局 /etc/resolv.conf · 含本地 DNS 存根，非网卡专属")
                          : QStringLiteral("系统全局 /etc/resolv.conf · 非网卡专属");
        }
    }
}

NetworkSnapshot NetworkCollector::collect(qint64 monotonicMilliseconds)
{
    NetworkSnapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTime();
    if (const auto release = readFile(paths_.kernelRelease, 4096))
        snapshot.isWsl = release->toLower().contains("microsoft") || release->toLower().contains("wsl");
    QVector<NetworkDevice> devices;
    QSet<QString> seen;
    for (const auto &device : provider_()) {
        if (!safeDevice(device) || seen.contains(identity(device))) continue;
        seen.insert(identity(device));
        devices.append(device);
    }
    QHash<QString, Counters> next;
    for (const auto &device : devices) {
        NetworkInterfaceInfo info;
        info.name = device.name;
        info.displayName = device.displayName.isEmpty() ? device.name : device.displayName;
        info.index = device.index;
        info.up = device.flags.testFlag(QNetworkInterface::IsUp);
        info.running = device.flags.testFlag(QNetworkInterface::IsRunning);
        info.loopback = device.flags.testFlag(QNetworkInterface::IsLoopBack);
        QSet<QString> addresses;
        for (const auto &entry : device.addresses) {
            const auto ip = entry.ip();
            if (ip.isNull() || addresses.contains(ip.toString())) continue;
            if (ip.protocol() != QAbstractSocket::IPv4Protocol && ip.protocol() != QAbstractSocket::IPv6Protocol) continue;
            addresses.insert(ip.toString());
            info.addresses.append({ip.toString(), entry.netmask().isNull() ? QString{} : entry.netmask().toString(),
                                   entry.prefixLength(), ip.protocol() == QAbstractSocket::IPv6Protocol});
        }
        const QString base = paths_.sysClassNetRoot + '/' + device.name;
        const auto firstIndex = counter(base + "/ifindex");
        info.receivedBytes = counter(base + "/statistics/rx_bytes");
        info.transmittedBytes = counter(base + "/statistics/tx_bytes");
        const auto finalIndex = counter(base + "/ifindex");
        if (!firstIndex || !finalIndex || *firstIndex != quint64(device.index) || *finalIndex != *firstIndex) {
            info.receivedBytes.reset();
            info.transmittedBytes.reset();
            info.error = QStringLiteral("网卡计数或身份暂不可用，等待下一次采样");
        } else if (!info.receivedBytes || !info.transmittedBytes) {
            info.error = QStringLiteral("部分流量计数不可用");
        }
        const QString key = identity(device);
        const auto old = previous_.constFind(key);
        const bool connected = info.up && info.running;
        if (connected && old != previous_.cend() && old->connected && old->milliseconds >= 0
            && monotonicMilliseconds > old->milliseconds) {
            const auto elapsed = monotonicMilliseconds - old->milliseconds;
            info.downloadBytesPerSecond = bytesPerSecond(info.receivedBytes, old->received, elapsed);
            info.uploadBytesPerSecond = bytesPerSecond(info.transmittedBytes, old->transmitted, elapsed);
        }
        next.insert(key, {info.receivedBytes, info.transmittedBytes, monotonicMilliseconds, connected});
        snapshot.interfaces.append(std::move(info));
    }
    previous_ = std::move(next);
    // Read every counter before any bounded-but-slow nmcli query. The supplied
    // monotonic timestamp therefore tracks counter sampling, not DNS lookup completion.
    refreshDetails(devices, monotonicMilliseconds);
    for (auto &info : snapshot.interfaces) {
        const auto config = configuration_.value(info.name);
        info.defaultRoute = config.defaultRoute;
        info.gateways = config.gateways;
        info.dnsServers = config.dnsServers;
        info.dnsSource = config.dnsSource;
        info.routeError = config.routeError;
    }
    std::sort(snapshot.interfaces.begin(), snapshot.interfaces.end(), [](const NetworkInterfaceInfo &a, const NetworkInterfaceInfo &b) {
        if (a.loopback != b.loopback) return !a.loopback;
        if (a.defaultRoute != b.defaultRoute) return a.defaultRoute;
        if (a.up != b.up) return a.up;
        return a.index < b.index;
    });
    if (snapshot.interfaces.isEmpty()) snapshot.error = QStringLiteral("未发现可用网卡");
    return snapshot;
}

} // namespace monitor

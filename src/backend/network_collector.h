// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "domain/network_types.h"
#include <QHash>
#include <QNetworkInterface>
#include <functional>

namespace monitor {

struct NetworkSourcePaths {
    QString sysClassNetRoot = QStringLiteral("/sys/class/net");
    QString procNetRoot = QStringLiteral("/proc/net");
    QString resolvConf = QStringLiteral("/etc/resolv.conf");
    QString kernelRelease = QStringLiteral("/proc/sys/kernel/osrelease");
    QString nmcliExecutable;
    bool useNetworkManager = true;
    int networkManagerTimeoutMs = 800;
};

struct NetworkDevice {
    QString name;
    QString displayName;
    int index = 0;
    QNetworkInterface::InterfaceFlags flags;
    QList<QNetworkAddressEntry> addresses;
};

class NetworkCollector final {
public:
    using InterfaceProvider = std::function<QVector<NetworkDevice>()>;
    explicit NetworkCollector(const NetworkSourcePaths &paths = {}, InterfaceProvider provider = {});
    NetworkSnapshot collect(qint64 monotonicMilliseconds);
    void requestDetailsRefresh();

private:
    struct Counters {
        std::optional<quint64> received;
        std::optional<quint64> transmitted;
        qint64 milliseconds = 0;
        bool connected = false;
    };
    struct Configuration {
        bool defaultRoute = false;
        QStringList gateways;
        QStringList dnsServers;
        QString dnsSource;
        QString routeError;
    };
    static QVector<NetworkDevice> systemInterfaces();
    void refreshDetails(const QVector<NetworkDevice> &devices, qint64 milliseconds);
    QHash<QString, QStringList> networkManagerDns() const;

    NetworkSourcePaths paths_;
    InterfaceProvider provider_;
    QString nmcliExecutable_;
    QHash<QString, Counters> previous_;
    QHash<QString, Configuration> configuration_;
    std::optional<qint64> configurationTimestamp_;
    QString configurationTopology_;
};

} // namespace monitor

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QStringList>
#include <QVector>
#include <optional>

namespace monitor {
struct NetworkAddress {
    QString address;
    QString subnetMask;
    int prefixLength = -1;
    bool ipv6 = false;
};
struct NetworkInterfaceInfo {
    QString name;
    QString displayName;
    int index = 0;
    bool up = false;
    bool running = false;
    bool loopback = false;
    bool defaultRoute = false;
    QVector<NetworkAddress> addresses;
    QStringList gateways;
    QStringList dnsServers;
    QString dnsSource;
    std::optional<quint64> receivedBytes;
    std::optional<quint64> transmittedBytes;
    std::optional<double> downloadBytesPerSecond;
    std::optional<double> uploadBytesPerSecond;
    QString error;
    // When nonempty, false defaultRoute means unknown, not proof that no route exists.
    QString routeError;
};
enum class ProxyMode { Unknown, Disabled, Manual, Automatic };
struct ProxyInfo {
    ProxyMode mode = ProxyMode::Unknown;
    QString source;
    QString detail;
    // Names only: proxy URLs can contain credentials and must never enter snapshots.
    QStringList environmentVariables;
};
struct NetworkSnapshot {
    QDateTime timestamp;
    QVector<NetworkInterfaceInfo> interfaces;
    ProxyInfo proxy;
    QString error;
    bool isWsl = false;
};
} // namespace monitor
Q_DECLARE_METATYPE(monitor::NetworkSnapshot)

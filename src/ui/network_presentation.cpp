// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_presentation.h"
#include <cmath>

namespace monitor::ui {
QString networkRateLabel(const std::optional<double> &bytesPerSecond)
{
    if (!bytesPerSecond || !std::isfinite(*bytesPerSecond) || *bytesPerSecond < 0) return QStringLiteral("—");
    const QStringList units = {QStringLiteral("B/s"), QStringLiteral("KiB/s"), QStringLiteral("MiB/s"),
        QStringLiteral("GiB/s"), QStringLiteral("TiB/s"), QStringLiteral("PiB/s"), QStringLiteral("EiB/s")};
    double amount = *bytesPerSecond; int unit = 0;
    while (amount >= 1024 && unit + 1 < units.size()) { amount /= 1024; ++unit; }
    return QString::number(amount, 'f', unit ? 1 : 0) + QLatin1Char(' ') + units[unit];
}
QString networkLinkLabel(const NetworkInterfaceInfo &interface)
{
    if (!interface.up) return QStringLiteral("未启用");
    if (interface.loopback) return QStringLiteral("本机回环");
    return interface.running ? QStringLiteral("链路已连接") : QStringLiteral("已启用，等待链路");
}
QString networkProxyModeLabel(ProxyMode mode)
{
    switch (mode) {
    case ProxyMode::Disabled: return QStringLiteral("未启用");
    case ProxyMode::Manual: return QStringLiteral("已启用（手动）");
    case ProxyMode::Automatic: return QStringLiteral("已启用（自动）");
    case ProxyMode::Unknown: return QStringLiteral("未知（无法读取）");
    }
    return QStringLiteral("未知（无法读取）");
}
QString networkEnvironmentProxyLabel(const ProxyInfo &proxy)
{
    return proxy.environmentVariables.isEmpty() ? QStringLiteral("未配置")
        : QStringLiteral("已配置：") + proxy.environmentVariables.join(QStringLiteral("、"));
}
} // namespace monitor::ui

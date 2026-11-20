// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "domain/network_types.h"

namespace monitor::ui {
QString networkRateLabel(const std::optional<double> &bytesPerSecond);
QString networkLinkLabel(const NetworkInterfaceInfo &interface);
QString networkProxyModeLabel(ProxyMode mode);
QString networkEnvironmentProxyLabel(const ProxyInfo &proxy);
} // namespace monitor::ui

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "domain/network_types.h"
#include <QDialog>

namespace monitor::ui {
class NetworkDetailsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit NetworkDetailsDialog(const NetworkInterfaceInfo &interface, const ProxyInfo &proxy,
        const QDateTime &timestamp, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
};
} // namespace monitor::ui

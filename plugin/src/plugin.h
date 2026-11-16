// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QQmlExtensionPlugin>
class KylinMonitorPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override;
};

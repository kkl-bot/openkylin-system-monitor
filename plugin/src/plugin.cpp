// SPDX-License-Identifier: GPL-3.0-or-later
#include "plugin.h"
#include "monitor_backend.h"
#include "resource_bars.h"
#include <qqml.h>

void KylinMonitorPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(QString::fromLatin1(uri) == QStringLiteral("org.ukui.kylinmonitor"));
    qmlRegisterType<monitor::widget::MonitorBackend>(uri, 1, 0, "MonitorBackend");
    qmlRegisterType<monitor::widget::ResourceBars>(uri, 1, 0, "ResourceBars");
}

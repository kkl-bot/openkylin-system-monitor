// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_details_dialog.h"
#include "ui/network_presentation.h"
#include "domain/metrics.h"
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScreen>
#include <QShowEvent>
#include <QTextOption>
#include <QVBoxLayout>
#include <QWindow>

namespace monitor::ui {
NetworkDetailsDialog::NetworkDetailsDialog(const NetworkInterfaceInfo &interface, const ProxyInfo &proxy,
    const QDateTime &timestamp, QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("networkDetailsDialog")); setWindowTitle(QStringLiteral("网络详细信息"));
    resize(qMax(640, fontMetrics().horizontalAdvance(QLatin1Char('M')) * 64), qMax(480, fontMetrics().height() * 24));
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(QStringLiteral("显示打开时的网卡快照；DNS 来源和代理配置不代表所有应用的实际行为。"), this);
    notice->setTextFormat(Qt::PlainText); notice->setWordWrap(true);
    notice->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); layout->addWidget(notice);
    auto *text = new QPlainTextEdit(this); text->setObjectName(QStringLiteral("networkDetailsText"));
    text->setReadOnly(true); text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    text->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    const auto value = [](const QString &source) { return source.isEmpty() ? QStringLiteral("未提供") : source; };
    QStringList lines = {QStringLiteral("接口名字：") + value(interface.name), QStringLiteral("显示名称：") + value(interface.displayName),
        QStringLiteral("接口编号：") + QString::number(interface.index), QStringLiteral("链路状态：") + networkLinkLabel(interface),
        QStringLiteral("默认路由：") + (interface.defaultRoute ? QStringLiteral("是")
            : (interface.routeError.isEmpty() ? QStringLiteral("否") : QStringLiteral("未知（读取失败）"))),
        QStringLiteral("采样时间：") + (timestamp.isValid() ? timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("未提供")),
        QString(), QStringLiteral("IP 地址")};
    if (interface.addresses.isEmpty()) lines.append(QStringLiteral("未发现地址"));
    for (const auto &address : interface.addresses) {
        lines.append((address.ipv6 ? QStringLiteral("IPv6：") : QStringLiteral("IPv4：")) + value(address.address));
        lines.append(QStringLiteral("  子网掩码：") + value(address.subnetMask));
        lines.append(QStringLiteral("  前缀长度：") + (address.prefixLength >= 0 ? QString::number(address.prefixLength) : QStringLiteral("未提供")));
    }
    lines.append({QString(), QStringLiteral("网关")});
    lines.append(interface.gateways.isEmpty()
        ? QStringList{interface.routeError.isEmpty() ? QStringLiteral("未发现网关") : QStringLiteral("无法读取网关")}
        : interface.gateways);
    if (!interface.routeError.isEmpty()) lines.append(QStringLiteral("路由读取说明：") + interface.routeError);
    lines.append({QString(), QStringLiteral("DNS 服务器")});
    lines.append(interface.dnsServers.isEmpty() ? QStringList{QStringLiteral("未提供 DNS 服务器")} : interface.dnsServers);
    lines.append(QStringLiteral("DNS 来源：") + value(interface.dnsSource));
    lines.append({QString(), QStringLiteral("系统代理：") + networkProxyModeLabel(proxy.mode),
        QStringLiteral("系统代理来源：") + value(proxy.source), QStringLiteral("系统代理说明：") + value(proxy.detail),
        QStringLiteral("环境代理变量：") + networkEnvironmentProxyLabel(proxy),
        QStringLiteral("代理是否生效由应用决定；链路状态不代表互联网可达。"), QString(),
        QStringLiteral("累计接收：") + (interface.receivedBytes ? formatBytes(*interface.receivedBytes) : QStringLiteral("未提供")),
        QStringLiteral("累计发送：") + (interface.transmittedBytes ? formatBytes(*interface.transmittedBytes) : QStringLiteral("未提供"))});
    if (!interface.error.isEmpty()) lines.append(QStringLiteral("读取说明：") + interface.error);
    text->setPlainText(lines.join(QLatin1Char('\n'))); layout->addWidget(text, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
void NetworkDetailsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    const auto *screen = windowHandle() ? windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect available = screen->availableGeometry().adjusted(16, 32, -16, -32); if (available.isEmpty()) return;
    resize(size().boundedTo(available.size()));
    const QPoint position = parentWidget() ? parentWidget()->mapToGlobal(parentWidget()->rect().center()) - rect().center()
        : available.center() - rect().center();
    move(qBound(available.left(), position.x(), qMax(available.left(), available.right() - width() + 1)),
         qBound(available.top(), position.y(), qMax(available.top(), available.bottom() - height() + 1)));
}
} // namespace monitor::ui

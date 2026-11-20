// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_page.h"
#include "ui/flow_layout.h"
#include "ui/network_details_dialog.h"
#include "ui/network_history_chart.h"
#include "ui/network_presentation.h"
#include "ui/network_rate_card.h"
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace monitor::ui {
namespace {
QLabel *label(const QString &text, const QString &name, QWidget *parent, bool flexible = true)
{
    auto *result = new QLabel(text, parent); result->setObjectName(name);
    result->setTextFormat(Qt::PlainText); result->setWordWrap(true);
    if (flexible) result->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return result;
}
QString interfaceLabel(const NetworkInterfaceInfo &interface)
{
    return interface.displayName.isEmpty() || interface.displayName == interface.name ? interface.name
        : interface.displayName + QStringLiteral(" · ") + interface.name;
}
}
NetworkPage::NetworkPage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("networkPage"));
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this); scroll->setObjectName(QStringLiteral("networkContentScroll"));
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget; scroll->setWidget(content); outer->addWidget(scroll);
    auto *root = new QVBoxLayout(content); root->setContentsMargins(24, 20, 24, 18); root->setSpacing(8);
    root->setSizeConstraint(QLayout::SetMinAndMaxSize);
    root->addWidget(label(QStringLiteral("网络"), QStringLiteral("pageTitle"), this));
    root->addWidget(label(QStringLiteral("按网卡查看实时收发速度 · 最近 60 秒"), QStringLiteral("subtitle"), this));
    interfaces_ = new QComboBox(this); interfaces_->setObjectName(QStringLiteral("networkInterface"));
    interfaces_->setAccessibleName(QStringLiteral("选择网卡")); interfaces_->setMinimumContentsLength(12);
    interfaces_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    root->addWidget(interfaces_);
    interfaceName_ = label({}, QStringLiteral("networkInterfaceName"), this); root->addWidget(interfaceName_);
    auto *actions = new FlowLayout;
    auto *refresh = new QPushButton(QStringLiteral("刷新"), this); refresh->setObjectName(QStringLiteral("networkRefresh")); actions->addWidget(refresh);
    details_ = new QPushButton(QStringLiteral("详细信息"), this); details_->setObjectName(QStringLiteral("networkDetails")); actions->addWidget(details_);
    actions->addWidget(label(QStringLiteral("刷新间隔"), QStringLiteral("muted"), this, false));
    interval_ = new QComboBox(this); interval_->setObjectName(QStringLiteral("networkInterval"));
    interval_->addItem(QStringLiteral("0.5 秒"), 500); interval_->addItem(QStringLiteral("1 秒"), 1000);
    interval_->addItem(QStringLiteral("2 秒"), 2000); interval_->addItem(QStringLiteral("5 秒"), 5000); interval_->setCurrentIndex(1); actions->addWidget(interval_);
    pause_ = new QPushButton(QStringLiteral("暂停"), this); pause_->setObjectName(QStringLiteral("networkPause")); pause_->setCheckable(true);
    actions->addWidget(pause_);
    root->addLayout(actions);
    auto *rates = new FlowLayout;
    auto *downloadCard = new NetworkRateCard(QStringLiteral("↓ 下载速度"), QStringLiteral("networkDownload"), this);
    auto *uploadCard = new NetworkRateCard(QStringLiteral("↑ 上传速度"), QStringLiteral("networkUpload"), this);
    download_ = downloadCard->valueLabel(); upload_ = uploadCard->valueLabel();
    rates->addWidget(downloadCard); rates->addWidget(uploadCard); root->addLayout(rates);
    auto *linkStatus = new FlowLayout;
    link_ = label({}, QStringLiteral("networkLinkStatus"), this, false); linkStatus->addWidget(link_);
    linkStatus->addWidget(label(QStringLiteral("链路状态不代表互联网可达。"), QStringLiteral("networkLinkNotice"), this, false));
    root->addLayout(linkStatus);
    root->addWidget(label(QStringLiteral("代理配置"), QStringLiteral("sectionTitle"), this));
    auto *proxyStatus = new FlowLayout;
    systemProxy_ = label({}, QStringLiteral("networkSystemProxy"), this, false); proxyStatus->addWidget(systemProxy_);
    environmentProxy_ = label({}, QStringLiteral("networkEnvironmentProxy"), this, false); proxyStatus->addWidget(environmentProxy_);
    root->addLayout(proxyStatus);
    root->addWidget(label(QStringLiteral("是否使用代理由应用决定。"), QStringLiteral("networkProxyNotice"), this));
    notice_ = label({}, QStringLiteral("networkNotice"), this); root->addWidget(notice_);
    chart_ = new NetworkHistoryChart(this); root->addWidget(chart_, 1);
    status_ = label({}, QStringLiteral("networkStatus"), this); root->addWidget(status_);
    connect(interfaces_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0) setSelectedInterface(interfaces_->itemData(index).toString());
    });
    connect(details_, &QPushButton::clicked, this, &NetworkPage::openDetails);
    connect(refresh, &QPushButton::clicked, this, &NetworkPage::refreshRequested);
    connect(pause_, &QPushButton::toggled, this, [this](bool paused) { setPaused(paused); emit pauseRequested(paused); });
    connect(interval_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        chart_->setInterval(interval_->currentData().toInt()); emit intervalRequested(interval_->currentData().toInt());
    });
    rebuildInterfaces(); refreshView();
}
void NetworkPage::acceptSnapshot(const NetworkSnapshot &snapshot)
{
    snapshot_ = snapshot; chart_->acceptSnapshot(snapshot);
    if (selectedInterface_.isEmpty()) {
        selectedInterface_ = defaultInterface();
        if (!selectedInterface_.isEmpty()) emit selectedInterfaceChanged(selectedInterface_);
    }
    rebuildInterfaces(); refreshView();
}
void NetworkPage::setPaused(bool paused)
{
    paused_ = paused; chart_->setPaused(paused);
    const QSignalBlocker blocker(pause_); pause_->setChecked(paused);
    pause_->setText(paused ? QStringLiteral("继续") : QStringLiteral("暂停")); refreshView();
}
void NetworkPage::setInterval(int milliseconds)
{
    const int index = interval_->findData(milliseconds); if (index < 0) return;
    const QSignalBlocker blocker(interval_); interval_->setCurrentIndex(index); chart_->setInterval(milliseconds);
}
QString NetworkPage::selectedInterface() const { return selectedInterface_; }
void NetworkPage::setSelectedInterface(const QString &name)
{
    const QString selected = name.isEmpty() ? defaultInterface() : name;
    if (selectedInterface_ == selected) return;
    selectedInterface_ = selected; rebuildInterfaces(); refreshView(); emit selectedInterfaceChanged(selectedInterface_);
}
const NetworkInterfaceInfo *NetworkPage::currentInterface() const
{
    for (const auto &interface : snapshot_.interfaces) if (interface.name == selectedInterface_) return &interface;
    return nullptr;
}
QString NetworkPage::defaultInterface() const
{
    for (const auto &interface : snapshot_.interfaces) if (interface.up && interface.defaultRoute && !interface.loopback) return interface.name;
    for (const auto &interface : snapshot_.interfaces) if (interface.up && !interface.loopback) return interface.name;
    return snapshot_.interfaces.isEmpty() ? QString() : snapshot_.interfaces.first().name;
}
void NetworkPage::rebuildInterfaces()
{
    QStringList names, labels;
    for (const auto &interface : snapshot_.interfaces) {
        names.append(interface.name); labels.append(interfaceLabel(interface));
    }
    if (!selectedInterface_.isEmpty() && !currentInterface()) {
        names.append(selectedInterface_); labels.append(QStringLiteral("接口不可用 · ") + selectedInterface_);
    }
    if (names.isEmpty()) { names.append(QString()); labels.append(QStringLiteral("未发现网卡")); }
    bool changed = interfaces_->count() != names.size();
    for (int row = 0; !changed && row < names.size(); ++row)
        changed = interfaces_->itemData(row).toString() != names[row] || interfaces_->itemText(row) != labels[row];

    const QSignalBlocker blocker(interfaces_);
    // Rate/status updates must not reset the popup's model or interrupt an in-progress selection.
    if (changed) {
        interfaces_->clear();
        for (int row = 0; row < names.size(); ++row) {
            interfaces_->addItem(labels[row], names[row]);
            interfaces_->setItemData(row, QStringLiteral("<qt>") + labels[row].toHtmlEscaped() + QStringLiteral("</qt>"), Qt::ToolTipRole);
        }
    }
    const int selected = qMax(0, interfaces_->findData(selectedInterface_));
    if (interfaces_->currentIndex() != selected) interfaces_->setCurrentIndex(selected);
    interfaces_->setToolTip(QStringLiteral("<qt>") + labels.value(selected).toHtmlEscaped() + QStringLiteral("</qt>"));
    interfaces_->setEnabled(!snapshot_.interfaces.isEmpty());
}
void NetworkPage::refreshView()
{
    const auto *interface = currentInterface(); details_->setEnabled(interface != nullptr);
    chart_->setInterface(selectedInterface_);
    interfaceName_->setText(interface ? interfaceLabel(*interface) : (selectedInterface_.isEmpty() ? QStringLiteral("等待网卡信息")
        : QStringLiteral("接口不可用：") + selectedInterface_));
    interfaceName_->setVisible(interface == nullptr);
    download_->setText(networkRateLabel(interface ? interface->downloadBytesPerSecond : std::nullopt));
    upload_->setText(networkRateLabel(interface ? interface->uploadBytesPerSecond : std::nullopt));
    link_->setText(QStringLiteral("链路状态：") + (interface ? networkLinkLabel(*interface) : QStringLiteral("不可用")));
    systemProxy_->setText(QStringLiteral("系统代理：") + networkProxyModeLabel(snapshot_.proxy.mode));
    environmentProxy_->setText(QStringLiteral("环境代理变量：") + networkEnvironmentProxyLabel(snapshot_.proxy));
    QStringList notes;
    if (snapshot_.isWsl) notes.append(QStringLiteral("WSL 虚拟网卡流量；不代表 Windows 主机全部网络流量。"));
    if (!snapshot_.error.isEmpty()) notes.append(snapshot_.error);
    if (interface && !interface->error.isEmpty()) notes.append(interface->error);
    notice_->setText(notes.join(QLatin1Char('\n'))); notice_->setVisible(!notes.isEmpty());
    status_->setText(!snapshot_.timestamp.isValid() ? QStringLiteral("等待首次采样")
        : (paused_ ? QStringLiteral("已暂停 · ") : QStringLiteral("实时采样 · ")) + QStringLiteral("更新于 ")
            + snapshot_.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss")));
}
void NetworkPage::openDetails()
{
    const auto *interface = currentInterface(); if (!interface) return;
    NetworkDetailsDialog dialog(*interface, snapshot_.proxy, snapshot_.timestamp, this); dialog.exec();
}
} // namespace monitor::ui

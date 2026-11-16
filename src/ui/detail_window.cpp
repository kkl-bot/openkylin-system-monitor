#include "ui/detail_window.h"
#include "ui/history_chart.h"
#include "ui/resource_tile.h"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace monitor::ui {
namespace {
QLabel *label(const QString &text, const QString &name, QWidget *parent = nullptr)
{
    auto *result = new QLabel(text, parent);
    result->setObjectName(name);
    result->setTextFormat(Qt::PlainText);
    return result;
}
QFrame *metricCard(const QString &text, QLabel **value, QLabel **caption = nullptr)
{
    auto *card = new QFrame;
    card->setObjectName("metricCard");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 13, 18, 14);
    auto *title = label(text, "cardLabel");
    if (caption) *caption = title;
    layout->addWidget(title);
    *value = label(QStringLiteral("—"), "cardValue");
    layout->addWidget(*value);
    return card;
}
} // namespace

DetailWindow::DetailWindow(MonitorModel *model, QWidget *parent) : QWidget(parent), model_(model)
{
    setObjectName("detailWindow");
    setWindowTitle(QStringLiteral("系统监测 · Kylin Monitor"));
    resize(1040, 780);
    setMinimumSize(880, 700);
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(232);
    auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(16, 30, 16, 24); side->setSpacing(8);
    side->addWidget(label(QStringLiteral("系统监测"), "brand"));
    side->addWidget(label("KYLIN  /  MONITOR", "eyebrow"));
    side->addSpacing(30);
    side->addWidget(label(QStringLiteral("资源概览"), "muted"));
    side->addSpacing(8);
    auto *resourceHost = new QWidget; resourceHost->setObjectName("resourceHost");
    resourcesLayout_ = new QVBoxLayout(resourceHost); resourcesLayout_->setContentsMargins(0, 0, 0, 0);
    resourcesLayout_->setSpacing(8); resourcesLayout_->addStretch();
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(resourceHost);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    side->addWidget(scroll, 1);
    environment_ = label("openKylin · Linux", "muted"); environment_->setWordWrap(true);
    side->addWidget(environment_);
    side->addWidget(label(QStringLiteral("让系统状态，一目了然。"), "eyebrow"));
    root->addWidget(sidebar);

    auto *main = new QVBoxLayout; main->setContentsMargins(32, 28, 30, 20); main->setSpacing(14);
    auto *top = new QHBoxLayout;
    title_ = label("CPU", "pageTitle"); top->addWidget(title_); top->addStretch();
    status_ = label(QStringLiteral("●  实时监测"), "live"); top->addWidget(status_); top->addSpacing(12);
    auto *mini = new QPushButton(QStringLiteral("极简模式 ↗")); mini->setObjectName("primary");
    mini->setToolTip(QStringLiteral("切换为仅显示柱状图的桌面悬浮窗")); top->addWidget(mini);
    main->addLayout(top);
    subtitle_ = label(QStringLiteral("正在读取处理器信息…"), "subtitle"); subtitle_->setWordWrap(true); main->addWidget(subtitle_);
    main->addSpacing(4);
    auto *cards = new QHBoxLayout; cards->setSpacing(12);
    cards->addWidget(metricCard(QStringLiteral("利用率"), &usage_), 1);
    cards->addWidget(metricCard(QStringLiteral("温度 · 最高传感器读数"), &temperature_), 1);
    cards->addWidget(metricCard(QStringLiteral("逻辑处理器"), &capacity_, &capacityLabel_), 1);
    main->addLayout(cards);
    auto *chartHeading = new QHBoxLayout;
    chartHeading->addWidget(label(QStringLiteral("利用率"), "sectionTitle")); chartHeading->addStretch();
    chartHeading->addWidget(label(QStringLiteral("最近 60 秒"), "muted")); main->addLayout(chartHeading);
    usageChart_ = new HistoryChart(model, false); main->addWidget(usageChart_, 3);
    main->addWidget(label(QStringLiteral("温度"), "sectionTitle"));
    temperatureChart_ = new HistoryChart(model, true); main->addWidget(temperatureChart_, 2);
    notice_ = label({}, "notice"); notice_->setWordWrap(true); notice_->setMinimumHeight(32); main->addWidget(notice_);
    details_ = label({}, "muted"); details_->setWordWrap(true); main->addWidget(details_);
    auto *line = new QFrame; line->setFrameShape(QFrame::HLine); main->addWidget(line);
    auto *bottom = new QHBoxLayout;
    timestamp_ = label(QStringLiteral("等待首次采样"), "muted"); bottom->addWidget(timestamp_); bottom->addStretch();
    bottom->addWidget(label(QStringLiteral("刷新"), "muted"));
    interval_ = new QComboBox;
    interval_->addItem(QStringLiteral("0.5 秒"), 500); interval_->addItem(QStringLiteral("1 秒"), 1000);
    interval_->addItem(QStringLiteral("2 秒"), 2000); interval_->addItem(QStringLiteral("5 秒"), 5000);
    interval_->setCurrentIndex(1); bottom->addWidget(interval_);
    pause_ = new QPushButton(QStringLiteral("暂停")); pause_->setCheckable(true); bottom->addWidget(pause_);
    main->addLayout(bottom); root->addLayout(main, 1);
    connect(model_, &MonitorModel::changed, this, &DetailWindow::refresh);
    connect(mini, &QPushButton::clicked, this, &DetailWindow::miniRequested);
    connect(pause_, &QPushButton::toggled, this, &DetailWindow::pauseRequested);
    connect(interval_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { emit intervalRequested(interval_->currentData().toInt()); });
    refresh();
}
void DetailWindow::rebuildResources()
{
    QStringList ids; for (const auto &sample : model_->snapshot().resources) ids.append(sample.id);
    if (ids == resourceIds_) return;
    qDeleteAll(tiles_); tiles_.clear(); resourceIds_ = ids;
    if (!ids.contains(selectedId_) && !ids.isEmpty()) selectedId_ = ids.first();
    for (const auto &id : ids) {
        auto *tile = new ResourceTile(model_, id); tiles_.append(tile);
        resourcesLayout_->insertWidget(resourcesLayout_->count() - 1, tile);
        connect(tile, &QPushButton::clicked, this, [this, id]() { selectResource(id); });
    }
}
void DetailWindow::selectResource(const QString &id) { selectedId_ = id; refresh(); }
void DetailWindow::refresh()
{
    rebuildResources();
    const auto *sample = model_->resource(selectedId_);
    if (!sample) return;
    for (auto *tile : tiles_) tile->setChecked(tile->resourceId() == selectedId_);
    title_->setText(resourceLabel(sample->kind)); subtitle_->setText(sample->name);
    usage_->setText(formatMetric(sample->utilization, "%", 1));
    temperature_->setText(formatMetric(sample->temperature, " °C", 1));
    usage_->setToolTip(sample->utilization.value ? sample->utilization.source : sample->utilization.reason);
    temperature_->setToolTip(sample->temperature.value ? sample->temperature.source : sample->temperature.reason);
    if (sample->kind == ResourceKind::Cpu) {
        capacityLabel_->setText(QStringLiteral("逻辑处理器")); capacity_->setText(sample->details.value(QStringLiteral("逻辑处理器"), "—"));
    } else {
        capacityLabel_->setText(sample->kind == ResourceKind::Gpu ? QStringLiteral("显存占用 / 总量") : QStringLiteral("已用 / 总内存"));
        capacity_->setText(sample->usedBytes && sample->totalBytes ? formatBytes(*sample->usedBytes) + " / " + formatBytes(*sample->totalBytes) : QStringLiteral("—"));
    }
    capacity_->setStyleSheet(sample->kind == ResourceKind::Cpu ? "font-size: 29px;" : "font-size: 16px;");
    QStringList notes;
    if (!sample->utilization.value) notes.append(sample->utilization.reason);
    if (!sample->temperature.value && !notes.contains(sample->temperature.reason)) notes.append(sample->temperature.reason);
    notice_->setText(notes.isEmpty() ? QStringLiteral("温度取已识别传感器的最高有效值。") : notes.join(QStringLiteral("；")));
    QStringList info;
    for (auto it = sample->details.cbegin(); it != sample->details.cend(); ++it) {
        if (it.key() != QStringLiteral("设备标识")) info.append(it.key() + "  " + it.value());
    }
    details_->setText(info.join(QStringLiteral("    ·    ")));
    environment_->setText(model_->snapshot().isWsl ? QStringLiteral("openKylin · WSL 2\nCPU / 内存反映 WSL 环境") : QStringLiteral("openKylin · Linux"));
    timestamp_->setText(QStringLiteral("更新于 ") + model_->snapshot().timestamp.toLocalTime().toString("HH:mm:ss"));
    usageChart_->setResource(selectedId_); temperatureChart_->setResource(selectedId_);
}
void DetailWindow::setPaused(bool paused)
{
    const QSignalBlocker blocker(pause_); pause_->setChecked(paused);
    pause_->setText(paused ? QStringLiteral("继续") : QStringLiteral("暂停"));
    status_->setText(paused ? QStringLiteral("Ⅱ  已暂停") : QStringLiteral("●  实时监测"));
}
void DetailWindow::setInterval(int milliseconds)
{
    const QSignalBlocker blocker(interval_); interval_->setCurrentIndex(interval_->findData(milliseconds));
}
void DetailWindow::closeEvent(QCloseEvent *event) { event->accept(); qApp->quit(); }
} // namespace monitor::ui

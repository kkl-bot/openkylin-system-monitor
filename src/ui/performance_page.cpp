// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/performance_page.h"
#include "ui/flow_layout.h"
#include "ui/history_chart.h"
#include "ui/memory_composition_bar.h"
#include "ui/performance_metric.h"
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

namespace monitor::ui {
namespace {
QLabel *textLabel(const QString &text, const QString &name, const char *role, QWidget *parent, bool flexible = true)
{
    auto *label = new QLabel(text, parent); label->setObjectName(name); label->setProperty("textRole", role);
    label->setTextFormat(Qt::PlainText); label->setWordWrap(true);
    if (flexible) label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
}
bool validCapacity(const ResourceSample &sample)
{
    return sample.usedBytes && sample.totalBytes && *sample.totalBytes > 0 && *sample.usedBytes <= *sample.totalBytes;
}
QString tooltip(const QString &value) { return QStringLiteral("<qt>") + value.toHtmlEscaped() + QStringLiteral("</qt>"); }
}
PerformancePage::PerformancePage(MonitorModel *model, QWidget *parent) : QWidget(parent), model_(model)
{
    setObjectName(QStringLiteral("performancePage"));
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(0);
    scroll_ = new QScrollArea(this); scroll_->setObjectName(QStringLiteral("performanceScroll"));
    scroll_->setFrameShape(QFrame::NoFrame); scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll_->viewport()->installEventFilter(this);
    auto *content = new QWidget; content->setObjectName(QStringLiteral("performanceContent"));
    contentLayout_ = new QVBoxLayout(content); contentLayout_->setContentsMargins(26, 20, 26, 20); contentLayout_->setSpacing(12);
    contentLayout_->setSizeConstraint(QLayout::SetMinAndMaxSize);
    scroll_->setWidget(content); outer->addWidget(scroll_, 1);

    auto *header = new QHBoxLayout;
    title_ = textLabel(QStringLiteral("CPU"), QStringLiteral("pageTitle"), "title", content, false);
    capacity_ = textLabel({}, QStringLiteral("performanceCapacity"), "section", content);
    capacity_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->addWidget(title_); header->addSpacing(12); header->addWidget(capacity_, 1); contentLayout_->addLayout(header);
    deviceName_ = textLabel({}, QStringLiteral("subtitle"), "body", content); contentLayout_->addWidget(deviceName_);
    auto *usageHeading = new QHBoxLayout;
    usageHeading_ = textLabel(QStringLiteral("利用率"), QStringLiteral("performanceUsageHeading"), "caption", content);
    auto *historyWindow = textLabel(QStringLiteral("最近 60 秒"), QStringLiteral("performanceHistoryWindow"), "caption", content, false);
    usageHeading->addWidget(usageHeading_, 1); usageHeading->addWidget(historyWindow); contentLayout_->addLayout(usageHeading);
    usageChart_ = new HistoryChart(model, false, content); usageChart_->setObjectName(QStringLiteral("performanceUsageChart"));
    usageChart_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); contentLayout_->addWidget(usageChart_);

    composition_ = new QWidget(content); composition_->setObjectName(QStringLiteral("memoryComposition"));
    auto *compositionLayout = new QVBoxLayout(composition_); compositionLayout->setContentsMargins(0, 0, 0, 0); compositionLayout->setSpacing(6);
    compositionLayout->addWidget(textLabel(QStringLiteral("内存组成"), QStringLiteral("memoryCompositionHeading"), "section", composition_));
    compositionBar_ = new MemoryCompositionBar(composition_); compositionLayout->addWidget(compositionBar_);
    auto *compositionLegend = new FlowLayout;
    usedLabel_ = textLabel({}, QStringLiteral("memoryCompositionUsed"), "body", composition_, false);
    availableLabel_ = textLabel({}, QStringLiteral("memoryCompositionAvailable"), "body", composition_, false);
    compositionLegend->addWidget(usedLabel_); compositionLegend->addWidget(availableLabel_); compositionLayout->addLayout(compositionLegend);
    compositionNotice_ = textLabel({}, QStringLiteral("memoryCompositionNotice"), "caption", composition_); compositionLayout->addWidget(compositionNotice_);
    contentLayout_->addWidget(composition_);

    auto *metrics = new FlowLayout(nullptr, 14);
    usageMetric_ = new PerformanceMetric(QStringLiteral("利用率"), QStringLiteral("cardValue"), content);
    capacityMetric_ = new PerformanceMetric(QStringLiteral("逻辑处理器"), QStringLiteral("performanceCapacityValue"), content);
    extraMetric_ = new PerformanceMetric(QStringLiteral("平均频率"), QStringLiteral("performanceExtraValue"), content);
    temperatureMetric_ = new PerformanceMetric(QStringLiteral("温度"), QStringLiteral("performanceTemperatureValue"), content);
    metrics->addWidget(usageMetric_); metrics->addWidget(capacityMetric_); metrics->addWidget(extraMetric_); metrics->addWidget(temperatureMetric_);
    contentLayout_->addLayout(metrics);
    details_ = textLabel({}, QStringLiteral("performanceDetails"), "body", content); contentLayout_->addWidget(details_);
    notice_ = textLabel({}, QStringLiteral("notice"), "caption", content); contentLayout_->addWidget(notice_);

    temperatureHeading_ = textLabel(QStringLiteral("温度历史"), QStringLiteral("performanceTemperatureHeading"), "section", content);
    contentLayout_->addWidget(temperatureHeading_);
    temperatureChart_ = new HistoryChart(model, true, content); temperatureChart_->setObjectName(QStringLiteral("performanceTemperatureChart"));
    temperatureChart_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); contentLayout_->addWidget(temperatureChart_);
    temperatureNotice_ = textLabel({}, QStringLiteral("performanceTemperatureNotice"), "caption", content); contentLayout_->addWidget(temperatureNotice_);
    environment_ = textLabel({}, QStringLiteral("performanceEnvironment"), "caption", content); contentLayout_->addWidget(environment_);
    timestamp_ = textLabel(QStringLiteral("等待首次采样"), QStringLiteral("performanceTimestamp"), "caption", content); contentLayout_->addWidget(timestamp_);
    contentLayout_->addStretch();

    auto *footer = new QFrame(this); footer->setObjectName(QStringLiteral("performanceControls"));
    auto *controls = new FlowLayout(footer, 8); controls->setContentsMargins(18, 10, 18, 10);
    status_ = textLabel(QStringLiteral("实时"), QStringLiteral("performanceStatus"), "caption", footer, false); controls->addWidget(status_);
    controls->addWidget(textLabel(QStringLiteral("刷新"), QStringLiteral("performanceIntervalLabel"), "body", footer, false));
    interval_ = new QComboBox(footer); interval_->setObjectName(QStringLiteral("performanceInterval"));
    interval_->addItem(QStringLiteral("0.5 秒"), 500); interval_->addItem(QStringLiteral("1 秒"), 1000);
    interval_->addItem(QStringLiteral("2 秒"), 2000); interval_->addItem(QStringLiteral("5 秒"), 5000); interval_->setCurrentIndex(1);
    controls->addWidget(interval_);
    pause_ = new QPushButton(QStringLiteral("暂停"), footer); pause_->setObjectName(QStringLiteral("performancePause")); pause_->setCheckable(true); controls->addWidget(pause_);
    auto *mini = new QPushButton(QStringLiteral("极简模式"), footer); mini->setObjectName(QStringLiteral("primary"));
    mini->setToolTip(QStringLiteral("切换为仅显示柱状图的桌面悬浮窗")); controls->addWidget(mini); outer->addWidget(footer);

    connect(model_, &MonitorModel::changed, this, &PerformancePage::refresh);
    connect(mini, &QPushButton::clicked, this, &PerformancePage::miniRequested);
    connect(pause_, &QPushButton::toggled, this, [this](bool paused) { setPaused(paused); emit pauseRequested(paused); });
    connect(interval_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this]() { emit intervalRequested(interval_->currentData().toInt()); });
    refresh(); scheduleLayoutUpdate();
}
void PerformancePage::setResource(const QString &id)
{
    const bool changed = resourceId_ != id; resourceId_ = id; refresh();
    if (changed) scroll_->verticalScrollBar()->setValue(0);
}
void PerformancePage::setInterval(int milliseconds)
{
    const int index = interval_->findData(milliseconds); if (index < 0) return;
    const QSignalBlocker blocker(interval_); interval_->setCurrentIndex(index);
}
void PerformancePage::setPaused(bool paused)
{
    const QSignalBlocker blocker(pause_); pause_->setChecked(paused);
    pause_->setText(paused ? QStringLiteral("继续") : QStringLiteral("暂停"));
    status_->setText(paused ? QStringLiteral("已暂停") : QStringLiteral("实时"));
}
void PerformancePage::refresh()
{
    usageChart_->setResource(resourceId_); temperatureChart_->setResource(resourceId_);
    const auto *sample = model_->resource(resourceId_);
    if (!sample) {
        title_->setText(QStringLiteral("资源不可用")); capacity_->clear(); deviceName_->setText(QStringLiteral("等待设备数据")); deviceName_->show();
        usageMetric_->setValue(QStringLiteral("—")); capacityMetric_->setValue(QStringLiteral("—")); temperatureMetric_->setValue(QStringLiteral("—"));
        extraMetric_->hide(); composition_->hide(); details_->clear(); notice_->clear(); temperatureNotice_->clear();
        temperatureChart_->hide(); temperatureHeading_->hide(); environment_->clear(); timestamp_->setText(QStringLiteral("等待首次采样")); return;
    }
    const bool memory = sample->kind == ResourceKind::Memory;
    const bool capacityValid = validCapacity(*sample);
    title_->setText(model_->resourceTitle(*sample));
    deviceName_->setText(sample->name); deviceName_->setToolTip(tooltip(sample->name));
    deviceName_->setVisible(!memory || sample->name != QStringLiteral("物理内存"));
    usageHeading_->setText(memory ? QStringLiteral("内存使用率") : QStringLiteral("利用率"));
    usageMetric_->setCaption(memory ? QStringLiteral("内存使用率") : QStringLiteral("利用率"));
    usageMetric_->setValue(formatMetric(sample->utilization, QStringLiteral(" %"), 1),
        sample->utilization.value ? sample->utilization.source : sample->utilization.reason);
    temperatureMetric_->setValue(formatMetric(sample->temperature, QStringLiteral(" °C"), 1),
        sample->temperature.value ? sample->temperature.source : sample->temperature.reason);
    extraMetric_->show();
    if (sample->kind == ResourceKind::Cpu) {
        const QString count = sample->details.value(QStringLiteral("逻辑处理器"), QStringLiteral("—"));
        capacity_->setText(count + QStringLiteral(" 个逻辑处理器")); capacityMetric_->setCaption(QStringLiteral("逻辑处理器")); capacityMetric_->setValue(count);
        extraMetric_->setCaption(QStringLiteral("平均频率"));
        extraMetric_->setValue(sample->details.value(QStringLiteral("平均频率"), QStringLiteral("—")));
    } else {
        capacity_->setText(sample->totalBytes ? formatBytes(*sample->totalBytes) + (memory ? QString() : QStringLiteral(" 显存")) : QStringLiteral("容量不可用"));
        capacityMetric_->setCaption(memory ? QStringLiteral("可用内存") : QStringLiteral("显存使用量"));
        capacityMetric_->setValue(capacityValid ? formatBytes(memory ? *sample->totalBytes - *sample->usedBytes : *sample->usedBytes) : QStringLiteral("—"));
        extraMetric_->setCaption(memory ? QStringLiteral("交换空间总量") : QStringLiteral("显存总量"));
        extraMetric_->setValue(memory ? sample->details.value(QStringLiteral("交换空间"), QStringLiteral("—"))
            : (sample->totalBytes ? formatBytes(*sample->totalBytes) : QStringLiteral("—")));
    }
    capacity_->setToolTip(tooltip(capacity_->text()));
    composition_->setVisible(memory);
    if (memory) {
        compositionBar_->setValues(sample->usedBytes, sample->totalBytes);
        usedLabel_->setText(capacityValid ? QStringLiteral("已用 %1（%2%）").arg(formatBytes(*sample->usedBytes),
            QString::number(100.0 * *sample->usedBytes / *sample->totalBytes, 'f', 1)) : QStringLiteral("已用 —"));
        availableLabel_->setText(capacityValid ? QStringLiteral("可用 %1（%2%）").arg(formatBytes(*sample->totalBytes - *sample->usedBytes),
            QString::number(100.0 * (*sample->totalBytes - *sample->usedBytes) / *sample->totalBytes, 'f', 1)) : QStringLiteral("可用 —"));
        compositionNotice_->setText(!capacityValid ? QStringLiteral("无法取得有效容量，组成比例不可用。")
            : (sample->details.value(QStringLiteral("统计方式")).contains(QStringLiteral("估算"))
                ? QStringLiteral("可用内存按旧内核字段估算，包含可回收缓存。")
                : QStringLiteral("可用内存包含可回收缓存。")));
    }
    QStringList information;
    QSet<QString> summaryKeys;
    if (sample->kind == ResourceKind::Cpu) summaryKeys = QSet<QString>{QStringLiteral("逻辑处理器"), QStringLiteral("平均频率")};
    if (memory) summaryKeys = QSet<QString>{QStringLiteral("可用内存"), QStringLiteral("交换空间")};
    for (auto it = sample->details.cbegin(); it != sample->details.cend(); ++it)
        if (!summaryKeys.contains(it.key())) information.append(it.key() + QStringLiteral("：") + it.value());
    details_->setText(information.join(QLatin1Char('\n'))); details_->setVisible(!information.isEmpty());
    details_->setToolTip(tooltip(details_->text()));
    notice_->setText(sample->utilization.value ? QString() : sample->utilization.reason);
    notice_->setVisible(!notice_->text().isEmpty());
    bool hasTemperature = sample->temperature.value.has_value();
    for (const auto &point : model_->history(resourceId_)) hasTemperature = hasTemperature || point.temperature.has_value();
    temperatureChart_->setVisible(hasTemperature); temperatureHeading_->setVisible(hasTemperature);
    temperatureNotice_->setText(sample->temperature.value ? QStringLiteral("温度取已识别传感器的最高有效读数。")
        : QStringLiteral("温度不可用：") + sample->temperature.reason);
    environment_->setText(model_->snapshot().isWsl ? QStringLiteral("WSL 2 · 读数反映 wsl。") : QString());
    environment_->setVisible(!environment_->text().isEmpty());
    const auto time = model_->snapshot().timestamp;
    timestamp_->setText(time.isValid() ? QStringLiteral("更新于 ") + time.toLocalTime().toString(QStringLiteral("HH:mm:ss")) : QStringLiteral("等待首次采样"));
    scheduleLayoutUpdate();
}
void PerformancePage::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); scheduleLayoutUpdate(); }
void PerformancePage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange || event->type() == QEvent::ApplicationFontChange) scheduleLayoutUpdate();
}
bool PerformancePage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == scroll_->viewport() && event->type() == QEvent::Resize) scheduleLayoutUpdate();
    return QWidget::eventFilter(watched, event);
}
void PerformancePage::scheduleLayoutUpdate()
{
    if (layoutUpdatePending_) return;
    layoutUpdatePending_ = true;
    QTimer::singleShot(0, this, [this]() { layoutUpdatePending_ = false; updateLayout(); });
}
void PerformancePage::updateLayout()
{
    const int margin = width() < 650 ? 16 : 26;
    contentLayout_->setContentsMargins(margin, 20, margin, 20);
    const int line = usageChart_->fontMetrics().height();
    const int minimum = qMax(150, line * 6);
    const int preferred = qBound(minimum, scroll_->viewport()->height() * 2 / 5, qMax(minimum, 340));
    usageChart_->setFixedHeight(preferred);
    temperatureChart_->setFixedHeight(qMax(140, line * 6));
}
} // namespace monitor::ui

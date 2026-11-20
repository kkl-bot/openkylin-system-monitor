#include "ui/detail_window.h"
#include "ui/history_chart.h"
#include "ui/resource_tile.h"
#include "ui/process_page.h"
#include "ui/network_page.h"
#include "ui/flow_layout.h"
#include "ui/typography.h"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QShowEvent>
#include <QHideEvent>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QResizeEvent>

namespace monitor::ui {
namespace {
QLabel *label(const QString &text, const QString &name, QWidget *parent = nullptr)
{
    auto *result = new QLabel(text, parent);
    result->setObjectName(name);
    if (name == "brand" || name == "pageTitle") result->setProperty("textRole", "title");
    else if (name == "cardValue") result->setProperty("textRole", "metric");
    else if (name == "sectionTitle") result->setProperty("textRole", "section");
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    return result;
}
QFrame *metricCard(const QString &text, QLabel **value, QLabel **caption = nullptr)
{
    auto *card = new QFrame;
    card->setObjectName("metricCard");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 13, 18, 14);
    auto *title = label(text, "cardLabel");
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    if (caption) *caption = title;
    layout->addWidget(title);
    *value = label(QStringLiteral("—"), "cardValue");
    (*value)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(*value);
    return card;
}
} // namespace

DetailWindow::DetailWindow(MonitorModel *model, QWidget *parent) : QWidget(parent), model_(model)
{
    setObjectName("detailWindow");
    setWindowTitle(QStringLiteral("系统监测 · Kylin Monitor"));
    resize(1180, 780);
    setMinimumSize(760, 500);
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto *sidebar = new QFrame; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(232);
    sidebar_ = sidebar;
    auto *side = new QVBoxLayout(sidebar); side->setContentsMargins(16, 30, 16, 24); side->setSpacing(8);
    brand_ = label(QStringLiteral("任务管理器"), "brand");
    side->addWidget(brand_);
    side->addSpacing(20);
    processNavigation_ = new QPushButton(QStringLiteral("进程"));
    processNavigation_->setObjectName("processNavigation");
    processNavigation_->setCheckable(true);
    side->addWidget(processNavigation_);
    networkNavigation_ = new QPushButton(QStringLiteral("网络"));
    networkNavigation_->setObjectName("networkNavigation");
    networkNavigation_->setCheckable(true);
    side->addWidget(networkNavigation_);
    side->addSpacing(12);
    side->addWidget(label(QStringLiteral("资源概览"), "muted"));
    side->addSpacing(8);
    auto *resourceHost = new QWidget; resourceHost->setObjectName("resourceHost");
    auto *resourceContent = new QVBoxLayout(resourceHost); resourceContent->setContentsMargins(0, 0, 0, 0);
    resourceContent->setSizeConstraint(QLayout::SetMinAndMaxSize);
    auto *resourceCards = new QWidget;
    resourcesLayout_ = new QVBoxLayout(resourceCards); resourcesLayout_->setContentsMargins(0, 0, 0, 0);
    resourcesLayout_->setSpacing(8); resourcesLayout_->addStretch();
    resourceContent->addWidget(resourceCards, 1);
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(resourceHost);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    side->addWidget(scroll, 1);
    environment_ = label("openKylin · Linux", "muted"); environment_->setWordWrap(true);
    resourceContent->addWidget(environment_);
    root->addWidget(sidebar);

    pages_ = new QStackedWidget;
    auto *performance = new QWidget;
    performance->setObjectName("performanceContent");
    auto *main = new QVBoxLayout(performance); main->setContentsMargins(32, 28, 30, 20); main->setSpacing(14);
    performanceLayout_ = main;
    main->setSizeConstraint(QLayout::SetMinAndMaxSize);
    auto *top = new QHBoxLayout;
    title_ = label("CPU", "pageTitle"); top->addWidget(title_); top->addStretch();
    title_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    status_ = label(QStringLiteral("●  实时监测"), "live"); top->addWidget(status_); top->addSpacing(12);
    status_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *mini = new QPushButton(QStringLiteral("极简模式 ↗")); mini->setObjectName("primary");
    mini->setToolTip(QStringLiteral("切换为仅显示柱状图的桌面悬浮窗")); top->addWidget(mini);
    main->addLayout(top);
    subtitle_ = label(QStringLiteral("正在读取处理器信息…"), "subtitle"); subtitle_->setWordWrap(true); main->addWidget(subtitle_);
    subtitle_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    main->addSpacing(4);
    cardsLayout_ = new QGridLayout; cardsLayout_->setSpacing(12);
    cards_ = {metricCard(QStringLiteral("利用率"), &usage_),
              metricCard(QStringLiteral("温度 · 最高传感器读数"), &temperature_),
              metricCard(QStringLiteral("逻辑处理器"), &capacity_, &capacityLabel_)};
    for (int i = 0; i < cards_.size(); ++i) cardsLayout_->addWidget(cards_[i], 0, i);
    main->addLayout(cardsLayout_);
    auto *chartHeading = new QHBoxLayout;
    chartHeading->addWidget(label(QStringLiteral("利用率"), "sectionTitle")); chartHeading->addStretch();
    chartHeading->addWidget(label(QStringLiteral("最近 60 秒"), "muted")); main->addLayout(chartHeading);
    usageChart_ = new HistoryChart(model, false); main->addWidget(usageChart_, 3);
    main->addWidget(label(QStringLiteral("温度"), "sectionTitle"));
    temperatureChart_ = new HistoryChart(model, true); main->addWidget(temperatureChart_, 2);
    notice_ = label({}, "notice"); notice_->setWordWrap(true); notice_->setMinimumHeight(32); main->addWidget(notice_);
    details_ = label({}, "muted"); details_->setWordWrap(true); main->addWidget(details_);
    notice_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    details_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *line = new QFrame; line->setFrameShape(QFrame::HLine); main->addWidget(line);
    auto *bottom = new FlowLayout;
    timestamp_ = label(QStringLiteral("等待首次采样"), "muted"); bottom->addWidget(timestamp_);
    bottom->addWidget(label(QStringLiteral("刷新"), "muted"));
    interval_ = new QComboBox;
    interval_->setObjectName("performanceInterval");
    interval_->addItem(QStringLiteral("0.5 秒"), 500); interval_->addItem(QStringLiteral("1 秒"), 1000);
    interval_->addItem(QStringLiteral("2 秒"), 2000); interval_->addItem(QStringLiteral("5 秒"), 5000);
    interval_->setCurrentIndex(1); bottom->addWidget(interval_);
    pause_ = new QPushButton(QStringLiteral("暂停")); pause_->setCheckable(true); bottom->addWidget(pause_);
    pause_->setObjectName("performancePause");
    main->addLayout(bottom);
    performanceScroll_ = new QScrollArea;
    performanceScroll_->setObjectName("performanceScroll");
    performanceScroll_->setWidgetResizable(true);
    performanceScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    performanceScroll_->setWidget(performance);
    pages_->addWidget(performanceScroll_);
    processPage_ = new ProcessPage;
    pages_->addWidget(processPage_);
    networkPage_ = new NetworkPage;
    pages_->addWidget(networkPage_);
    root->addWidget(pages_, 1);
    connect(processNavigation_, &QPushButton::clicked, this, &DetailWindow::showProcesses);
    connect(networkNavigation_, &QPushButton::clicked, this, &DetailWindow::showNetwork);
    connect(pages_, &QStackedWidget::currentChanged, this, &DetailWindow::updateActivePage);
    connect(processPage_, &ProcessPage::pauseRequested, this, &DetailWindow::pauseRequested);
    connect(processPage_, &ProcessPage::intervalRequested, this, &DetailWindow::intervalRequested);
    connect(networkPage_, &NetworkPage::pauseRequested, this, &DetailWindow::pauseRequested);
    connect(networkPage_, &NetworkPage::intervalRequested, this, &DetailWindow::intervalRequested);
    connect(model_, &MonitorModel::changed, this, &DetailWindow::refresh);
    connect(mini, &QPushButton::clicked, this, &DetailWindow::miniRequested);
    connect(pause_, &QPushButton::toggled, this, &DetailWindow::pauseRequested);
    connect(interval_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { emit intervalRequested(interval_->currentData().toInt()); });
    enableWindowTypography(this);
    refresh();
    updateLayout();
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
        // UKUI can assign its own button font when a new device appears after the window is shown.
        tile->setFont(font());
        connect(tile, &QPushButton::clicked, this, [this, id]() { selectResource(id); });
    }
    updateLayout();
}
void DetailWindow::selectResource(const QString &id) { selectedId_ = id; pages_->setCurrentIndex(0); refresh(); }
void DetailWindow::showProcesses() { pages_->setCurrentWidget(processPage_); updateActivePage(); }
bool DetailWindow::showingProcesses() const { return pages_->currentWidget() == processPage_; }
ProcessPage *DetailWindow::processPage() const { return processPage_; }
void DetailWindow::showNetwork() { pages_->setCurrentWidget(networkPage_); updateActivePage(); }
bool DetailWindow::showingNetwork() const { return pages_->currentWidget() == networkPage_; }
NetworkPage *DetailWindow::networkPage() const { return networkPage_; }
void DetailWindow::updateActivePage()
{
    processNavigation_->setChecked(showingProcesses());
    networkNavigation_->setChecked(showingNetwork());
    for (auto *tile : tiles_) {
        tile->setAutoExclusive(false);
        tile->setChecked(pages_->currentIndex() == 0 && tile->resourceId() == selectedId_);
        tile->setAutoExclusive(true);
    }
    emit processPageActiveChanged(isVisible() && !isMinimized() && showingProcesses());
    emit networkPageActiveChanged(isVisible() && !isMinimized() && showingNetwork());
}
void DetailWindow::refresh()
{
    rebuildResources();
    const auto *sample = model_->resource(selectedId_);
    if (!sample) return;
    for (auto *tile : tiles_) {
        tile->setAutoExclusive(false);
        tile->setChecked(pages_->currentIndex() == 0 && tile->resourceId() == selectedId_);
        tile->setAutoExclusive(true);
    }
    title_->setText(resourceLabel(sample->kind)); subtitle_->setText(sample->name);
    usage_->setText(formatMetric(sample->utilization, "%", 1));
    temperature_->setText(formatMetric(sample->temperature, " °C", 1));
    usage_->setToolTip((sample->utilization.value ? sample->utilization.source : sample->utilization.reason).toHtmlEscaped());
    temperature_->setToolTip((sample->temperature.value ? sample->temperature.source : sample->temperature.reason).toHtmlEscaped());
    if (sample->kind == ResourceKind::Cpu) {
        capacityLabel_->setText(QStringLiteral("逻辑处理器")); capacity_->setText(sample->details.value(QStringLiteral("逻辑处理器"), "—"));
    } else {
        capacityLabel_->setText(sample->kind == ResourceKind::Gpu ? QStringLiteral("显存占用 / 总量") : QStringLiteral("已用 / 总内存"));
        capacity_->setText(sample->usedBytes && sample->totalBytes ? formatBytes(*sample->usedBytes) + " / " + formatBytes(*sample->totalBytes) : QStringLiteral("—"));
    }
    QStringList notes;
    if (!sample->utilization.value) notes.append(sample->utilization.reason);
    if (!sample->temperature.value && !notes.contains(sample->temperature.reason)) notes.append(sample->temperature.reason);
    notice_->setText(notes.isEmpty() ? QStringLiteral("温度取已识别传感器的最高有效值。") : notes.join(QStringLiteral("；")));
    QStringList info;
    for (auto it = sample->details.cbegin(); it != sample->details.cend(); ++it) {
        if (it.key() != QStringLiteral("设备标识")) info.append(it.key() + "  " + it.value());
    }
    details_->setText(info.join(QStringLiteral("    ·    ")));
    subtitle_->setToolTip(sample->name.toHtmlEscaped());
    details_->setToolTip(details_->text().toHtmlEscaped());
    capacity_->setToolTip(capacity_->text().toHtmlEscaped());
    environment_->setText(model_->snapshot().isWsl ? QStringLiteral("openKylin · WSL 2\nCPU / 内存反映 WSL 环境") : QStringLiteral("openKylin · Linux"));
    timestamp_->setText(QStringLiteral("更新于 ") + model_->snapshot().timestamp.toLocalTime().toString("HH:mm:ss"));
    usageChart_->setResource(selectedId_); temperatureChart_->setResource(selectedId_);
}
void DetailWindow::setPaused(bool paused)
{
    processPage_->setPaused(paused);
    networkPage_->setPaused(paused);
    const QSignalBlocker blocker(pause_); pause_->setChecked(paused);
    pause_->setText(paused ? QStringLiteral("继续") : QStringLiteral("暂停"));
    status_->setText(paused ? QStringLiteral("Ⅱ  已暂停") : QStringLiteral("●  实时监测"));
}
void DetailWindow::setInterval(int milliseconds)
{
    processPage_->setInterval(milliseconds);
    networkPage_->setInterval(milliseconds);
    const QSignalBlocker blocker(interval_); interval_->setCurrentIndex(interval_->findData(milliseconds));
}
void DetailWindow::closeEvent(QCloseEvent *event) { event->accept(); qApp->quit(); }
void DetailWindow::showEvent(QShowEvent *event) { QWidget::showEvent(event); updateActivePage(); }
void DetailWindow::hideEvent(QHideEvent *event) { QWidget::hideEvent(event); updateActivePage(); }
void DetailWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) updateActivePage();
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) updateLayout();
}
void DetailWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}
void DetailWindow::updateLayout()
{
    if (!sidebar_ || !performanceLayout_ || !cardsLayout_) return;
    int sideWidth = width() < 1000 ? 200 : 232;
    const int preferredLimit = width() / 3;
    sideWidth = qMax(sideWidth, qMin(preferredLimit, brand_->fontMetrics().horizontalAdvance(brand_->text()) + 36));
    for (auto *tile : tiles_) {
        sideWidth = qMax(sideWidth, tile->minimumSizeHint().width() + 48);
        sideWidth = qMax(sideWidth, qMin(preferredLimit, tile->sizeHint().width() + 48));
    }
    sidebar_->setFixedWidth(sideWidth);
    const int margin = width() < 1000 ? 16 : 30;
    performanceLayout_->setContentsMargins(margin, 24, margin, 20);
    const int available = width() - sideWidth - margin * 2 - 20;
    // Measure a useful card width in the current font instead of assuming a DPI.
    const int cardWidth = qMax(190, fontMetrics().horizontalAdvance(QStringLiteral("温度 · 最高传感器读数")) + 40);
    const int columns = available >= cardWidth * 3 + 24 ? 3 : available >= cardWidth * 2 + 12 ? 2 : 1;
    if (columns != cardColumns_) {
        for (auto *card : cards_) cardsLayout_->removeWidget(card);
        for (int column = 0; column < 3; ++column) cardsLayout_->setColumnStretch(column, column < columns ? 1 : 0);
        for (int i = 0; i < cards_.size(); ++i)
            cardsLayout_->addWidget(cards_[i], i / columns, i % columns, 1, columns == 2 && i == 2 ? 2 : 1);
        cardColumns_ = columns;
    }
}
} // namespace monitor::ui

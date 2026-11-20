#include "ui/mini_window.h"
#include "ui/theme.h"
#include "ui/typography.h"
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTextDocument>

namespace monitor::ui {
namespace {
QPoint globalPosition(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}
} // namespace

MiniWindow::MiniWindow(MonitorModel *model, QWidget *parent) : QWidget(parent), model_(model)
{
    setWindowTitle(QStringLiteral("Kylin Monitor · 极简模式"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(178, 108);
    setCursor(Qt::OpenHandCursor);
    setAccessibleName(QStringLiteral("资源柱状图；从左到右 CPU、GPU、内存；每组利用率和温度。双击打开详情。"));
    connect(model_, &MonitorModel::changed, this, [this]() { refreshTooltip(); update(); });
    refreshTooltip();
}
QVector<const ResourceSample *> MiniWindow::displayedResources() const
{
    const ResourceSample *gpu = nullptr;
    for (const auto &sample : model_->snapshot().resources) {
        if (sample.kind != ResourceKind::Gpu) continue;
        if (!gpu || sample.id == gpuId_) gpu = &sample;
    }
    return {model_->resource("cpu"), gpu, model_->resource("memory")};
}
void MiniWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    const auto samples = displayedResources();
    for (int group = 0; group < samples.size(); ++group) {
        const auto *sample = samples[group];
        const auto kind = group == 0 ? ResourceKind::Cpu : group == 1 ? ResourceKind::Gpu : ResourceKind::Memory;
        for (int column = 0; column < 2; ++column) {
            const QRectF track(12 + group * 56 + column * 20, 10, 14, 88);
            const auto color = resourceColor(kind);
            QColor background = color; background.setAlpha(42);
            painter.setPen(QPen(QColor(255, 255, 255, 100), 1)); painter.setBrush(background);
            painter.drawRoundedRect(track, 5, 5);
            const auto value = sample ? (column == 0 ? sample->utilization.value : sample->temperature.value) : std::optional<double>{};
            painter.setPen(Qt::NoPen);
            if (value) {
                const double ratio = qBound(0.0, *value / (column == 0 ? 100.0 : 120.0), 1.0);
                QColor fill = color; fill.setAlpha(paused_ ? 100 : column == 0 ? 255 : 155);
                painter.setBrush(fill);
                if (ratio > 0) painter.drawRoundedRect(QRectF(track.left(), track.bottom() - ratio * track.height(), track.width(), ratio * track.height()), 4, 4);
            } else {
                QColor hatch = color; hatch.setAlpha(100);
                painter.setBrush(QBrush(hatch, Qt::BDiagPattern)); painter.drawRoundedRect(track, 5, 5);
            }
        }
    }
}
void MiniWindow::refreshTooltip()
{
    QStringList lines;
    lines.append(QStringLiteral("左→右：CPU / GPU / 内存；每组：利用率 / 温度（0–120 °C）"));
    for (const auto *sample : displayedResources()) {
        if (!sample) continue;
        lines.append(resourceLabel(sample->kind) + " · " + sample->name + "\n" +
                     formatMetric(sample->utilization, "%", 1) + "  /  " + formatMetric(sample->temperature, " °C", 1));
    }
    lines.append(QStringLiteral("斜纹 = 无可用读数；拖动移动，双击详情，右键设置"));
    if (paused_) lines.append(QStringLiteral("采样已暂停，显示最后一次读数"));
    setToolTip(Qt::convertFromPlainText(lines.join('\n')));
}
void MiniWindow::setPaused(bool paused) { paused_ = paused; refreshTooltip(); update(); }
void MiniWindow::setInterval(int milliseconds) { interval_ = milliseconds; }
void MiniWindow::setAlwaysOnTop(bool enabled)
{
    alwaysOnTop_ = enabled;
    const bool visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    if (visible) show();
}
bool MiniWindow::alwaysOnTop() const { return alwaysOnTop_; }
void MiniWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = true; dragOffset_ = globalPosition(event) - frameGeometry().topLeft(); setCursor(Qt::ClosedHandCursor);
    }
}
void MiniWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!dragging_ || !(event->buttons() & Qt::LeftButton)) return;
    QPoint target = globalPosition(event) - dragOffset_;
    auto *screen = QGuiApplication::screenAt(globalPosition(event));
    if (screen) {
        const auto bounds = screen->availableGeometry();
        target.setX(qBound(bounds.left(), target.x(), qMax(bounds.left(), bounds.right() - width() + 1)));
        target.setY(qBound(bounds.top(), target.y(), qMax(bounds.top(), bounds.bottom() - height() + 1)));
    }
    move(target);
}
void MiniWindow::mouseReleaseEvent(QMouseEvent *) { dragging_ = false; setCursor(Qt::OpenHandCursor); }
void MiniWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) { dragging_ = false; setCursor(Qt::OpenHandCursor); emit detailRequested(); }
}
void MiniWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Return) emit detailRequested();
    else QWidget::keyPressEvent(event);
}
void MiniWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    const QFont menuFont = typographyFont(font(), TextRole::Body);
    menu.setFont(menuFont);
    menu.addAction(QStringLiteral("打开详细模式"), this, &MiniWindow::detailRequested);
    menu.addAction(paused_ ? QStringLiteral("继续采样") : QStringLiteral("暂停采样"), this, [this]() { emit pauseRequested(!paused_); });
    auto *top = menu.addAction(QStringLiteral("保持置顶")); top->setCheckable(true); top->setChecked(alwaysOnTop_);
    connect(top, &QAction::toggled, this, &MiniWindow::setAlwaysOnTop);
    auto *refresh = menu.addMenu(QStringLiteral("刷新间隔"));
    refresh->setFont(menuFont);
    auto *intervalGroup = new QActionGroup(&menu);
    for (const int milliseconds : {500, 1000, 2000, 5000}) {
        auto *action = refresh->addAction(QString::number(milliseconds / 1000.0) + QStringLiteral(" 秒"));
        action->setCheckable(true); action->setChecked(interval_ == milliseconds); intervalGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, milliseconds]() { emit intervalRequested(milliseconds); });
    }
    QVector<const ResourceSample *> gpus;
    for (const auto &sample : model_->snapshot().resources) if (sample.kind == ResourceKind::Gpu) gpus.append(&sample);
    if (gpus.size() > 1) {
        auto *devices = menu.addMenu(QStringLiteral("悬浮窗显示的 GPU"));
        devices->setFont(menuFont);
        auto *group = new QActionGroup(&menu);
        const auto *displayed = displayedResources()[1];
        for (const auto *gpu : gpus) {
            auto *action = devices->addAction(gpu->name); action->setCheckable(true); group->addAction(action);
            action->setChecked(displayed && displayed->id == gpu->id);
            const auto id = gpu->id;
            connect(action, &QAction::triggered, this, [this, id]() { gpuId_ = id; refreshTooltip(); update(); });
        }
    }
    menu.addSeparator(); menu.addAction(QStringLiteral("退出"), this, &MiniWindow::quitRequested);
    menu.exec(event->globalPos());
}
} // namespace monitor::ui

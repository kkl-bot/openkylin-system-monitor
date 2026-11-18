#include "ui/history_chart.h"
#include "ui/theme.h"
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QTextOption>
#include <cmath>

namespace monitor::ui {
namespace {
struct ChartLayout {
    QFont axisFont;
    QStringList labels;
    int labelHeight = 0;
    int labelWidth = 0;
    int gap = 0;
    int padding = 0;
    int top = 0;
    int bottom = 0;
    int right = 0;
    int minimumPlotWidth = 0;
};

ChartLayout measureChart(const QFont &font, const QPaintDevice *device, double maximum, bool temperature)
{
    ChartLayout layout;
    layout.axisFont = font;
    if (font.pointSizeF() > 0) layout.axisFont.setPointSizeF(font.pointSizeF() * 0.85);
    else if (font.pixelSize() > 0) layout.axisFont.setPixelSize(qMax(1, qRound(font.pixelSize() * 0.85)));
    const QFontMetrics metrics(layout.axisFont, device);
    layout.labelHeight = metrics.height();
    layout.gap = qMax(4, metrics.height() / 4);
    layout.padding = qMax(2, metrics.height() / 8);
    for (int i = 0; i <= 4; ++i) {
        const QString text = QString::number(maximum * (4 - i) / 4, 'f', 0) + (temperature ? "°" : "%");
        layout.labels.append(text);
        layout.labelWidth = qMax(layout.labelWidth, metrics.horizontalAdvance(text) + 2);
    }
    layout.top = (layout.labelHeight + 1) / 2 + layout.padding;
    layout.bottom = layout.gap + layout.labelHeight + layout.padding;
    layout.right = layout.gap + layout.labelWidth + layout.padding;
    layout.minimumPlotWidth = qMax(metrics.horizontalAdvance('0') * 8,
        metrics.horizontalAdvance(QStringLiteral("60 秒前")) + metrics.horizontalAdvance(QStringLiteral("现在")) + layout.gap);
    return layout;
}

void drawWrappedMessage(QPainter &painter, const QRectF &bounds, const QString &text)
{
    if (bounds.width() <= 0 || bounds.height() <= 0) return;
    QTextOption options(Qt::AlignCenter);
    options.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    painter.save();
    painter.setClipRect(bounds);
    painter.drawText(bounds, text, options);
    painter.restore();
}
} // namespace

HistoryChart::HistoryChart(MonitorModel *model, bool temperature, QWidget *parent)
    : QWidget(parent), model_(model), temperature_(temperature)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(model_, &MonitorModel::changed, this, [this]() { updateGeometry(); update(); });
}
void HistoryChart::setResource(const QString &id)
{
    if (resourceId_ == id) return;
    resourceId_ = id;
    updateGeometry();
    update();
}
double HistoryChart::axisMaximum() const
{
    double maximum = temperature_ ? 120.0 : 100.0;
    if (temperature_) {
        for (const auto &point : model_->history(resourceId_)) {
            if (point.temperature && std::isfinite(*point.temperature))
                maximum = qMax(maximum, std::ceil(*point.temperature / 20.0) * 20.0);
        }
    }
    return maximum;
}
QSize HistoryChart::sizeHint() const
{
    const auto layout = measureChart(font(), this, axisMaximum(), temperature_);
    return {qMax(layout.minimumPlotWidth, layout.labelHeight * 22) + layout.padding + layout.right,
            layout.top + layout.bottom + layout.labelHeight * (temperature_ ? 6 : 10)};
}
QSize HistoryChart::minimumSizeHint() const
{
    const auto layout = measureChart(font(), this, axisMaximum(), temperature_);
    return {layout.padding + layout.minimumPlotWidth + layout.right,
            layout.top + layout.bottom + (layout.labelHeight + layout.gap) * 4};
}
void HistoryChart::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange
        || event->type() == QEvent::StyleChange) {
        updateGeometry();
        update();
    }
}
void HistoryChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto *sample = model_->resource(resourceId_);
    const auto points = model_->history(resourceId_);
    const double maximum = axisMaximum();
    const auto layout = measureChart(font(), this, maximum, temperature_);
    const QRectF plot(layout.padding, layout.top, width() - layout.padding - layout.right,
                      height() - layout.top - layout.bottom);
    const auto current = sample ? (temperature_ ? sample->temperature : sample->utilization) : Metric{};
    const QString message = sample ? QStringLiteral("暂无可用读数") : QStringLiteral("正在读取系统信息…");
    if (plot.width() <= 0 || plot.height() <= 0) {
        if (!current.value) {
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.setFont(font());
            drawWrappedMessage(painter, rect().adjusted(layout.padding, layout.padding, -layout.padding, -layout.padding), message);
        }
        return;
    }
    const QColor color = sample ? resourceColor(sample->kind) : QColor("#2979e8");
    painter.fillRect(plot, palette().color(QPalette::Base));
    QColor grid = palette().color(QPalette::Mid); grid.setAlpha(85);
    painter.setPen(QPen(grid, 1));
    for (int i = 0; i <= 6; ++i) {
        const double x = plot.left() + i * plot.width() / 6.0;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    painter.setFont(layout.axisFont);
    const int labelStride = plot.height() / 4 >= layout.labelHeight + layout.gap ? 1
        : plot.height() / 2 >= layout.labelHeight + layout.gap ? 2 : 4;
    for (int i = 0; i <= 4; ++i) {
        const double y = plot.top() + i * plot.height() / 4.0;
        painter.setPen(QPen(grid, 1));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        if (i % labelStride == 0 && (i == 0 || plot.height() >= layout.labelHeight + layout.gap)) {
            painter.setPen(palette().color(QPalette::PlaceholderText));
            painter.drawText(QRectF(plot.right() + layout.gap, y - layout.labelHeight / 2.0,
                                    layout.labelWidth, layout.labelHeight),
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, layout.labels[i]);
        }
    }
    painter.setPen(palette().color(QPalette::PlaceholderText));
    const int timeWidth = qMax(0, int((plot.width() - layout.gap) / 2));
    painter.drawText(QRectF(plot.left(), plot.bottom() + layout.gap, timeWidth, layout.labelHeight),
                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     painter.fontMetrics().elidedText(QStringLiteral("60 秒前"), Qt::ElideRight, timeWidth));
    painter.drawText(QRectF(plot.right() - timeWidth, plot.bottom() + layout.gap, timeWidth, layout.labelHeight),
                     Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
                     painter.fontMetrics().elidedText(QStringLiteral("现在"), Qt::ElideLeft, timeWidth));
    painter.save();
    painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
    QVector<QPointF> segment;
    const auto flush = [&]() {
        if (segment.isEmpty()) return;
        QPainterPath line; line.moveTo(segment.first());
        for (int i = 1; i < segment.size(); ++i) line.lineTo(segment[i]);
        QPainterPath fill = line;
        fill.lineTo(segment.last().x(), plot.bottom());
        fill.lineTo(segment.first().x(), plot.bottom()); fill.closeSubpath();
        QColor translucent = color; translucent.setAlpha(25);
        painter.fillPath(fill, translucent);
        painter.setPen(QPen(color, 2)); painter.drawPath(line);
        if (segment.size() == 1) { painter.setBrush(color); painter.drawEllipse(segment.first(), 2, 2); }
        segment.clear();
    };
    QDateTime previous;
    for (const auto &point : points) {
        const auto value = temperature_ ? point.temperature : point.utilization;
        if (!value || (previous.isValid() && previous.msecsTo(point.timestamp) > 6500)) flush();
        if (value) {
            const auto age = point.timestamp.msecsTo(model_->snapshot().timestamp);
            segment.append({plot.right() - age / 60000.0 * plot.width(),
                            plot.bottom() - qBound(0.0, *value / maximum, 1.0) * plot.height()});
        }
        previous = point.timestamp;
    }
    flush();
    painter.restore();
    if (!current.value) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.setFont(font());
        const QRectF messageBounds = plot.adjusted(layout.gap, layout.gap, -layout.gap, -layout.gap);
        drawWrappedMessage(painter, messageBounds, message);
    }
}
} // namespace monitor::ui

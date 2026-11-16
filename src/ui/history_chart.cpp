#include "ui/history_chart.h"
#include "ui/theme.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace monitor::ui {
HistoryChart::HistoryChart(MonitorModel *model, bool temperature, QWidget *parent)
    : QWidget(parent), model_(model), temperature_(temperature)
{
    setMinimumHeight(temperature ? 120 : 190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(model_, &MonitorModel::changed, this, QOverload<>::of(&HistoryChart::update));
}
void HistoryChart::setResource(const QString &id) { resourceId_ = id; update(); }
void HistoryChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF plot(1, 16, width() - 48, height() - 39);
    const auto *sample = model_->resource(resourceId_);
    const auto points = model_->history(resourceId_);
    double maximum = temperature_ ? 120.0 : 100.0;
    if (temperature_) for (const auto &point : points)
        if (point.temperature) maximum = qMax(maximum, std::ceil(*point.temperature / 20.0) * 20.0);
    const QColor color = sample ? resourceColor(sample->kind) : QColor("#2979e8");
    painter.fillRect(plot, palette().color(QPalette::Base));
    QColor grid = palette().color(QPalette::Mid); grid.setAlpha(85);
    painter.setPen(QPen(grid, 1));
    for (int i = 0; i <= 6; ++i) {
        const double x = plot.left() + i * plot.width() / 6.0;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    QFont small = font(); small.setPointSize(8); painter.setFont(small);
    for (int i = 0; i <= 4; ++i) {
        const double y = plot.top() + i * plot.height() / 4.0;
        painter.setPen(QPen(grid, 1));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(QRectF(plot.right() + 8, y - 8, 38, 16), Qt::AlignLeft | Qt::AlignVCenter,
                         QString::number(maximum * (4 - i) / 4, 'f', 0) + (temperature_ ? "°" : "%"));
    }
    painter.drawText(QRectF(plot.left(), plot.bottom() + 7, 120, 16), Qt::AlignLeft, QStringLiteral("60 秒前"));
    painter.drawText(QRectF(plot.right() - 70, plot.bottom() + 7, 70, 16), Qt::AlignRight, QStringLiteral("现在"));
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
    const auto current = sample ? (temperature_ ? sample->temperature : sample->utilization) : Metric{};
    if (!current.value) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.setFont(font());
        painter.drawText(plot, Qt::AlignCenter, sample ? QStringLiteral("暂无可用读数") : QStringLiteral("正在读取系统信息…"));
    }
}
} // namespace monitor::ui

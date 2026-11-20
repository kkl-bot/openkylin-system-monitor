// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_history_chart.h"
#include "ui/network_presentation.h"
#include "ui/paint_palette.h"
#include "ui/theme.h"
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QSet>
#include <QTextOption>
#include <QVariant>
#include <cmath>

namespace monitor::ui {
namespace {
std::optional<double> validRate(const std::optional<double> &value)
{
    return value && std::isfinite(*value) && *value >= 0 ? value : std::nullopt;
}
}
NetworkHistoryChart::NetworkHistoryChart(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("networkHistoryChart"));
    setProperty("textRole", "caption");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
}
void NetworkHistoryChart::acceptSnapshot(const NetworkSnapshot &snapshot)
{
    if (!snapshot.timestamp.isValid()) return;
    if (timestamp_.isValid() && snapshot.timestamp <= timestamp_) series_.clear();
    timestamp_ = snapshot.timestamp;
    const auto cutoff = timestamp_.addSecs(-60);
    QSet<QString> present;
    for (const auto &interface : snapshot.interfaces) {
        present.insert(interface.name);
        auto &series = series_[interface.name];
        if (series.index != interface.index) series.points.clear();
        series.index = interface.index; series.lastSeen = timestamp_;
        const bool interrupted = breakPending_ || paused_ || (!series.points.isEmpty()
            && series.points.last().timestamp.msecsTo(timestamp_) > qMax(2000, interval_ * 3));
        series.points.append({timestamp_, paused_ ? std::nullopt : validRate(interface.downloadBytesPerSecond),
            paused_ ? std::nullopt : validRate(interface.uploadBytesPerSecond), interrupted});
    }
    for (auto it = series_.begin(); it != series_.end();) {
        if (!present.contains(it.key())) it->points.append({timestamp_, std::nullopt, std::nullopt, true});
        while (!it->points.isEmpty() && it->points.first().timestamp < cutoff) it->points.removeFirst();
        if (it->lastSeen < cutoff) it = series_.erase(it); else ++it;
    }
    breakPending_ = paused_; updateGeometry(); update();
}
void NetworkHistoryChart::setInterface(const QString &name)
{
    if (selectedInterface_ == name) return;
    selectedInterface_ = name; updateGeometry(); update();
}
void NetworkHistoryChart::setPaused(bool paused)
{
    if (paused_ == paused) return;
    paused_ = paused; breakPending_ = true; update();
}
void NetworkHistoryChart::setInterval(int milliseconds)
{
    if (milliseconds == 500 || milliseconds == 1000 || milliseconds == 2000 || milliseconds == 5000) interval_ = milliseconds;
}
QVector<NetworkRatePoint> NetworkHistoryChart::history(const QString &name) const { return series_.value(name).points; }
double NetworkHistoryChart::axisMaximum() const
{
    double maximum = 1024;
    for (const auto &point : series_.value(selectedInterface_).points) {
        if (point.download) maximum = qMax(maximum, *point.download);
        if (point.upload) maximum = qMax(maximum, *point.upload);
    }
    const double rounded = std::pow(2.0, std::ceil(std::log2(maximum)));
    return std::isfinite(rounded) ? rounded : maximum;
}
QSize NetworkHistoryChart::sizeHint() const { return {fontMetrics().height() * 26, fontMetrics().height() * 12}; }
QSize NetworkHistoryChart::minimumSizeHint() const { return {fontMetrics().height() * 14, fontMetrics().height() * 8}; }
void NetworkHistoryChart::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange || event->type() == QEvent::StyleChange) {
        updateGeometry(); update();
    }
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) update();
}
void NetworkHistoryChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font()); const QFontMetrics metrics(font());
    const int gap = qMax(5, metrics.height() / 3), height = metrics.height();
    const double maximum = axisMaximum();
    QStringList ticks; int axisWidth = 0;
    for (int i = 0; i <= 4; ++i) {
        ticks.append(networkRateLabel(maximum * ((4 - i) / 4.0))); axisWidth = qMax(axisWidth, metrics.horizontalAdvance(ticks.last()));
    }
    const QString downloadLegend = QStringLiteral("↓ 下载"), uploadLegend = QStringLiteral("↑ 上传 · 虚线");
    const int legendWidth = qMax(1, width() - gap * 2);
    const int downloadWidth = metrics.horizontalAdvance(downloadLegend);
    const bool stackedLegend = downloadWidth + metrics.horizontalAdvance(uploadLegend) + gap * 2 > legendWidth;
    const int legendHeight = (height + gap) * (stackedLegend ? 2 : 1);
    const QRectF plot(gap, legendHeight + gap, width() - axisWidth - gap * 3,
        QWidget::height() - legendHeight - height - gap * 3);
    const QColor download = chartAccentColor(palette().color(QPalette::Highlight), palette());
    const QColor upload = chartAccentColor(resourceColor(ResourceKind::Gpu), palette());
    painter.setPen(download); painter.drawText(QRect(gap, 0, stackedLegend ? legendWidth : downloadWidth, height + gap), Qt::AlignVCenter, downloadLegend);
    const int uploadX = stackedLegend ? gap : gap + downloadWidth + gap * 2;
    painter.setPen(upload); painter.drawText(QRect(uploadX, stackedLegend ? height + gap : 0,
        width() - uploadX - gap, height + gap), Qt::AlignVCenter, uploadLegend);
    if (plot.width() < gap * 4 || plot.height() < gap * 4) return;
    painter.fillRect(plot, palette().color(QPalette::Base));
    const QColor grid = chartGridColor(palette());
    painter.setPen(QPen(grid, 1));
    for (int i = 0; i <= 6; ++i) {
        const qreal x = plot.left() + plot.width() * i / 6;
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    }
    const int labelStride = plot.height() / 4 >= height + gap ? 1
        : plot.height() / 2 >= height + gap ? 2 : 4;
    for (int i = 0; i <= 4; ++i) {
        const qreal y = plot.top() + plot.height() * i / 4;
        painter.setPen(QPen(grid, 1)); painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        if (i % labelStride == 0) {
            painter.setPen(secondaryPaintText(palette()));
            painter.drawText(QRectF(plot.right() + gap, y - height / 2.0, axisWidth, height), Qt::AlignLeft | Qt::AlignVCenter, ticks[i]);
        }
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(chartBorderColor(palette()), 1));
    painter.drawRect(plot);
    painter.setPen(secondaryPaintText(palette()));
    painter.drawText(QRectF(plot.left(), plot.bottom() + gap, plot.width(), height), Qt::AlignLeft, QStringLiteral("60 秒前"));
    painter.drawText(QRectF(plot.left(), plot.bottom() + gap, plot.width(), height), Qt::AlignRight, paused_ ? QStringLiteral("已暂停") : QStringLiteral("现在"));
    const auto points = series_.value(selectedInterface_).points;
    bool anyValue = false;
    painter.save(); painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
    for (bool receiving : {true, false}) {
        QVector<QPolygonF> segments;
        bool connected = false;
        for (const auto &point : points) {
            const auto value = receiving ? point.download : point.upload;
            if (!value) { connected = false; continue; }
            anyValue = true;
            const double seconds = point.timestamp.msecsTo(timestamp_) / 1000.0;
            const QPointF position(plot.right() - seconds / 60.0 * plot.width(),
                plot.bottom() - qBound(0.0, *value / maximum, 1.0) * plot.height());
            if (!connected || point.breakBefore) segments.append(QPolygonF());
            segments.last().append(position);
            connected = true;
        }
        const QColor color = receiving ? download : upload;
        for (const auto &segment : segments) {
            QPainterPath path;
            path.moveTo(segment.first());
            for (int i = 1; i < segment.size(); ++i) path.lineTo(segment[i]);
            if (receiving) {
                QPainterPath area = path;
                area.lineTo(segment.last().x(), plot.bottom());
                area.lineTo(segment.first().x(), plot.bottom());
                area.closeSubpath();
                painter.fillPath(area, chartFillColor(color, palette()));
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, 2, receiving ? Qt::SolidLine : Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            if (segment.size() == 1) { painter.setBrush(color); painter.drawEllipse(segment.first(), 2, 2); }
        }
    }
    painter.restore();
    if (!anyValue) {
        painter.setPen(secondaryPaintText(palette()));
        QTextOption option(Qt::AlignCenter); option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.drawText(plot.adjusted(gap, gap, -gap, -gap), QStringLiteral("等待连续采样\n未知速率显示为 —"), option);
    }
}
} // namespace monitor::ui

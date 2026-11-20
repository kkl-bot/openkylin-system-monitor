// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_history_chart.h"
#include "ui/network_presentation.h"
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QTextOption>
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
}
void NetworkHistoryChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    QFont axisFont = font();
    if (axisFont.pointSizeF() > 0) axisFont.setPointSizeF(axisFont.pointSizeF() * 0.85);
    else if (axisFont.pixelSize() > 0) axisFont.setPixelSize(qMax(1, qRound(axisFont.pixelSize() * 0.85)));
    painter.setFont(axisFont); const QFontMetrics metrics(axisFont);
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
    const QColor download = palette().color(QPalette::Highlight);
    const QColor upload = palette().color(QPalette::Window).lightness() < 128 ? QColor("#57c9b2") : QColor("#147b67");
    painter.setPen(download); painter.drawText(QRect(gap, 0, stackedLegend ? legendWidth : downloadWidth, height + gap), Qt::AlignVCenter, downloadLegend);
    const int uploadX = stackedLegend ? gap : gap + downloadWidth + gap * 2;
    painter.setPen(upload); painter.drawText(QRect(uploadX, stackedLegend ? height + gap : 0,
        width() - uploadX - gap, height + gap), Qt::AlignVCenter, uploadLegend);
    if (plot.width() < gap * 4 || plot.height() < gap * 4) return;
    painter.fillRect(plot, palette().color(QPalette::Base));
    QColor grid = palette().color(QPalette::Mid); grid.setAlpha(100);
    for (int i = 0; i <= 4; ++i) {
        const qreal y = plot.top() + plot.height() * i / 4;
        painter.setPen(QPen(grid, 1)); painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(QRectF(plot.right() + gap, y - height / 2.0, axisWidth, height), Qt::AlignRight | Qt::AlignVCenter, ticks[i]);
    }
    painter.drawText(QRectF(plot.left(), plot.bottom() + gap, plot.width(), height), Qt::AlignLeft, QStringLiteral("60 秒前"));
    painter.drawText(QRectF(plot.left(), plot.bottom() + gap, plot.width(), height), Qt::AlignRight, paused_ ? QStringLiteral("已暂停") : QStringLiteral("现在"));
    const auto points = series_.value(selectedInterface_).points;
    bool anyValue = false;
    painter.save(); painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
    for (bool receiving : {true, false}) {
        QPainterPath path; bool connected = false;
        for (const auto &point : points) {
            const auto value = receiving ? point.download : point.upload;
            if (!value) { connected = false; continue; }
            anyValue = true;
            const double seconds = point.timestamp.msecsTo(timestamp_) / 1000.0;
            const QPointF position(plot.right() - seconds / 60.0 * plot.width(),
                plot.bottom() - qBound(0.0, *value / maximum, 1.0) * plot.height());
            if (!connected || point.breakBefore) path.moveTo(position); else path.lineTo(position);
            connected = true;
        }
        painter.setPen(QPen(receiving ? download : upload, 2, receiving ? Qt::SolidLine : Qt::DashLine)); painter.drawPath(path);
    }
    painter.restore();
    if (!anyValue) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        QTextOption option(Qt::AlignCenter); option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.drawText(plot.adjusted(gap, gap, -gap, -gap), QStringLiteral("等待连续采样\n未知速率显示为 —"), option);
    }
}
} // namespace monitor::ui

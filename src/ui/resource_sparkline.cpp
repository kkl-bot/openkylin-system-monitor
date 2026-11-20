// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/resource_sparkline.h"
#include "ui/paint_palette.h"
#include "ui/resource_history_geometry.h"
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

namespace monitor::ui {

void paintResourceSparkline(QPainter &painter, const QRectF &frame,
                            const QVector<HistoryPoint> &history, const QDateTime &end,
                            const QColor &color, const QPalette &palette)
{
    if (frame.width() <= 4 || frame.height() <= 4)
        return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor lineColor = chartAccentColor(color, palette);
    painter.fillRect(frame, blendPaintColor(palette.color(QPalette::Base), lineColor, 0.025));
    const QColor outline = blendPaintColor(chartBorderColor(palette), lineColor, 0.32);
    painter.setPen(QPen(outline, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(frame.adjusted(0.5, 0.5, -0.5, -0.5));
    const QRectF plot = frame.adjusted(2, 2, -2, -2);
    const auto segments = resourceHistorySegments(history, end, plot, ResourceHistoryMetric::Utilization, 100.0);
    painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
    for (const auto &segment : segments) {
        if (segment.size() == 1) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(lineColor);
            painter.drawEllipse(segment.first(), 1.5, 1.5);
            continue;
        }
        QPainterPath line;
        line.moveTo(segment.first());
        for (int i = 1; i < segment.size(); ++i)
            line.lineTo(segment[i]);
        QPainterPath area = line;
        area.lineTo(segment.last().x(), plot.bottom());
        area.lineTo(segment.first().x(), plot.bottom());
        area.closeSubpath();
        painter.fillPath(area, chartFillColor(lineColor, palette));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(lineColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(line);
    }
    if (segments.isEmpty()) {
        painter.setPen(secondaryPaintText(palette));
        painter.drawText(plot, Qt::AlignCenter | Qt::TextSingleLine, QStringLiteral("—"));
    }
    painter.restore();
}

} // namespace monitor::ui

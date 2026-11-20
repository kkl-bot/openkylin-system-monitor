// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/resource_history_geometry.h"
#include <cmath>

namespace monitor::ui {

QVector<QPolygonF> resourceHistorySegments(const QVector<HistoryPoint> &history,
                                          const QDateTime &end, const QRectF &plot,
                                          ResourceHistoryMetric metric, double maximum)
{
    QVector<QPolygonF> segments;
    if (!end.isValid() || plot.width() <= 0 || plot.height() <= 0
        || !std::isfinite(maximum) || maximum <= 0)
        return segments;
    QPolygonF current;
    QDateTime previous;
    const auto flush = [&]() {
        if (!current.isEmpty()) segments.append(current);
        current.clear();
    };
    for (const auto &point : history) {
        const auto value = metric == ResourceHistoryMetric::Temperature ? point.temperature : point.utilization;
        const qint64 age = point.timestamp.msecsTo(end);
        if (!point.timestamp.isValid() || age < 0 || age > 60000 || !value || !std::isfinite(*value)
            || (metric == ResourceHistoryMetric::Utilization && *value < 0)) {
            flush();
            previous = {};
            continue;
        }
        if (previous.isValid()) {
            const qint64 elapsed = previous.msecsTo(point.timestamp);
            if (elapsed <= 0 || elapsed > 6500)
                flush();
        }
        current.append(QPointF(plot.right() - age / 60000.0 * plot.width(),
            plot.bottom() - qBound(0.0, *value / maximum, 1.0) * plot.height()));
        previous = point.timestamp;
    }
    flush();
    return segments;
}

} // namespace monitor::ui

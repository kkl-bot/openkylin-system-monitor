// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/monitor_model.h"
#include <QPolygonF>

namespace monitor::ui {

enum class ResourceHistoryMetric { Utilization, Temperature };

QVector<QPolygonF> resourceHistorySegments(const QVector<HistoryPoint> &history,
                                          const QDateTime &end, const QRectF &plot,
                                          ResourceHistoryMetric metric, double maximum);

} // namespace monitor::ui

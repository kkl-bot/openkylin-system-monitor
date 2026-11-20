// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ui/monitor_model.h"
#include <QPolygonF>

class QPainter;
class QPalette;
class QColor;

namespace monitor::ui {

void paintResourceSparkline(QPainter &painter, const QRectF &frame,
                            const QVector<HistoryPoint> &history, const QDateTime &end,
                            const QColor &color, const QPalette &palette);

} // namespace monitor::ui

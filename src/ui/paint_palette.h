// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QColor>

class QPalette;

namespace monitor::ui {
QColor blendPaintColor(const QColor &base, const QColor &tint, qreal amount);
QColor secondaryPaintText(const QPalette &palette);
QColor chartGridColor(const QPalette &palette);
QColor chartBorderColor(const QPalette &palette);
QColor chartAccentColor(const QColor &accent, const QPalette &palette);
QColor chartFillColor(const QColor &accent, const QPalette &palette);
} // namespace monitor::ui

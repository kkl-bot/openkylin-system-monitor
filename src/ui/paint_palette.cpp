// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/paint_palette.h"
#include <QPalette>

namespace monitor::ui {
QColor blendPaintColor(const QColor &base, const QColor &tint, qreal amount)
{
    amount = qBound<qreal>(0, amount, 1);
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
        base.greenF() * (1 - amount) + tint.greenF() * amount,
        base.blueF() * (1 - amount) + tint.blueF() * amount);
}
QColor secondaryPaintText(const QPalette &palette)
{
    return blendPaintColor(palette.color(QPalette::Base), palette.color(QPalette::WindowText), 0.76);
}
QColor chartGridColor(const QPalette &palette)
{
    return blendPaintColor(palette.color(QPalette::Base), palette.color(QPalette::Mid), 0.62);
}
QColor chartBorderColor(const QPalette &palette)
{
    return blendPaintColor(palette.color(QPalette::Base), palette.color(QPalette::Mid), 0.92);
}
QColor chartAccentColor(const QColor &accent, const QPalette &palette)
{
    // Keep resource hues recognizable while improving fine-line contrast on
    // both light and dark surfaces. All neutral tones follow the live palette.
    const bool dark = palette.color(QPalette::Base).lightnessF() < 0.5;
    return blendPaintColor(accent, palette.color(QPalette::WindowText), dark ? 0.20 : 0.24);
}
QColor chartFillColor(const QColor &accent, const QPalette &palette)
{
    QColor fill = accent;
    fill.setAlpha(palette.color(QPalette::Base).lightnessF() < 0.5 ? 40 : 27);
    return fill;
}
} // namespace monitor::ui

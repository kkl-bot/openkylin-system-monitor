// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/process_heat_delegate.h"
#include "ui/process_table_model.h"
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyleOptionFocusRect>
#include <QWidget>
#include <cmath>

namespace monitor::ui {
namespace {
QColor blended(const QColor &base, const QColor &tint, qreal amount)
{
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
                           base.greenF() * (1 - amount) + tint.greenF() * amount,
                           base.blueF() * (1 - amount) + tint.blueF() * amount);
}

qreal luminance(const QColor &color)
{
    const auto linear = [](qreal channel) { return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4); };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

qreal contrast(const QColor &first, const QColor &second)
{
    const qreal a = luminance(first), b = luminance(second);
    return (qMax(a, b) + 0.05) / (qMin(a, b) + 0.05);
}

QColor heatBackground(const QStyleOptionViewItem &option, double load)
{
    const QColor base = option.palette.color(option.features.testFlag(QStyleOptionViewItem::Alternate)
        ? QPalette::AlternateBase : QPalette::Base);
    const bool dark = base.lightnessF() < 0.5;
    const QColor tint = dark ? QColor(112, 154, 225) : QColor(81, 125, 202);
    // A light blue wash leaves low background activity quiet, while sustained
    // load is still distinguishable without competing with row selection.
    qreal strength = (dark ? 0.025 : 0.018) + (dark ? 0.30 : 0.32) * std::sqrt(load);
    QColor result = blended(base, tint, strength);
    // Preserve the palette's text contrast; unusual/high-contrast themes can
    // reduce the tint rather than having their foreground colors replaced.
    const QColor text = option.palette.color(QPalette::Text);
    const qreal minimum = qMin<qreal>(4.5, contrast(base, text));
    for (int attempt = 0; attempt < 8 && contrast(result, text) < minimum; ++attempt) {
        strength *= 0.5;
        result = blended(base, tint, strength);
    }
    if (contrast(result, text) < minimum) return base;
    return result;
}
} // namespace

ProcessHeatDelegate::ProcessHeatDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void ProcessHeatDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem cell(option);
    initStyleOption(&cell, index);
    cell.showDecorationSelected = true;
    const bool enabled = cell.state.testFlag(QStyle::State_Enabled);
    const bool selected = cell.state.testFlag(QStyle::State_Selected);
    const auto group = !enabled ? QPalette::Disabled
        : cell.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
    cell.palette.setCurrentColorGroup(group);
    const QVariant metric = index.data(ProcessTableModel::UtilizationRole);
    bool valid = false;
    const double load = metric.toDouble(&valid);
    if (valid && std::isfinite(load) && load > 0 && load <= 1
        && !selected && enabled) {
        const QColor background = heatBackground(cell, load);
        cell.backgroundBrush = background;
        cell.palette.setColor(QPalette::Base, background);
        cell.palette.setColor(QPalette::AlternateBase, background);
    }
    const QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();
    const QString text = cell.text;
    cell.text.clear();
    painter->save();
    if (selected) {
        // Some platform styles replace selection with a translucent overlay.
        // Pair the actual highlight surface with its matching foreground instead.
        painter->fillRect(cell.rect, cell.palette.brush(QPalette::Highlight));
    } else {
        QStyleOptionViewItem background(cell);
        background.state &= ~QStyle::State_HasFocus;
        style->drawControl(QStyle::CE_ItemViewItem, &background, painter, cell.widget);
    }
    const int margin = qMax(10, cell.fontMetrics.height() / 3);
    const QRect textRect = cell.rect.adjusted(margin, 0, -margin, 0);
    if (textRect.isValid()) {
        const auto alignment = QStyle::visualAlignment(cell.direction, cell.displayAlignment);
        const auto elided = cell.fontMetrics.elidedText(text, cell.textElideMode, textRect.width());
        painter->save();
        painter->setFont(cell.font);
        painter->setClipRect(textRect, Qt::IntersectClip);
        if (selected) {
            painter->setPen(cell.palette.color(QPalette::HighlightedText));
            painter->drawText(textRect, int(alignment) | Qt::TextSingleLine, elided);
        } else {
            style->drawItemText(painter, textRect, int(alignment) | Qt::TextSingleLine, cell.palette, enabled, elided, QPalette::Text);
        }
        painter->restore();
    }
    if (cell.state.testFlag(QStyle::State_HasFocus)) {
        cell.text = text;
        QStyleOptionFocusRect focus;
        focus.rect = style->subElementRect(QStyle::SE_ItemViewItemFocusRect, &cell, cell.widget);
        focus.state = cell.state | QStyle::State_KeyboardFocusChange | QStyle::State_Item;
        focus.direction = cell.direction; focus.fontMetrics = cell.fontMetrics; focus.palette = cell.palette;
        focus.backgroundColor = cell.palette.color(selected ? QPalette::Highlight : QPalette::Base);
        style->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, cell.widget);
    }
    painter->restore();
}

} // namespace monitor::ui

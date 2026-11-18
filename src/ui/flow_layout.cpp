// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/flow_layout.h"

namespace monitor::ui {
FlowLayout::FlowLayout(QWidget *parent, int spacing) : QLayout(parent)
{
    setContentsMargins(0, 0, 0, 0);
    setSpacing(spacing);
}
FlowLayout::~FlowLayout() { while (auto *item = takeAt(0)) delete item; }
void FlowLayout::addItem(QLayoutItem *item) { items_.append(item); }
int FlowLayout::count() const { return items_.size(); }
QLayoutItem *FlowLayout::itemAt(int index) const { return items_.value(index); }
QLayoutItem *FlowLayout::takeAt(int index) { return index >= 0 && index < count() ? items_.takeAt(index) : nullptr; }
Qt::Orientations FlowLayout::expandingDirections() const { return {}; }
bool FlowLayout::hasHeightForWidth() const { return true; }
int FlowLayout::heightForWidth(int width) const { return arrange(QRect(0, 0, width, 0), true); }
QSize FlowLayout::minimumSize() const
{
    QSize result;
    for (auto *item : items_) if (!item->isEmpty()) result = result.expandedTo(item->minimumSize());
    const auto margins = contentsMargins();
    return result + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}
QSize FlowLayout::sizeHint() const
{
    int width = 0;
    for (auto *item : items_) if (!item->isEmpty()) width += item->sizeHint().width() + spacing();
    const auto margins = contentsMargins();
    width = qMax(0, width - spacing()) + margins.left() + margins.right();
    return QSize(width, heightForWidth(width));
}
void FlowLayout::setGeometry(const QRect &rect) { QLayout::setGeometry(rect); arrange(rect, false); }
int FlowLayout::arrange(const QRect &rect, bool measureOnly) const
{
    const auto margins = contentsMargins();
    const QRect area = rect.marginsRemoved(margins);
    int x = area.x(), y = area.y(), rowHeight = 0;
    for (auto *item : items_) {
        if (item->isEmpty()) continue;
        const QSize preferred = item->sizeHint().expandedTo(item->minimumSize());
        const int width = qMin(preferred.width(), qMax(0, area.width()));
        if (rowHeight && x + width > area.right() + 1) {
            x = area.x(); y += rowHeight + spacing(); rowHeight = 0;
        }
        const int height = item->hasHeightForWidth() ? qMax(preferred.height(), item->heightForWidth(width)) : preferred.height();
        if (!measureOnly) item->setGeometry(QRect(x, y, width, height));
        x += width + spacing(); rowHeight = qMax(rowHeight, height);
    }
    return y + rowHeight - rect.y() + margins.bottom();
}
} // namespace monitor::ui

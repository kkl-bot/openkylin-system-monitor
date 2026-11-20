// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/process_table_header.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QVariant>

namespace monitor::ui {

ProcessTableHeader::ProcessTableHeader(QWidget *parent) : QHeaderView(Qt::Horizontal, parent) {}

void ProcessTableHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid() || !model()) return;
    QStyleOptionHeader section;
    initStyleOption(&section);
    section.rect = rect;
    section.section = logicalIndex;
    section.orientation = Qt::Horizontal;
    section.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken);
    if (sectionsClickable() && viewport()->underMouse()
        && logicalIndexAt(viewport()->mapFromGlobal(QCursor::pos())) == logicalIndex) {
        section.state |= QStyle::State_MouseOver;
        if (QApplication::mouseButtons().testFlag(Qt::LeftButton)) section.state |= QStyle::State_Sunken;
    }
    const int visual = visualIndex(logicalIndex);
    section.position = count() == 1 ? QStyleOptionHeader::OnlyOneSection
        : visual == 0 ? QStyleOptionHeader::Beginning
        : visual == count() - 1 ? QStyleOptionHeader::End : QStyleOptionHeader::Middle;
    section.selectedPosition = QStyleOptionHeader::NotAdjacent;
    const bool sorted = isSortIndicatorShown() && sortIndicatorSection() == logicalIndex;
    section.sortIndicator = QStyleOptionHeader::None;
    const auto alignment = model()->headerData(logicalIndex, Qt::Horizontal, Qt::TextAlignmentRole);
    section.textAlignment = alignment.isValid() ? Qt::Alignment(alignment.toInt()) : defaultAlignment();

    painter->save();
    painter->setClipRect(rect, Qt::IntersectClip);
    painter->setFont(font());
    // Keep native/QSS section framing and hover, but own label/arrow geometry.
    // Some styles return almost the entire section for SE_HeaderArrow.
    section.text.clear();
    style()->drawControl(QStyle::CE_HeaderSection, &section, painter, this);

    const int margin = qMax(10, fontMetrics().height() / 3);
    QRect labelRect = rect.adjusted(margin, 0, -margin, 0);
    const auto group = !isEnabled() ? QPalette::Disabled
        : window()->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    const QColor foreground = palette().color(group, QPalette::ButtonText);
    if (sorted && labelRect.isValid()) {
        const int arrowWidth = qMin(labelRect.width(), qMax(10, fontMetrics().height() / 2));
        const qreal arrowHeight = qMax(4, arrowWidth / 2);
        const int gap = qMax(8, fontMetrics().height() / 3);
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        const qreal left = rtl ? labelRect.left() : labelRect.right() - arrowWidth + 1;
        const qreal right = left + arrowWidth - 1;
        const qreal top = rect.center().y() - arrowHeight / 2.0;
        const qreal bottom = top + arrowHeight;
        const bool ascending = sortIndicatorOrder() == Qt::AscendingOrder;
        QPolygonF arrow;
        arrow << QPointF(left, ascending ? bottom : top)
              << QPointF((left + right) / 2.0, ascending ? top : bottom)
              << QPointF(right, ascending ? bottom : top);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(foreground, qMax<qreal>(1.5, fontMetrics().height() / 16.0),
            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPolyline(arrow);
        if (rtl) labelRect.setLeft(labelRect.left() + arrowWidth + gap);
        else labelRect.setRight(labelRect.right() - arrowWidth - gap);
    }
    if (labelRect.isValid()) {
        const auto text = fontMetrics().elidedText(model()->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString(),
            Qt::ElideRight, labelRect.width());
        painter->setClipRect(labelRect, Qt::IntersectClip);
        painter->setPen(foreground);
        painter->drawText(labelRect, int(QStyle::visualAlignment(layoutDirection(), section.textAlignment)) | Qt::TextSingleLine, text);
    }
    painter->restore();
}

} // namespace monitor::ui

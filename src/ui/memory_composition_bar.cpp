// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/memory_composition_bar.h"
#include "ui/paint_palette.h"
#include "ui/theme.h"
#include "domain/metrics.h"
#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace monitor::ui {
MemoryCompositionBar::MemoryCompositionBar(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("memoryCompositionBar"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("内存组成：已用与可用"));
}
void MemoryCompositionBar::setValues(const std::optional<quint64> &used, const std::optional<quint64> &total)
{
    const bool valid = used && total && *total > 0 && *used <= *total;
    used_ = valid ? used : std::nullopt; total_ = valid ? total : std::nullopt;
    const QString description = valid
        ? QStringLiteral("已用 %1（%2%）；可用 %3（%4%）")
            .arg(formatBytes(*used), QString::number(100.0 * *used / *total, 'f', 1), formatBytes(*total - *used),
                 QString::number(100.0 * (*total - *used) / *total, 'f', 1))
        : QStringLiteral("无法取得有效的内存容量，组成比例不可用");
    setAccessibleDescription(description);
    setToolTip(QStringLiteral("<qt>") + description.toHtmlEscaped() + QStringLiteral("</qt>"));
    update();
}
QSize MemoryCompositionBar::sizeHint() const { return {360, qMax(26, fontMetrics().height() + 6)}; }
QSize MemoryCompositionBar::minimumSizeHint() const { return {32, qMax(26, fontMetrics().height() + 6)}; }
void MemoryCompositionBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange || event->type() == QEvent::StyleChange) {
        updateGeometry(); update();
    }
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) update();
}
void MemoryCompositionBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(1, 1, -1, -1);
    if (bounds.width() <= 0 || bounds.height() <= 0) return;
    const QColor color = chartAccentColor(resourceColor(ResourceKind::Memory), palette());
    const QColor base = palette().color(QPalette::Base);
    QPainterPath shape;
    shape.addRoundedRect(bounds, 3, 3);
    painter.fillPath(shape, blendPaintColor(base, color, 0.035));
    painter.save();
    painter.setClipPath(shape);
    if (used_ && total_) {
        const qreal fraction = static_cast<double>(*used_) / *total_;
        const QColor fill = blendPaintColor(base, color, base.lightnessF() < 0.5 ? 0.60 : 0.42);
        const qreal usedWidth = bounds.width() * fraction;
        painter.fillRect(QRectF(bounds.topLeft(), QSizeF(usedWidth, bounds.height())), fill);
        if (*used_ > 0 && *used_ < *total_) {
            painter.setPen(QPen(blendPaintColor(base, color, 0.75), 1));
            painter.drawLine(QPointF(bounds.left() + usedWidth, bounds.top()), QPointF(bounds.left() + usedWidth, bounds.bottom()));
        }
    } else {
        painter.fillRect(bounds, QBrush(chartBorderColor(palette()), Qt::BDiagPattern));
    }
    painter.restore();
    painter.setPen(QPen(blendPaintColor(chartBorderColor(palette()), color, 0.42), 1));
    painter.setBrush(Qt::NoBrush); painter.drawPath(shape);
}
} // namespace monitor::ui

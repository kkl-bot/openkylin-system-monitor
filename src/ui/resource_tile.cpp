#include "ui/resource_tile.h"
#include "ui/theme.h"
#include <QPainter>
#include <QPainterPath>

namespace monitor::ui {
ResourceTile::ResourceTile(MonitorModel *model, const QString &id, QWidget *parent)
    : QPushButton(parent), model_(model), id_(id)
{
    setCheckable(true);
    setAutoExclusive(true);
    setMinimumHeight(108);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(id);
    connect(model, &MonitorModel::changed, this, QOverload<>::of(&ResourceTile::update));
}
QString ResourceTile::resourceId() const { return id_; }
void ResourceTile::paintEvent(QPaintEvent *)
{
    const auto *sample = model_->resource(id_);
    if (!sample) return;
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto color = resourceColor(sample->kind);
    QColor selected = palette().color(QPalette::Highlight); selected.setAlpha(40);
    p.setPen(isChecked() ? QPen(palette().color(QPalette::Highlight)) : Qt::NoPen);
    p.setBrush(isChecked() ? selected : underMouse() ? palette().color(QPalette::Button) : palette().color(QPalette::AlternateBase));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);
    p.setPen(Qt::NoPen); p.setBrush(color); p.drawRoundedRect(QRectF(15, 20, 4, 18), 2, 2);
    QFont label = font(); label.setPointSize(11); label.setBold(true); p.setFont(label);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRect(29, 16, width() - 44, 26), Qt::AlignLeft | Qt::AlignVCenter, resourceLabel(sample->kind));
    label.setPointSize(9); label.setBold(false); p.setFont(label); p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(QRect(29, 43, width() - 43, 20), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(sample->name, Qt::ElideRight, width() - 44));
    p.setPen(color); label.setPointSize(12); p.setFont(label);
    p.drawText(QRect(29, 72, 90, 22), Qt::AlignLeft, formatMetric(sample->utilization, "%"));
    label.setPointSize(9); p.setFont(label); p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(QRect(width() - 85, 73, 67, 22), Qt::AlignRight, formatMetric(sample->temperature, " °C"));
    if (hasFocus()) { p.setPen(QPen(color, 1, Qt::DashLine)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), 7, 7); }
}
} // namespace monitor::ui

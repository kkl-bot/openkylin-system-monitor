// SPDX-License-Identifier: GPL-3.0-or-later
#include "resource_bars.h"
#include <QPainter>
#include <QMouseEvent>
#include <QQuickWindow>

namespace monitor::widget {
ResourceBars::ResourceBars(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    const auto observe = [this](QQuickWindow *window) {
        if (observedWindow_) observedWindow_->removeEventFilter(this);
        observedWindow_ = window;
        if (window) window->installEventFilter(this);
    };
    connect(this, &QQuickItem::windowChanged, this, observe);
    observe(window());
}
ResourceBars::~ResourceBars()
{
    // QQuickItem may emit windowChanged during its base destructor, after this
    // class's QPointer member has already been destroyed.
    disconnect(this, nullptr, this, nullptr);
    if (observedWindow_) observedWindow_->removeEventFilter(this);
}
QVariantList ResourceBars::resources() const { return resources_; }
bool ResourceBars::dark() const { return dark_; }
bool ResourceBars::paused() const { return paused_; }
bool ResourceBars::interactive() const { return interactive_; }
void ResourceBars::setResources(const QVariantList &value) { resources_ = value; emit resourcesChanged(); update(); }
void ResourceBars::setDark(bool value) { if (dark_ != value) { dark_ = value; emit darkChanged(); update(); } }
void ResourceBars::setPaused(bool value) { if (paused_ != value) { paused_ = value; emit pausedChanged(); update(); } }
void ResourceBars::setInteractive(bool value) { if (interactive_ != value) { interactive_ = value; emit interactiveChanged(); } }
bool ResourceBars::eventFilter(QObject *watched, QEvent *event)
{
    if (interactive_ && isVisible() && isEnabled() && watched == observedWindow_ && event->type() == QEvent::MouseButtonDblClick) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && contains(mapFromScene(mouse->localPos()))) emit doubleClicked();
    }
    // Observe double-clicks without accepting or grabbing a press. Peony's
    // parent MouseArea retains the complete press / move / release drag stream.
    return false;
}
void ResourceBars::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing);
    const QColor colors[3] = {QColor(dark_ ? "#70afff" : "#2979e8"),
                             QColor(dark_ ? "#54d4b6" : "#1b9b82"),
                             QColor(dark_ ? "#c49af0" : "#a366d0")};
    const qreal margin = qMax<qreal>(8, qMin(width(), height()) * 0.08);
    const qreal groupWidth = (width() - 2 * margin) / 3;
    const qreal barWidth = qMax<qreal>(3, groupWidth * 0.25);
    const qreal barHeight = qMax<qreal>(1, height() - 2 * margin);
    for (int group = 0; group < 3; ++group) {
        const auto resource = group < resources_.size() ? resources_[group].toMap() : QVariantMap{};
        for (int column = 0; column < 2; ++column) {
            const QRectF track(margin + group * groupWidth + groupWidth * 0.13 + column * groupWidth * 0.35,
                               margin, barWidth, barHeight);
            const QVariant value = resource.value(column == 0 ? "utilization" : "temperature");
            QColor trackColor = colors[group]; trackColor.setAlpha(dark_ ? 40 : 24);
            painter->setPen(Qt::NoPen); painter->setBrush(trackColor); painter->drawRoundedRect(track, 4, 4);
            if (value.isValid() && !value.isNull()) {
                const qreal ratio = qBound(0.0, value.toDouble() / (column == 0 ? 100.0 : 120.0), 1.0);
                QColor fill = colors[group]; fill.setAlpha(paused_ ? 95 : column == 0 ? 255 : 155);
                painter->setBrush(fill);
                if (ratio > 0) painter->drawRoundedRect(QRectF(track.left(), track.bottom() - ratio * track.height(), track.width(), ratio * track.height()), 4, 4);
            } else {
                QColor hatch = colors[group]; hatch.setAlpha(100);
                painter->setBrush(QBrush(hatch, Qt::BDiagPattern)); painter->drawRoundedRect(track, 4, 4);
            }
        }
    }
}
} // namespace monitor::widget

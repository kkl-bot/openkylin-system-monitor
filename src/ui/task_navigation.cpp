// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/task_navigation.h"
#include "ui/paint_palette.h"
#include <QButtonGroup>
#include <QEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace monitor::ui {
namespace {
enum class Glyph { Menu, Processes, Performance, Network };

void drawGlyph(QPainter &p, Glyph glyph)
{
    switch (glyph) {
    case Glyph::Menu:
        for (int y : {5, 10, 15}) p.drawLine(2, y, 18, y);
        break;
    case Glyph::Processes:
        p.drawRoundedRect(QRectF(2, 3, 16, 14), 2, 2);
        p.drawLine(8, 3, 8, 17);
        p.drawLine(8, 9, 18, 9);
        break;
    case Glyph::Performance: {
        p.drawRoundedRect(QRectF(2, 2, 16, 16), 3, 3);
        QPolygonF points;
        points << QPointF(4, 11) << QPointF(7, 11) << QPointF(9, 6)
               << QPointF(12, 14) << QPointF(14, 10) << QPointF(16, 10);
        p.drawPolyline(points);
        break;
    }
    case Glyph::Network:
        p.drawRoundedRect(QRectF(6, 2, 8, 6), 1, 1);
        p.drawLine(10, 8, 10, 12);
        p.drawLine(4, 12, 16, 12);
        p.drawLine(4, 12, 4, 15);
        p.drawLine(16, 12, 16, 15);
        p.drawRoundedRect(QRectF(2, 15, 4, 3), 1, 1);
        p.drawRoundedRect(QRectF(14, 15, 4, 3), 1, 1);
        break;
    }
}

class NavigationButton final : public QPushButton {
public:
    NavigationButton(Glyph glyph, const QString &label, QWidget *parent);
    void setCompact(bool compact);
protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *) override;
private:
    Glyph glyph_;
    bool compact_ = false;
};

NavigationButton::NavigationButton(Glyph glyph, const QString &label, QWidget *parent)
    : QPushButton(label, parent), glyph_(glyph)
{
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(label);
    setToolTip(label);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_StyledBackground, false);
}
void NavigationButton::setCompact(bool compact) { compact_ = compact; update(); }
bool NavigationButton::event(QEvent *event)
{
    const bool handled = QPushButton::event(event);
    if (event->type() == QEvent::Polish || event->type() == QEvent::StyleChange) {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_StyledBackground, false);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
    return handled;
}
void NavigationButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect box = rect().adjusted(1, 1, -1, -1);
    const QPalette navigationPalette = parentWidget() ? parentWidget()->palette() : palette();
    const QColor accent = navigationPalette.color(QPalette::Highlight);
    const QColor foreground = navigationPalette.color(QPalette::WindowText);
    if (underMouse() || isChecked() || isDown()) {
        p.setBrush(blendPaintColor(navigationPalette.color(QPalette::AlternateBase), accent,
            isDown() ? 0.19 : isChecked() ? 0.13 : 0.055));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(box, 7, 7);
    }
    if (isChecked()) {
        p.setBrush(accent);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(2, height() / 4, 3, height() / 2), 1.5, 1.5);
    }
    const int icon = qMax(19, fontMetrics().height() * 4 / 5);
    const int x = compact_ ? (width() - icon) / 2 : 14;
    p.save();
    p.translate(x, (height() - icon) / 2);
    p.scale(icon / 20.0, icon / 20.0);
    p.setPen(QPen(isChecked() ? chartAccentColor(accent, navigationPalette) : foreground,
        1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    drawGlyph(p, glyph_);
    p.restore();
    if (!compact_) {
        p.setPen(foreground);
        p.drawText(rect().adjusted(x + icon + 12, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, text());
    }
    if (hasFocus() && window()->testAttribute(Qt::WA_KeyboardFocusChange)) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(accent, 1, Qt::DashLine));
        p.drawRoundedRect(box.adjusted(1, 1, -1, -1), 5, 5);
    }
}
} // namespace

TaskNavigation::TaskNavigation(QWidget *parent) : QFrame(parent)
{
    setObjectName("sidebar");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 8, 6, 8);
    layout->setSpacing(4);
    toggle_ = new NavigationButton(Glyph::Menu, tr("任务管理器"), this);
    toggle_->setObjectName("navigationToggle");
    layout->addWidget(toggle_);
    layout->addSpacing(8);
    connect(toggle_, &QPushButton::clicked, this, [this] { setCollapsed(!collapsed_); });
    group_ = new QButtonGroup(this);
    auto add = [&](TaskSection section, Glyph glyph, const QString &label, const char *name) {
        auto *button = new NavigationButton(glyph, label, this);
        button->setObjectName(name);
        button->setCheckable(true);
        group_->addButton(button, static_cast<int>(section));
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, section] { emit sectionRequested(section); });
    };
    add(TaskSection::Processes, Glyph::Processes, tr("进程"), "processNavigation");
    add(TaskSection::Performance, Glyph::Performance, tr("性能"), "performanceNavigation");
    add(TaskSection::Network, Glyph::Network, tr("网络"), "networkNavigation");
    layout->addStretch();
    setCollapsed(false);
}
void TaskNavigation::setSection(TaskSection section)
{
    group_->button(static_cast<int>(section))->setChecked(true);
}
bool TaskNavigation::isCollapsed() const { return collapsed_; }
void TaskNavigation::setCollapsed(bool collapsed)
{
    collapsed_ = collapsed;
    const auto action = collapsed ? tr("展开导航") : tr("收起导航");
    toggle_->setAccessibleName(action);
    toggle_->setToolTip(action);
    updateDimensions();
}
void TaskNavigation::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) updateDimensions();
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        update();
        for (auto *button : findChildren<QPushButton *>()) button->update();
    }
}
void TaskNavigation::updateDimensions()
{
    if (!group_) return;
    const int line = fontMetrics().height();
    const int icon = qMax(19, line * 4 / 5);
    setFixedWidth(collapsed_ ? icon + 34 : qMax(155, fontMetrics().horizontalAdvance(toggle_->text()) + icon + 52));
    for (auto *button : findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly)) {
        static_cast<NavigationButton *>(button)->setCompact(collapsed_);
        button->setFixedHeight(qMax(38, line + 16));
    }
}
} // namespace monitor::ui

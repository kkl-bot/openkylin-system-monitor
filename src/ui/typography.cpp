// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/typography.h"
#include <QApplication>
#include <QEvent>
#include <QFontMetricsF>
#include <QVariant>
#include <QWidget>

namespace monitor::ui {
namespace {
QFont resizedFont(const QFont &base, qreal size)
{
    QFont result(base);
    if (base.pixelSize() > 0)
        result.setPixelSize(qRound(size));
    else
        result.setPointSizeF(qRound(size * 2) / 2.0);
    return result;
}

qreal fontSize(const QFont &font)
{
    return font.pixelSize() > 0 ? font.pixelSize() : font.pointSizeF();
}

QFont windowBodyFont(const QWidget *window, const QFont &desktopFont)
{
    const qreal minSize = desktopFont.pixelSize() > 0 ? 12.0 * window->logicalDpiY() / 72.0 : 12.0;
    // 760x500 uses the readable floor; 1180x780 adds one sixth. Width gets
    // more weight because it determines how much text fits on each line.
    const qreal widthSteps = qMax(0, window->width() - 760) / 420.0;
    const qreal heightSteps = qMax(0, window->height() - 500) / 280.0;
    const qreal growth = (2 * widthSteps + heightSteps) / 3.0;
    const qreal scale = 1.0 + qMin(0.5, growth / 6.0);
    return resizedFont(desktopFont, qMax(minSize, fontSize(desktopFont)) * scale);
}

TextRole roleOf(const QWidget *widget)
{
    const QString role = widget->property("textRole").toString();
    if (role == "title") return TextRole::Title;
    if (role == "metric") return TextRole::Metric;
    if (role == "section") return TextRole::Section;
    if (role == "caption") return TextRole::Caption;
    return TextRole::Body;
}

class WindowTypography final : public QObject {
public:
    explicit WindowTypography(QWidget *window);
private:
    bool eventFilter(QObject *object, QEvent *event) override;
    void update(bool force);
    QWidget *window_;
    QFont desktopFont_;
    QFont currentBody_;
    bool updating_ = false;
};

WindowTypography::WindowTypography(QWidget *window)
    : QObject(window), window_(window), desktopFont_(QApplication::font())
{
    window_->installEventFilter(this);
    connect(qApp, &QGuiApplication::fontChanged, this, [this](const QFont &font) {
        desktopFont_ = font;
        update(true);
    });
    update(true);
}

bool WindowTypography::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::Resize || event->type() == QEvent::ScreenChangeInternal)
        update(false);
    else if (event->type() == QEvent::Show || event->type() == QEvent::StyleChange)
        update(true);
    return false;
}

void WindowTypography::update(bool force)
{
    if (updating_) return;
    QFont body = windowBodyFont(window_, desktopFont_);
    // Opening a modal detail view should preserve the reading size of its owner.
    if (const auto *owner = window_->parentWidget()) {
        const QFont parentBody = owner->window()->font();
        if (QFontMetricsF(parentBody, window_).height() > QFontMetricsF(body, window_).height())
            body = parentBody;
    }
    if (!force && currentBody_ == body && window_->font() == body) return;
    updating_ = true;
    currentBody_ = body;
    if (window_->font() != body) window_->setFont(body);
    applyTypography(window_);
    updating_ = false;
}
} // namespace

QFont typographyFont(const QFont &body, TextRole role)
{
    qreal scale = 1.0;
    if (role == TextRole::Title) scale = 1.5;
    if (role == TextRole::Metric) scale = 1.35;
    QFont result = scale == 1.0 ? body : resizedFont(body, fontSize(body) * scale);
    if (role == TextRole::Title || role == TextRole::Section || role == TextRole::Metric)
        result.setWeight(QFont::DemiBold);
    return result;
}

void applyTypography(QWidget *root)
{
    const QFont body = root->font();
    for (auto *child : root->findChildren<QWidget *>()) {
        if (child->window() != root->window()) continue;
        const auto target = typographyFont(body, roleOf(child));
        if (child->font() != target) child->setFont(target);
    }
}

void enableWindowTypography(QWidget *window)
{
    new WindowTypography(window);
}
} // namespace monitor::ui

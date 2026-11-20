// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_rate_card.h"
#include "ui/paint_palette.h"
#include "ui/theme.h"
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QVariant>
#include <QVBoxLayout>

namespace monitor::ui {
NetworkRateCard::NetworkRateCard(const QString &title, const QString &valueName, QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("metricCard"));
    upload_ = valueName == QStringLiteral("networkUpload");
    setProperty("networkDirection", upload_ ? "upload" : "download");
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(16, 12, 16, 12);
    auto *caption = new QLabel(title, this); caption->setObjectName(QStringLiteral("cardLabel"));
    caption->setProperty("textRole", "caption");
    caption->setTextFormat(Qt::PlainText); caption->setWordWrap(true); layout->addWidget(caption);
    value_ = new QLabel(QStringLiteral("—"), this); value_->setObjectName(valueName);
    value_->setProperty("textRole", "metric");
    value_->setTextFormat(Qt::PlainText); layout->addWidget(value_);
    value_->installEventFilter(this);
    updateMinimumWidth();
}
QLabel *NetworkRateCard::valueLabel() const { return value_; }
void NetworkRateCard::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor accent = chartAccentColor(upload_ ? resourceColor(ResourceKind::Gpu)
        : palette().color(QPalette::Highlight), palette());
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(6, 14, 3, qMax(0, height() - 28)), 1.5, 1.5);
}
void NetworkRateCard::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange || event->type() == QEvent::StyleChange)
        updateMinimumWidth();
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) update();
}
bool NetworkRateCard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == value_ && (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange
        || event->type() == QEvent::StyleChange)) updateMinimumWidth();
    return QFrame::eventFilter(watched, event);
}
void NetworkRateCard::updateMinimumWidth()
{
    // Typography owns the value font. Only measure its resolved size here, including later role updates.
    setMinimumWidth(qMax(180, value_->fontMetrics().horizontalAdvance(QStringLiteral("999.9 MiB/s")) + 32));
    updateGeometry();
}
} // namespace monitor::ui

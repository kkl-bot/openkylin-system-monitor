// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/network_rate_card.h"
#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>

namespace monitor::ui {
NetworkRateCard::NetworkRateCard(const QString &title, const QString &valueName, QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("metricCard"));
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(16, 12, 16, 12);
    auto *caption = new QLabel(title, this); caption->setObjectName(QStringLiteral("cardLabel"));
    caption->setTextFormat(Qt::PlainText); caption->setWordWrap(true); layout->addWidget(caption);
    value_ = new QLabel(QStringLiteral("—"), this); value_->setObjectName(valueName);
    value_->setTextFormat(Qt::PlainText); layout->addWidget(value_);
    updateValueFont();
}
QLabel *NetworkRateCard::valueLabel() const { return value_; }
void NetworkRateCard::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange || event->type() == QEvent::StyleChange)
        updateValueFont();
}
void NetworkRateCard::updateValueFont()
{
    QFont emphasis = font(); emphasis.setBold(true);
    if (emphasis.pointSizeF() > 0) emphasis.setPointSizeF(emphasis.pointSizeF() * 1.6);
    else if (emphasis.pixelSize() > 0) emphasis.setPixelSize(qMax(1, qRound(emphasis.pixelSize() * 1.6)));
    value_->setFont(emphasis);
    setMinimumWidth(qMax(180, value_->fontMetrics().horizontalAdvance(QStringLiteral("999.9 MiB/s")) + 32));
    updateGeometry();
}
} // namespace monitor::ui

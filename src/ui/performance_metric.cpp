// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/performance_metric.h"
#include <QLabel>
#include <QVariant>
#include <QVBoxLayout>

namespace monitor::ui {
PerformanceMetric::PerformanceMetric(const QString &caption, const QString &valueName, QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("performanceMetric"));
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 20, 4); layout->setSpacing(3);
    caption_ = new QLabel(caption, this); caption_->setObjectName(QStringLiteral("cardLabel"));
    caption_->setProperty("textRole", "caption"); caption_->setTextFormat(Qt::PlainText); caption_->setWordWrap(true);
    value_ = new QLabel(QStringLiteral("—"), this); value_->setObjectName(valueName);
    value_->setProperty("textRole", "metric"); value_->setTextFormat(Qt::PlainText); value_->setWordWrap(true);
    layout->addWidget(caption_); layout->addWidget(value_);
}
void PerformanceMetric::setCaption(const QString &caption) { caption_->setText(caption); }
void PerformanceMetric::setValue(const QString &value, const QString &description)
{
    value_->setText(value);
    value_->setToolTip(QStringLiteral("<qt>") + (description.isEmpty() ? value : description).toHtmlEscaped() + QStringLiteral("</qt>"));
}
} // namespace monitor::ui

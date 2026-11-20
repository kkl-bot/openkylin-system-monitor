// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>

class QLabel;
namespace monitor::ui {
class PerformanceMetric final : public QWidget {
public:
    explicit PerformanceMetric(const QString &caption, const QString &valueName, QWidget *parent = nullptr);
    void setCaption(const QString &caption);
    void setValue(const QString &value, const QString &description = QString());
private:
    QLabel *caption_;
    QLabel *value_;
};
} // namespace monitor::ui

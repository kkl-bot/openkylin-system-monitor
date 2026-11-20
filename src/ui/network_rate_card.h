// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFrame>

class QLabel;
namespace monitor::ui {
class NetworkRateCard final : public QFrame {
public:
    explicit NetworkRateCard(const QString &title, const QString &valueName, QWidget *parent = nullptr);
    QLabel *valueLabel() const;
protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void updateMinimumWidth();
    QLabel *value_;
    bool upload_ = false;
};
} // namespace monitor::ui

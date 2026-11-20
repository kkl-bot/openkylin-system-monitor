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
    void changeEvent(QEvent *event) override;
private:
    void updateValueFont();
    QLabel *value_;
};
} // namespace monitor::ui

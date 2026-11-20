// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <optional>

namespace monitor::ui {
class MemoryCompositionBar final : public QWidget {
    Q_OBJECT
public:
    explicit MemoryCompositionBar(QWidget *parent = nullptr);
    void setValues(const std::optional<quint64> &used, const std::optional<quint64> &total);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    std::optional<quint64> used_;
    std::optional<quint64> total_;
};
} // namespace monitor::ui

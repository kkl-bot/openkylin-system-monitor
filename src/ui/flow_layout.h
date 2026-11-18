// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QLayout>
#include <QList>

namespace monitor::ui {
// Reflows controls onto another line without shrinking their text.
class FlowLayout final : public QLayout {
public:
    explicit FlowLayout(QWidget *parent = nullptr, int spacing = 8);
    ~FlowLayout() override;
    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;
private:
    int arrange(const QRect &rect, bool measureOnly) const;
    QList<QLayoutItem *> items_;
};
} // namespace monitor::ui

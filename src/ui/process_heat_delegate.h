// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStyledItemDelegate>

namespace monitor::ui {

class ProcessHeatDelegate final : public QStyledItemDelegate {
public:
    explicit ProcessHeatDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

} // namespace monitor::ui

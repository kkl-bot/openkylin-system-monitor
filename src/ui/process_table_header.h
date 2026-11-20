// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHeaderView>

namespace monitor::ui {

class ProcessTableHeader final : public QHeaderView {
public:
    explicit ProcessTableHeader(QWidget *parent = nullptr);
protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};

} // namespace monitor::ui

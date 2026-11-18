// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSet>
#include <QTableView>

namespace monitor::ui {
class ProcessTableView final : public QTableView {
    Q_OBJECT
public:
    explicit ProcessTableView(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model) override;
    QByteArray savedHeaderState() const;
    bool restoreSavedHeaderState(const QByteArray &state);
protected:
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool viewportEvent(QEvent *event) override;
private:
    void scheduleMetricsUpdate();
    void updateMetrics();
    QSet<int> manualColumns_;
    bool updatingMetrics_ = false;
    bool metricsUpdatePending_ = false;
};
} // namespace monitor::ui

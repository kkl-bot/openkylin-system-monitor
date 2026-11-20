// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "domain/network_types.h"
#include <QHash>
#include <QWidget>

namespace monitor::ui {
struct NetworkRatePoint {
    QDateTime timestamp;
    std::optional<double> download;
    std::optional<double> upload;
    bool breakBefore = false;
};
class NetworkHistoryChart final : public QWidget {
    Q_OBJECT
public:
    explicit NetworkHistoryChart(QWidget *parent = nullptr);
    void acceptSnapshot(const NetworkSnapshot &snapshot);
    void setInterface(const QString &name);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    QVector<NetworkRatePoint> history(const QString &name) const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    struct Series {
        int index = 0;
        QDateTime lastSeen;
        QVector<NetworkRatePoint> points;
    };
    double axisMaximum() const;
    QHash<QString, Series> series_;
    QString selectedInterface_;
    QDateTime timestamp_;
    bool paused_ = false;
    bool breakPending_ = false;
    int interval_ = 1000;
};
} // namespace monitor::ui

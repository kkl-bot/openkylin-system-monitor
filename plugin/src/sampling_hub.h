// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "backend/monitor_service.h"
#include <QHash>
#include <QPointer>

namespace monitor::widget {
class SamplingHub final : public QObject {
    Q_OBJECT
public:
    static SamplingHub *instance();
    void subscribe(QObject *consumer, int interval);
    void unsubscribe(QObject *consumer);
    Snapshot latest() const;
signals:
    void sampled(const monitor::Snapshot &snapshot);
private:
    explicit SamplingHub(QObject *parent);
    void updateSchedule();
    MonitorService service_;
    QHash<QObject *, int> consumers_;
    Snapshot latest_;
    bool started_ = false;
};
} // namespace monitor::widget

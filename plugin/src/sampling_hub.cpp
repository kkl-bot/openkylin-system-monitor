// SPDX-License-Identifier: GPL-3.0-or-later
#include "sampling_hub.h"
#include <QCoreApplication>
#include <algorithm>

namespace monitor::widget {
SamplingHub *SamplingHub::instance()
{
    static QPointer<SamplingHub> hub;
    if (!hub) hub = new SamplingHub(QCoreApplication::instance());
    return hub;
}
SamplingHub::SamplingHub(QObject *parent) : QObject(parent)
{
    connect(&service_, &MonitorService::snapshotReady, this, [this](const Snapshot &snapshot) {
        latest_ = snapshot; emit sampled(snapshot);
    });
}
void SamplingHub::subscribe(QObject *consumer, int interval)
{
    if (!consumers_.contains(consumer))
        connect(consumer, &QObject::destroyed, this, [this, consumer]() { unsubscribe(consumer); });
    consumers_.insert(consumer, interval);
    updateSchedule();
}
void SamplingHub::unsubscribe(QObject *consumer) { consumers_.remove(consumer); updateSchedule(); }
void SamplingHub::updateSchedule()
{
    service_.setPaused(consumers_.isEmpty());
    if (consumers_.isEmpty()) return;
    const auto minimum = std::min_element(consumers_.cbegin(), consumers_.cend());
    service_.setInterval(*minimum);
    if (!started_) { started_ = true; service_.start(); }
}
Snapshot SamplingHub::latest() const { return latest_; }
} // namespace monitor::widget

#include "ui/monitor_model.h"
#include <QSet>

namespace monitor::ui {
MonitorModel::MonitorModel(QObject *parent) : QObject(parent) {}
void MonitorModel::acceptSnapshot(const Snapshot &snapshot)
{
    snapshot_ = snapshot;
    QSet<QString> ids;
    for (const auto &sample : snapshot.resources) {
        ids.insert(sample.id);
        auto &points = histories_[sample.id];
        if (!points.isEmpty() && points.last().timestamp >= snapshot.timestamp) points.clear();
        points.append({snapshot.timestamp, sample.utilization.value, sample.temperature.value});
        while (!points.isEmpty() && (points.first().timestamp.msecsTo(snapshot.timestamp) > 60000 || points.size() > 122))
            points.removeFirst();
    }
    for (auto it = histories_.begin(); it != histories_.end();) {
        if (!ids.contains(it.key())) it = histories_.erase(it);
        else ++it;
    }
    emit changed();
}
const Snapshot &MonitorModel::snapshot() const { return snapshot_; }
const ResourceSample *MonitorModel::resource(const QString &id) const
{
    for (const auto &resource : snapshot_.resources)
        if (resource.id == id) return &resource;
    return nullptr;
}
QVector<HistoryPoint> MonitorModel::history(const QString &id) const { return histories_.value(id); }
} // namespace monitor::ui

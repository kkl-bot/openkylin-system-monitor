#include "domain/snapshot_json.h"
#include <QJsonArray>
#include <QJsonObject>

namespace monitor {
namespace {
QString kindName(ResourceKind kind)
{
    switch (kind) {
    case ResourceKind::Cpu: return QStringLiteral("cpu");
    case ResourceKind::Gpu: return QStringLiteral("gpu");
    case ResourceKind::Memory: return QStringLiteral("memory");
    }
    return {};
}

QJsonObject metricJson(const Metric &metric)
{
    return {{"value", metric.value ? QJsonValue(*metric.value) : QJsonValue(QJsonValue::Null)},
            {"reason", metric.reason}, {"source", metric.source}};
}
} // namespace

QJsonDocument snapshotJson(const Snapshot &snapshot, bool paused)
{
    QJsonArray resources;
    for (const auto &sample : snapshot.resources) {
        QJsonObject object{{"id", sample.id}, {"kind", kindName(sample.kind)}, {"name", sample.name},
                           {"utilization_percent", metricJson(sample.utilization)},
                           {"temperature_celsius", metricJson(sample.temperature)}};
        if (sample.usedBytes) object.insert("used_bytes", static_cast<double>(*sample.usedBytes));
        if (sample.totalBytes) object.insert("total_bytes", static_cast<double>(*sample.totalBytes));
        resources.append(object);
    }
    return QJsonDocument(QJsonObject{{"timestamp", snapshot.timestamp.toString(Qt::ISODate)},
                                     {"paused", paused}, {"wsl", snapshot.isWsl}, {"resources", resources}});
}
} // namespace monitor

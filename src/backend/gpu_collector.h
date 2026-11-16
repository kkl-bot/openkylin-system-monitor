#pragma once
#include "backend/resource_collector.h"
#include "backend/hwmon_reader.h"

namespace monitor {
class GpuCollector final : public IResourceCollector {
public:
    explicit GpuCollector(const SourcePaths &paths = {});
    QVector<ResourceSample> collect() override;
    static QVector<ResourceSample> parseNvidiaOutput(const QString &csv);
private:
    QVector<ResourceSample> collectNvidia();
    QVector<ResourceSample> collectDrm(bool nvidiaAvailable) const;
    SourcePaths paths_;
    HwmonReader sensors_;
    QString nvidiaExecutable_;
    QString nvidiaError_;
};
} // namespace monitor

#pragma once
#include "backend/resource_collector.h"
#include "backend/hwmon_reader.h"

namespace monitor {
class MemoryCollector final : public IResourceCollector {
public:
    explicit MemoryCollector(const SourcePaths &paths = {});
    QVector<ResourceSample> collect() override;
private:
    SourcePaths paths_;
    HwmonReader sensors_;
};
} // namespace monitor

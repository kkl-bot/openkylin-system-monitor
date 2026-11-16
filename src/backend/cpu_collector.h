#pragma once
#include "backend/resource_collector.h"
#include "backend/hwmon_reader.h"
#include <array>

namespace monitor {
class CpuCollector final : public IResourceCollector {
public:
    explicit CpuCollector(const SourcePaths &paths = {});
    QVector<ResourceSample> collect() override;
private:
    SourcePaths paths_;
    HwmonReader sensors_;
    std::optional<std::array<quint64, 8>> previous_;
};
} // namespace monitor

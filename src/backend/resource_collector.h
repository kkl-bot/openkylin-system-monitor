#pragma once
#include "domain/metrics.h"

namespace monitor {
class IResourceCollector {
public:
    virtual ~IResourceCollector() = default;
    virtual QVector<ResourceSample> collect() = 0;
};
} // namespace monitor

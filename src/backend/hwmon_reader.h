#pragma once
#include "domain/metrics.h"

namespace monitor {
class HwmonReader {
public:
    explicit HwmonReader(const SourcePaths &paths);
    Metric temperature(ResourceKind kind) const;
    Metric deviceTemperature(const QString &devicePath) const;
private:
    Metric readSensors(const QStringList &directories) const;
    SourcePaths paths_;
};
} // namespace monitor

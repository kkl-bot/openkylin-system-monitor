#include "domain/metrics.h"
#include <cmath>

namespace monitor {
Metric Metric::available(double value, const QString &source)
{
    if (!std::isfinite(value))
        return unavailable(QStringLiteral("传感器返回无效读数"));
    return {value, {}, source};
}

Metric Metric::unavailable(const QString &reason)
{
    return {std::nullopt, reason, {}};
}

QString resourceLabel(ResourceKind kind)
{
    switch (kind) {
    case ResourceKind::Cpu: return QStringLiteral("CPU");
    case ResourceKind::Gpu: return QStringLiteral("GPU");
    case ResourceKind::Memory: return QStringLiteral("内存");
    }
    return {};
}

QString formatMetric(const Metric &metric, const QString &unit, int precision)
{
    return metric.value ? QString::number(*metric.value, 'f', precision) + unit
                        : QStringLiteral("—");
}

QString formatBytes(quint64 bytes)
{
    const double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    return gib >= 1.0 ? QString::number(gib, 'f', 1) + QStringLiteral(" GiB")
                      : QString::number(bytes / (1024.0 * 1024.0), 'f', 0) + QStringLiteral(" MiB");
}
} // namespace monitor

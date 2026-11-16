#include "backend/memory_collector.h"
#include "backend/file_reader.h"
#include <QRegularExpression>
#include <algorithm>
#include <limits>

namespace monitor {
MemoryCollector::MemoryCollector(const SourcePaths &paths) : paths_(paths), sensors_(paths) {}

QVector<ResourceSample> MemoryCollector::collect()
{
    ResourceSample sample;
    sample.id = "memory";
    sample.kind = ResourceKind::Memory;
    sample.name = QStringLiteral("物理内存");
    QMap<QString, quint64> values;
    for (const auto &line : files::readText(paths_.procRoot + "/meminfo").split('\n')) {
        const auto fields = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (fields.size() < 3 || fields[2] != "kB") continue;
        bool ok = false;
        const auto kb = fields[1].toULongLong(&ok);
        if (ok && !fields[1].startsWith('-') && kb <= std::numeric_limits<quint64>::max() / 1024)
            values.insert(fields[0].chopped(1), kb * 1024);
    }
    sample.utilization = Metric::unavailable(QStringLiteral("无法读取有效的 /proc/meminfo"));
    const quint64 total = values.value("MemTotal");
    if (total && (values.contains("MemAvailable") || values.contains("MemFree"))) {
        quint64 available = values.value("MemAvailable");
        if (!values.contains("MemAvailable")) {
            const auto reclaimable = values.value("MemFree") + values.value("Buffers") +
                                     values.value("Cached") + values.value("SReclaimable");
            available = reclaimable - std::min(reclaimable, values.value("Shmem"));
            sample.details.insert(QStringLiteral("统计方式"), QStringLiteral("可用内存估算（旧内核）"));
        } else sample.details.insert(QStringLiteral("统计方式"), QStringLiteral("总量 − 可用内存"));
        available = std::min(total, available);
        sample.totalBytes = total;
        sample.usedBytes = total - available;
        sample.utilization = Metric::available(100.0 * *sample.usedBytes / total, paths_.procRoot + "/meminfo");
        sample.details.insert(QStringLiteral("可用内存"), formatBytes(available));
        sample.details.insert(QStringLiteral("交换空间"), formatBytes(values.value("SwapTotal")));
    }
    sample.temperature = sensors_.temperature(ResourceKind::Memory);
    return {sample};
}
} // namespace monitor

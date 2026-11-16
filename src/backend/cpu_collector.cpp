#include "backend/cpu_collector.h"
#include "backend/file_reader.h"
#include <QRegularExpression>
#include <numeric>

namespace monitor {
CpuCollector::CpuCollector(const SourcePaths &paths) : paths_(paths), sensors_(paths) {}

QVector<ResourceSample> CpuCollector::collect()
{
    ResourceSample sample;
    sample.id = "cpu";
    sample.kind = ResourceKind::Cpu;
    sample.name = QStringLiteral("处理器");
    const auto info = files::readText(paths_.procRoot + "/cpuinfo");
    int logicalCount = 0;
    double mhz = 0;
    int mhzCount = 0;
    for (const auto &line : info.split('\n')) {
        const auto colon = line.indexOf(':');
        const auto key = line.left(colon).trimmed();
        const auto value = line.mid(colon + 1).trimmed();
        if (key == "model name" || key == "Hardware") sample.name = value;
        if (key == "processor") ++logicalCount;
        if (key == "cpu MHz") { mhz += value.toDouble(); ++mhzCount; }
    }
    sample.details.insert(QStringLiteral("逻辑处理器"), QString::number(logicalCount));
    if (mhzCount) sample.details.insert(QStringLiteral("平均频率"), QString::number(mhz / mhzCount / 1000, 'f', 2) + " GHz");
    const auto tokens = files::readText(paths_.procRoot + "/stat").section('\n', 0, 0)
                            .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    sample.utilization = Metric::unavailable(QStringLiteral("无法读取 /proc/stat"));
    if (tokens.size() >= 5 && tokens.first() == "cpu") {
        std::array<quint64, 8> ticks{};
        bool valid = true;
        for (int i = 0; i < 8 && i + 1 < tokens.size(); ++i) {
            bool ok = false;
            ticks[i] = tokens[i + 1].toULongLong(&ok);
            valid = valid && ok && !tokens[i + 1].startsWith('-');
        }
        if (valid) {
            sample.utilization = Metric::unavailable(QStringLiteral("正在等待下一次采样"));
            if (previous_) {
                quint64 totalDelta = 0;
                quint64 idleDelta = 0;
                bool reset = false;
                for (int i = 0; i < 8; ++i) {
                    // iowait is allowed to decrease; other counters resetting invalidates the interval.
                    if (ticks[i] < (*previous_)[i] && i != 4) reset = true;
                    const auto delta = ticks[i] >= (*previous_)[i] ? ticks[i] - (*previous_)[i] : 0;
                    totalDelta += delta;
                    if (i == 3 || i == 4) idleDelta += delta;
                }
                if (!reset && totalDelta > 0)
                    sample.utilization = Metric::available(100.0 * (totalDelta - idleDelta) / totalDelta,
                                                          paths_.procRoot + "/stat");
            }
            previous_ = ticks; // guest/guest_nice are already included in user/nice.
        } else previous_.reset();
    } else previous_.reset();
    sample.temperature = sensors_.temperature(ResourceKind::Cpu);
    return {sample};
}
} // namespace monitor

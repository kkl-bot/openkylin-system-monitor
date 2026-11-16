#include "backend/gpu_collector.h"
#include "backend/file_reader.h"
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

namespace monitor {
namespace {
QStringList csvFields(const QString &line)
{
    QStringList fields;
    QString field;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const auto ch = line[i];
        if (ch == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') { field += ch; ++i; }
            else quoted = !quoted;
        } else if (ch == ',' && !quoted) { fields.append(field.trimmed()); field.clear(); }
        else field += ch;
    }
    fields.append(field.trimmed());
    return quoted ? QStringList{} : fields;
}

Metric numericMetric(const QString &text, double minimum, double maximum)
{
    bool ok = false;
    const double value = text.toDouble(&ok);
    return ok && std::isfinite(value) && value >= minimum && value <= maximum
        ? Metric::available(value, QStringLiteral("nvidia-smi"))
        : Metric::unavailable(QStringLiteral("驱动未提供该指标（N/A 或不支持）"));
}
} // namespace

GpuCollector::GpuCollector(const SourcePaths &paths) : paths_(paths), sensors_(paths)
{
    nvidiaExecutable_ = paths.nvidiaSmi;
    if (nvidiaExecutable_.isEmpty()) {
        nvidiaExecutable_ = QStandardPaths::findExecutable("nvidia-smi");
        if (nvidiaExecutable_.isEmpty() && QFileInfo::exists("/usr/lib/wsl/lib/nvidia-smi"))
            nvidiaExecutable_ = "/usr/lib/wsl/lib/nvidia-smi";
    }
}

QVector<ResourceSample> GpuCollector::parseNvidiaOutput(const QString &csv)
{
    QVector<ResourceSample> result;
    for (const auto &line : csv.split('\n', Qt::SkipEmptyParts)) {
        const auto fields = csvFields(line);
        if (fields.size() != 6 || fields[0].isEmpty() || fields[0].startsWith('[')) continue;
        ResourceSample sample;
        sample.id = "gpu-nvidia-" + fields[0];
        sample.kind = ResourceKind::Gpu;
        sample.name = fields[1];
        sample.utilization = numericMetric(fields[2], 0, 100);
        sample.temperature = numericMetric(fields[3], -50, 200);
        const auto used = numericMetric(fields[4], 0, 1e9);
        const auto total = numericMetric(fields[5], 0, 1e9);
        if (used.value && total.value && *total.value > 0 && *used.value <= *total.value) {
            sample.usedBytes = static_cast<quint64>(*used.value * 1024 * 1024);
            sample.totalBytes = static_cast<quint64>(*total.value * 1024 * 1024);
        }
        sample.details.insert(QStringLiteral("驱动接口"), QStringLiteral("NVIDIA SMI"));
        sample.details.insert(QStringLiteral("设备标识"), fields[0]);
        result.append(sample);
    }
    return result;
}

QVector<ResourceSample> GpuCollector::collectNvidia()
{
    nvidiaError_.clear();
    if (nvidiaExecutable_.isEmpty()) return {};
    QProcess process;
    process.start(nvidiaExecutable_, {"--query-gpu=uuid,name,utilization.gpu,temperature.gpu,memory.used,memory.total",
                                      "--format=csv,noheader,nounits"});
    if (!process.waitForStarted(200) || !process.waitForFinished(700)) {
        process.kill();
        process.waitForFinished(200);
        nvidiaError_ = QStringLiteral("NVIDIA 查询不可用或超时");
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        nvidiaError_ = QStringLiteral("NVIDIA 驱动查询失败");
        return {};
    }
    return parseNvidiaOutput(QString::fromUtf8(process.readAllStandardOutput()));
}

QVector<ResourceSample> GpuCollector::collectDrm(bool nvidiaAvailable) const
{
    QVector<ResourceSample> result;
    const QRegularExpression cardPattern("^card[0-9]+$");
    for (const auto &card : files::directories(paths_.sysRoot + "/class/drm", "card*")) {
        if (!cardPattern.match(QFileInfo(card).fileName()).hasMatch()) continue;
        const QString device = card + "/device";
        const QString vendor = files::readText(device + "/vendor").toLower();
        if (vendor == "0x10de" && nvidiaAvailable) continue;
        ResourceSample sample;
        sample.id = "gpu-" + QFileInfo(card).fileName();
        sample.kind = ResourceKind::Gpu;
        const QString brand = vendor == "0x1002" ? "AMD" : vendor == "0x8086" ? "Intel" : vendor == "0x10de" ? "NVIDIA" : "GPU";
        const QString product = files::readText(device + "/product_name");
        sample.name = product.isEmpty() ? brand + " · " + QFileInfo(card).fileName() : product;
        sample.utilization = Metric::unavailable(QStringLiteral("当前驱动未暴露 GPU 利用率接口"));
        const auto busy = files::readUnsigned(device + "/gpu_busy_percent");
        if (busy && *busy <= 100)
            sample.utilization = Metric::available(*busy, device + "/gpu_busy_percent");
        sample.temperature = sensors_.deviceTemperature(device);
        const auto used = files::readUnsigned(device + "/mem_info_vram_used");
        const auto total = files::readUnsigned(device + "/mem_info_vram_total");
        if (total && used && *total > 0 && *used <= *total) {
            sample.usedBytes = used;
            sample.totalBytes = total;
        }
        sample.details.insert(QStringLiteral("驱动接口"), QStringLiteral("Linux DRM / sysfs"));
        sample.details.insert(QStringLiteral("设备编号"), vendor + " : " + files::readText(device + "/device"));
        result.append(sample);
    }
    return result;
}

QVector<ResourceSample> GpuCollector::collect()
{
    auto result = collectNvidia();
    result += collectDrm(!result.isEmpty());
    if (result.isEmpty()) {
        ResourceSample sample;
        sample.id = "gpu-unavailable";
        sample.kind = ResourceKind::Gpu;
        sample.name = QStringLiteral("GPU · 暂不可用");
        const QString reason = nvidiaError_.isEmpty() ? QStringLiteral("系统未暴露 GPU 监测接口；WSL 中可能不可用") : nvidiaError_;
        sample.utilization = Metric::unavailable(reason);
        sample.temperature = Metric::unavailable(reason);
        result.append(sample);
    }
    return result;
}
} // namespace monitor

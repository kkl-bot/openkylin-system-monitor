#include "backend/hwmon_reader.h"
#include "backend/file_reader.h"
#include <QDir>
#include <QRegularExpression>

namespace monitor {
HwmonReader::HwmonReader(const SourcePaths &paths) : paths_(paths) {}

Metric HwmonReader::readSensors(const QStringList &directories) const
{
    Metric result = Metric::unavailable(QStringLiteral("硬件或驱动未提供温度传感器"));
    const QRegularExpression channel(QStringLiteral("^temp[0-9]+_input$"));
    for (const auto &path : directories) {
        const QDir dir(path);
        for (const auto &name : dir.entryList({QStringLiteral("temp*_input")}, QDir::Files)) {
            if (!channel.match(name).hasMatch()) continue;
            const auto stem = name.left(name.size() - 5);
            if (files::readText(dir.filePath(stem + "fault")) == "1") continue;
            const auto enabled = files::readText(dir.filePath(stem + "enable"));
            if (enabled == "0") continue;
            bool ok = false;
            const double value = files::readText(dir.filePath(name)).toDouble(&ok) / 1000.0;
            if (ok && value >= -50 && value <= 200 && (!result.value || value > *result.value))
                result = Metric::available(value, dir.filePath(name));
        }
    }
    return result;
}

Metric HwmonReader::temperature(ResourceKind kind) const
{
    QStringList matches;
    const QStringList cpuDrivers = {"coretemp", "k10temp", "zenpower", "cpu_thermal", "soc_thermal"};
    const QStringList memoryDrivers = {"jc42", "spd5118", "spd5hub"};
    for (const auto &dir : files::directories(paths_.sysRoot + "/class/hwmon", "hwmon*")) {
        const QString name = files::readText(dir + "/name").toLower();
        if ((kind == ResourceKind::Cpu && cpuDrivers.contains(name)) ||
            (kind == ResourceKind::Memory && memoryDrivers.contains(name)))
            matches.append(dir);
    }
    Metric result = readSensors(matches);
    if (kind == ResourceKind::Cpu && !result.value) {
        for (const auto &dir : files::directories(paths_.sysRoot + "/class/thermal", "thermal_zone*")) {
            const QString type = files::readText(dir + "/type").toLower();
            if (!type.contains("cpu") && type != "x86_pkg_temp") continue;
            bool ok = false;
            const double value = files::readText(dir + "/temp").toDouble(&ok) / 1000.0;
            if (ok && value >= -50 && value <= 200 && (!result.value || value > *result.value))
                result = Metric::available(value, dir + "/temp");
        }
    }
    if (!result.value && kind == ResourceKind::Memory)
        result.reason = QStringLiteral("未检测到内存温度传感器（需要支持温度读取的 DIMM 与驱动）");
    return result;
}

Metric HwmonReader::deviceTemperature(const QString &devicePath) const
{
    return readSensors(files::directories(devicePath + "/hwmon", "hwmon*"));
}
} // namespace monitor

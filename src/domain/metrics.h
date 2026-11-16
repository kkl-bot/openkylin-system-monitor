#pragma once

#include <QDateTime>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <optional>

namespace monitor {

enum class ResourceKind { Cpu, Gpu, Memory };

struct Metric {
    std::optional<double> value;
    QString reason;
    QString source;
    static Metric available(double value, const QString &source);
    static Metric unavailable(const QString &reason);
};

struct ResourceSample {
    QString id;
    ResourceKind kind = ResourceKind::Cpu;
    QString name;
    Metric utilization;
    Metric temperature;
    std::optional<quint64> usedBytes;
    std::optional<quint64> totalBytes;
    QMap<QString, QString> details;
};

struct Snapshot {
    QDateTime timestamp;
    QVector<ResourceSample> resources;
    bool isWsl = false;
};

struct SourcePaths {
    QString procRoot = QStringLiteral("/proc");
    QString sysRoot = QStringLiteral("/sys");
    // Empty: auto-discover. Explicit paths also allow isolated test fixtures.
    QString nvidiaSmi;
};

QString resourceLabel(ResourceKind kind);
QString formatMetric(const Metric &metric, const QString &unit, int precision = 0);
QString formatBytes(quint64 bytes);

} // namespace monitor
Q_DECLARE_METATYPE(monitor::Snapshot)

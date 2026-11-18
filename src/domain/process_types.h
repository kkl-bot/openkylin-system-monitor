#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <optional>

namespace monitor {

struct ProcessIdentity {
    qint64 pid = 0;
    quint64 startTimeTicks = 0;
    quint32 uid = 0;
};

bool operator==(const ProcessIdentity &left, const ProcessIdentity &right);
bool operator!=(const ProcessIdentity &left, const ProcessIdentity &right);

struct ProcessInfo {
    ProcessIdentity identity;
    qint64 parentPid = 0;
    QString name;
    QString user;
    QString state;
    QString commandLine;
    QString executable;
    std::optional<double> cpuPercent;
    std::optional<double> memoryPercent;
    std::optional<double> readBytesPerSecond;
    std::optional<double> writeBytesPerSecond;
    quint64 residentBytes = 0;
    int threads = 0;
    bool canTerminate = false;
};

struct ProcessSnapshot {
    QDateTime timestamp;
    QVector<ProcessInfo> processes;
    QString error;
};

} // namespace monitor

Q_DECLARE_METATYPE(monitor::ProcessIdentity)
Q_DECLARE_METATYPE(monitor::ProcessSnapshot)

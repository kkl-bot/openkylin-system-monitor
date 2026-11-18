#pragma once

#include "domain/process_types.h"
#include <QHash>
#include <array>

namespace monitor {

class ProcessCollector final {
public:
    explicit ProcessCollector(const QString &procRoot = QStringLiteral("/proc"));
    ProcessSnapshot collect(qint64 monotonicMilliseconds);

    // processDirectory may refer to an already opened /proc/PID directory via /proc/self/fd.
    static std::optional<ProcessInfo> inspect(const QString &processDirectory, qint64 expectedPid);

private:
    struct PreviousSample {
        ProcessIdentity identity;
        quint64 cpuTicks = 0;
        std::optional<quint64> readBytes;
        std::optional<quint64> writeBytes;
        qint64 milliseconds = 0;
    };

    QString userName(quint32 uid);

    QString procRoot_;
    QHash<qint64, PreviousSample> previous_;
    std::optional<std::array<quint64, 8>> previousSystem_;
    std::optional<qint64> previousMilliseconds_;
    QHash<quint32, QString> userNames_;
};

} // namespace monitor

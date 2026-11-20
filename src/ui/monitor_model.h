#pragma once
#include "domain/metrics.h"
#include <QHash>
#include <QObject>

namespace monitor::ui {
struct HistoryPoint {
    QDateTime timestamp;
    std::optional<double> utilization;
    std::optional<double> temperature;
};

class MonitorModel final : public QObject {
    Q_OBJECT
public:
    explicit MonitorModel(QObject *parent = nullptr);
    void acceptSnapshot(const Snapshot &snapshot);
    const Snapshot &snapshot() const;
    const ResourceSample *resource(const QString &id) const;
    QString resourceTitle(const ResourceSample &sample) const;
    QVector<HistoryPoint> history(const QString &id) const;
signals:
    void changed();
private:
    Snapshot snapshot_;
    QHash<QString, QVector<HistoryPoint>> histories_;
};
} // namespace monitor::ui

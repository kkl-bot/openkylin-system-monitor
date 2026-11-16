#pragma once
#include "domain/metrics.h"
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QWaitCondition>

namespace monitor {
class SamplingThread final : public QThread {
    Q_OBJECT
public:
    explicit SamplingThread(QObject *parent = nullptr);
    ~SamplingThread() override;
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void stop();
signals:
    void sampled(const monitor::Snapshot &snapshot);
protected:
    void run() override;
private:
    QMutex mutex_;
    QWaitCondition wake_;
    bool paused_ = false;
    bool stopping_ = false;
    int interval_ = 1000;
};

class MonitorService final : public QObject {
    Q_OBJECT
public:
    explicit MonitorService(QObject *parent = nullptr);
    ~MonitorService() override;
    void start();
    void setPaused(bool paused);
    void setInterval(int milliseconds);
signals:
    void snapshotReady(const monitor::Snapshot &snapshot);
private:
    SamplingThread thread_;
};
} // namespace monitor

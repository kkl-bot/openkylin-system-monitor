#pragma once

#include "backend/process_sampling_thread.h"
#include <QObject>

namespace monitor {

class ProcessService final : public QObject {
    Q_OBJECT
public:
    explicit ProcessService(QObject *parent = nullptr);
    explicit ProcessService(const QString &procRoot, QObject *parent = nullptr);
    ~ProcessService() override;

    void start();
    void setActive(bool active);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void requestRefresh();

signals:
    void snapshotReady(const monitor::ProcessSnapshot &snapshot);

private:
    void updateWorkerState();
    void acceptSnapshot(const monitor::ProcessSnapshot &snapshot, quint64 generation,
                        bool requestedRefresh);

    ProcessSamplingThread thread_;
    bool started_ = false;
    bool active_ = false;
    bool paused_ = false;
    bool refreshPending_ = false;
    int interval_ = 1000;
    quint64 generation_ = 0;
};

} // namespace monitor

#pragma once

#include "domain/process_types.h"
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

namespace monitor {

class ProcessSamplingThread final : public QThread {
    Q_OBJECT
public:
    explicit ProcessSamplingThread(const QString &procRoot, QObject *parent = nullptr);
    ~ProcessSamplingThread() override;

    void updateState(bool active, bool paused, int interval, quint64 generation);
    void requestRefresh(quint64 generation);
    void stop();

signals:
    void sampled(const monitor::ProcessSnapshot &snapshot, quint64 generation,
                 bool requestedRefresh);

protected:
    void run() override;

private:
    const QString procRoot_;
    QMutex mutex_;
    QWaitCondition wake_;
    bool active_ = false;
    bool paused_ = false;
    bool stopping_ = false;
    bool refreshPending_ = false;
    bool reschedule_ = false;
    int interval_ = 1000;
    quint64 generation_ = 0;
    quint64 baselineEpoch_ = 0;
};

} // namespace monitor

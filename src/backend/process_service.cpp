#include "backend/process_service.h"

namespace monitor {

ProcessService::ProcessService(QObject *parent)
    : ProcessService(QStringLiteral("/proc"), parent)
{
}

ProcessService::ProcessService(const QString &procRoot, QObject *parent)
    : QObject(parent), thread_(procRoot)
{
    qRegisterMetaType<monitor::ProcessSnapshot>();
    connect(&thread_, &ProcessSamplingThread::sampled,
            this, &ProcessService::acceptSnapshot, Qt::QueuedConnection);
}

ProcessService::~ProcessService()
{
    thread_.stop();
}

void ProcessService::start()
{
    if (started_)
        return;
    started_ = true;
    thread_.start();
}

void ProcessService::setActive(bool active)
{
    if (active_ == active)
        return;
    active_ = active;
    refreshPending_ = false;
    ++generation_;
    updateWorkerState();
}

void ProcessService::setPaused(bool paused)
{
    if (paused_ == paused)
        return;
    paused_ = paused;
    refreshPending_ = false;
    ++generation_;
    updateWorkerState();
}

void ProcessService::setInterval(int milliseconds)
{
    if (milliseconds != 500 && milliseconds != 1000
        && milliseconds != 2000 && milliseconds != 5000)
        return;
    if (interval_ == milliseconds)
        return;
    interval_ = milliseconds;
    updateWorkerState();
}

void ProcessService::requestRefresh()
{
    if (!active_)
        return;
    refreshPending_ = true;
    ++generation_;
    thread_.requestRefresh(generation_);
}

void ProcessService::updateWorkerState()
{
    thread_.updateState(active_, paused_, interval_, generation_);
}

void ProcessService::acceptSnapshot(const monitor::ProcessSnapshot &snapshot,
                                  quint64 generation, bool requestedRefresh)
{
    // A sample may already be queued when the user hides or pauses the page.
    // Reject it here as well as in the worker, after returning to the GUI thread.
    if (!active_ || generation != generation_)
        return;
    if (paused_ && (!requestedRefresh || !refreshPending_))
        return;
    if (requestedRefresh)
        refreshPending_ = false;
    emit snapshotReady(snapshot);
}

} // namespace monitor

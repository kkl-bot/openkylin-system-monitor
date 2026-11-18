#include "backend/process_sampling_thread.h"
#include "backend/process_collector.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <memory>

namespace monitor {

ProcessSamplingThread::ProcessSamplingThread(const QString &procRoot, QObject *parent)
    : QThread(parent), procRoot_(procRoot)
{
}

ProcessSamplingThread::~ProcessSamplingThread()
{
    stop();
}

void ProcessSamplingThread::updateState(bool active, bool paused, int interval,
                                        quint64 generation)
{
    QMutexLocker lock(&mutex_);
    if (active_ != active || paused_ != paused) {
        ++baselineEpoch_;
        refreshPending_ = false;
    }
    active_ = active;
    paused_ = paused;
    interval_ = interval;
    generation_ = generation;
    reschedule_ = true;
    wake_.wakeAll();
}

void ProcessSamplingThread::requestRefresh(quint64 generation)
{
    QMutexLocker lock(&mutex_);
    if (!active_ || stopping_)
        return;
    generation_ = generation;
    refreshPending_ = true;
    wake_.wakeAll();
}

void ProcessSamplingThread::stop()
{
    {
        QMutexLocker lock(&mutex_);
        stopping_ = true;
        wake_.wakeAll();
    }
    wait();
}

void ProcessSamplingThread::run()
{
    QElapsedTimer clock;
    clock.start();
    std::unique_ptr<ProcessCollector> collector;
    quint64 collectorEpoch = 0;
    qint64 nextSamplingAt = 0;

    for (;;) {
        quint64 sampleGeneration = 0;
        quint64 sampleEpoch = 0;
        bool requestedRefresh = false;
        {
            QMutexLocker lock(&mutex_);
            for (;;) {
                if (stopping_)
                    return;
                if (reschedule_) {
                    nextSamplingAt = 0;
                    reschedule_ = false;
                }
                if (!active_ || (paused_ && !refreshPending_)) {
                    wake_.wait(&mutex_);
                    continue;
                }
                const qint64 remaining = nextSamplingAt - clock.elapsed();
                if (refreshPending_ || remaining <= 0)
                    break;
                wake_.wait(&mutex_, static_cast<unsigned long>(remaining));
            }
            sampleGeneration = generation_;
            sampleEpoch = baselineEpoch_;
            requestedRefresh = refreshPending_;
            refreshPending_ = false;
        }

        // Reconstructing the collector discards rate baselines across hidden or
        // paused periods. All filesystem reads stay on this worker thread.
        if (!collector || collectorEpoch != sampleEpoch) {
            collector = std::make_unique<ProcessCollector>(procRoot_);
            collectorEpoch = sampleEpoch;
        }
        const ProcessSnapshot snapshot = collector->collect(clock.elapsed());
        bool deliver = false;
        {
            QMutexLocker lock(&mutex_);
            deliver = !stopping_ && active_ && generation_ == sampleGeneration
                && (!paused_ || requestedRefresh);
            nextSamplingAt = clock.elapsed() + interval_;
        }
        if (deliver)
            emit sampled(snapshot, sampleGeneration, requestedRefresh);
    }
}

} // namespace monitor

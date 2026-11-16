#include "backend/monitor_service.h"
#include "backend/cpu_collector.h"
#include "backend/memory_collector.h"
#include "backend/gpu_collector.h"
#include "backend/file_reader.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <memory>
#include <vector>

namespace monitor {
SamplingThread::SamplingThread(QObject *parent) : QThread(parent) {}
SamplingThread::~SamplingThread() { stop(); }
void SamplingThread::stop()
{
    { QMutexLocker lock(&mutex_); stopping_ = true; wake_.wakeAll(); }
    wait();
}
void SamplingThread::setPaused(bool paused)
{
    QMutexLocker lock(&mutex_);
    paused_ = paused;
    wake_.wakeAll();
}
void SamplingThread::setInterval(int milliseconds)
{
    QMutexLocker lock(&mutex_);
    interval_ = qBound(500, milliseconds, 5000);
    wake_.wakeAll();
}
void SamplingThread::run()
{
    std::vector<std::unique_ptr<IResourceCollector>> collectors;
    collectors.push_back(std::make_unique<CpuCollector>());
    collectors.push_back(std::make_unique<GpuCollector>());
    collectors.push_back(std::make_unique<MemoryCollector>());
    const bool isWsl = files::readText("/proc/version").contains("microsoft", Qt::CaseInsensitive);
    for (;;) {
        {
            QMutexLocker lock(&mutex_);
            while (paused_ && !stopping_) wake_.wait(&mutex_);
            if (stopping_) break;
        }
        QElapsedTimer elapsed;
        elapsed.start();
        Snapshot snapshot;
        snapshot.isWsl = isWsl;
        for (auto &collector : collectors) snapshot.resources += collector->collect();
        snapshot.timestamp = QDateTime::currentDateTimeUtc();
        { QMutexLocker lock(&mutex_); if (!paused_ && !stopping_) emit sampled(snapshot); }
        QMutexLocker lock(&mutex_);
        if (stopping_) break;
        wake_.wait(&mutex_, static_cast<unsigned long>(qMax<qint64>(50, interval_ - elapsed.elapsed())));
    }
}
MonitorService::MonitorService(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<monitor::Snapshot>();
    connect(&thread_, &SamplingThread::sampled, this, &MonitorService::snapshotReady, Qt::QueuedConnection);
}
MonitorService::~MonitorService() { thread_.stop(); }
void MonitorService::start() { if (!thread_.isRunning()) thread_.start(); }
void MonitorService::setPaused(bool paused) { thread_.setPaused(paused); }
void MonitorService::setInterval(int milliseconds) { thread_.setInterval(milliseconds); }
} // namespace monitor

// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/network_sampling_thread.h"
#include "backend/network_collector.h"
#include "backend/proxy_reader.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <memory>
#include <utility>

namespace monitor {
namespace {

NetworkSampler defaultSampler()
{
    const auto collector = std::make_shared<NetworkCollector>();
    return [collector](qint64 elapsed, bool refreshConfiguration) {
        if (refreshConfiguration)
            collector->requestDetailsRefresh();
        return collector->collect(elapsed);
    };
}

ProxyInfo defaultProxy()
{
    return ProxyReader().read();
}

} // namespace

NetworkSamplingThread::NetworkSamplingThread(NetworkSamplerFactory factory,
                                             NetworkProxyProvider proxyProvider, QObject *parent)
    : QThread(parent), factory_(factory ? std::move(factory) : NetworkSamplerFactory(defaultSampler)),
      proxyProvider_(proxyProvider ? std::move(proxyProvider) : NetworkProxyProvider(defaultProxy))
{
}

NetworkSamplingThread::~NetworkSamplingThread()
{
    stop();
}

void NetworkSamplingThread::updateState(bool active, bool paused, int interval, quint64 generation)
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

void NetworkSamplingThread::requestRefresh(quint64 generation)
{
    QMutexLocker lock(&mutex_);
    if (!active_ || stopping_)
        return;
    generation_ = generation;
    refreshPending_ = true;
    wake_.wakeAll();
}

void NetworkSamplingThread::stop()
{
    {
        QMutexLocker lock(&mutex_);
        stopping_ = true;
        wake_.wakeAll();
    }
    wait();
}

void NetworkSamplingThread::run()
{
    QElapsedTimer clock;
    clock.start();
    NetworkSampler sampler;
    ProxyInfo proxy;
    quint64 samplerEpoch = 0;
    qint64 nextSamplingAt = 0;
    qint64 nextProxyAt = 0;

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

        if (!sampler || samplerEpoch != sampleEpoch) {
            sampler = factory_();
            samplerEpoch = sampleEpoch;
            nextProxyAt = 0;
        }
        NetworkSnapshot snapshot = sampler(clock.elapsed(), requestedRefresh);
        {
            QMutexLocker lock(&mutex_);
            if (stopping_)
                return;
            if (!active_ || generation_ != sampleGeneration || (paused_ && !requestedRefresh))
                continue;
        }
        if (requestedRefresh || clock.elapsed() >= nextProxyAt) {
            proxy = proxyProvider_();
            nextProxyAt = clock.elapsed() + 5000;
        }
        snapshot.proxy = proxy;
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

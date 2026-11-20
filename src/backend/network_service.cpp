// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/network_service.h"
#include <utility>

namespace monitor {

NetworkService::NetworkService(QObject *parent)
    : NetworkService(NetworkSamplerFactory{}, NetworkProxyProvider{}, parent)
{
}

NetworkService::NetworkService(NetworkSamplerFactory factory, NetworkProxyProvider proxyProvider,
                               QObject *parent)
    : QObject(parent), thread_(std::move(factory), std::move(proxyProvider))
{
    qRegisterMetaType<monitor::NetworkSnapshot>();
    connect(&thread_, &NetworkSamplingThread::sampled,
            this, &NetworkService::acceptSnapshot, Qt::QueuedConnection);
}

NetworkService::~NetworkService()
{
    thread_.stop();
}

void NetworkService::start()
{
    if (started_)
        return;
    started_ = true;
    thread_.start();
}

void NetworkService::setActive(bool active)
{
    if (active_ == active)
        return;
    active_ = active;
    refreshPending_ = false;
    ++generation_;
    updateWorkerState();
}

void NetworkService::setPaused(bool paused)
{
    if (paused_ == paused)
        return;
    paused_ = paused;
    refreshPending_ = false;
    ++generation_;
    updateWorkerState();
}

void NetworkService::setInterval(int milliseconds)
{
    if (milliseconds != 500 && milliseconds != 1000 && milliseconds != 2000 && milliseconds != 5000)
        return;
    if (interval_ == milliseconds)
        return;
    interval_ = milliseconds;
    updateWorkerState();
}

void NetworkService::requestRefresh()
{
    if (!active_)
        return;
    refreshPending_ = true;
    ++generation_;
    thread_.requestRefresh(generation_);
}

void NetworkService::updateWorkerState()
{
    thread_.updateState(active_, paused_, interval_, generation_);
}

void NetworkService::acceptSnapshot(const monitor::NetworkSnapshot &snapshot,
                                   quint64 generation, bool requestedRefresh)
{
    if (!active_ || generation != generation_)
        return;
    if (paused_ && (!requestedRefresh || !refreshPending_))
        return;
    if (requestedRefresh)
        refreshPending_ = false;
    emit snapshotReady(snapshot);
}

} // namespace monitor

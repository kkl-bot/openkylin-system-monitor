// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "domain/network_types.h"
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <functional>

namespace monitor {

// Factories are invoked on the worker. A new sampler discards rate baselines.
// The bool requests a fresh configuration read without discarding rate deltas.
using NetworkSampler = std::function<NetworkSnapshot(qint64, bool)>;
using NetworkSamplerFactory = std::function<NetworkSampler()>;
using NetworkProxyProvider = std::function<ProxyInfo()>;

class NetworkSamplingThread final : public QThread {
    Q_OBJECT
public:
    explicit NetworkSamplingThread(NetworkSamplerFactory factory = {},
                                   NetworkProxyProvider proxyProvider = {}, QObject *parent = nullptr);
    ~NetworkSamplingThread() override;

    void updateState(bool active, bool paused, int interval, quint64 generation);
    void requestRefresh(quint64 generation);
    void stop();

signals:
    void sampled(const monitor::NetworkSnapshot &snapshot, quint64 generation, bool requestedRefresh);

protected:
    void run() override;

private:
    NetworkSamplerFactory factory_;
    NetworkProxyProvider proxyProvider_;
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

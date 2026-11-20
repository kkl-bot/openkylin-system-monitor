// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "backend/network_sampling_thread.h"
#include <QObject>

namespace monitor {

class NetworkService final : public QObject {
    Q_OBJECT
public:
    explicit NetworkService(QObject *parent = nullptr);
    explicit NetworkService(NetworkSamplerFactory factory, NetworkProxyProvider proxyProvider = {},
                            QObject *parent = nullptr);
    ~NetworkService() override;

    void start();
    void setActive(bool active);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void requestRefresh();

signals:
    void snapshotReady(const monitor::NetworkSnapshot &snapshot);

private:
    void updateWorkerState();
    void acceptSnapshot(const monitor::NetworkSnapshot &snapshot, quint64 generation, bool requestedRefresh);
    NetworkSamplingThread thread_;
    bool started_ = false;
    bool active_ = false;
    bool paused_ = false;
    bool refreshPending_ = false;
    int interval_ = 1000;
    quint64 generation_ = 0;
};

} // namespace monitor

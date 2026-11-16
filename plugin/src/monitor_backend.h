// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "domain/metrics.h"
#include <QObject>
#include <QPointer>
#include <QVariantList>

namespace monitor::widget {
class SamplingHub;
class MonitorBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList resources READ resources NOTIFY resourcesChanged)
    Q_PROPERTY(QVariantList gpus READ gpus NOTIFY resourcesChanged)
    Q_PROPERTY(QString tooltip READ tooltip NOTIFY resourcesChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(QString selectedGpu READ selectedGpu WRITE setSelectedGpu NOTIFY selectedGpuChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
public:
    explicit MonitorBackend(QObject *parent = nullptr);
    ~MonitorBackend() override;
    QVariantList resources() const;
    QVariantList gpus() const;
    QString tooltip() const;
    bool active() const;
    bool paused() const;
    int interval() const;
    QString selectedGpu() const;
    QString lastError() const;
    void setActive(bool value);
    void setPaused(bool value);
    void setInterval(int value);
    void setSelectedGpu(const QString &value);
    Q_INVOKABLE bool openDetails();
    // Public value-type input makes hardware parsing and frontend projection independently testable.
    void acceptSnapshot(const Snapshot &snapshot);
signals:
    void resourcesChanged();
    void activeChanged();
    void pausedChanged();
    void intervalChanged();
    void selectedGpuChanged();
    void lastErrorChanged();
private:
    void updateSubscription();
    Snapshot snapshot_;
    QPointer<SamplingHub> hub_;
    bool active_ = false;
    bool paused_ = false;
    int interval_ = 1000;
    QString selectedGpu_;
    QString lastError_;
};
} // namespace monitor::widget

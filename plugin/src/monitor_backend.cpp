// SPDX-License-Identifier: GPL-3.0-or-later
#include "monitor_backend.h"
#include "sampling_hub.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace monitor::widget {
namespace {
QVariantMap projected(const ResourceSample &sample)
{
    return {{"id", sample.id}, {"name", sample.name}, {"label", resourceLabel(sample.kind)},
            {"kind", static_cast<int>(sample.kind)},
            {"utilization", sample.utilization.value ? QVariant(*sample.utilization.value) : QVariant()},
            {"temperature", sample.temperature.value ? QVariant(*sample.temperature.value) : QVariant()},
            {"utilizationReason", sample.utilization.reason}, {"temperatureReason", sample.temperature.reason}};
}
} // namespace
MonitorBackend::MonitorBackend(QObject *parent) : QObject(parent) {}
MonitorBackend::~MonitorBackend() { if (hub_) hub_->unsubscribe(this); }
void MonitorBackend::updateSubscription()
{
    if (!hub_ && active_ && !paused_) {
        hub_ = SamplingHub::instance();
        connect(hub_, &SamplingHub::sampled, this, [this](const Snapshot &snapshot) {
            if (active_ && !paused_) acceptSnapshot(snapshot);
        });
    }
    if (!hub_) return;
    if (active_ && !paused_) {
        if (snapshot_.resources.isEmpty()) acceptSnapshot(hub_->latest());
        hub_->subscribe(this, interval_);
    } else hub_->unsubscribe(this);
}
bool MonitorBackend::active() const { return active_; }
bool MonitorBackend::paused() const { return paused_; }
int MonitorBackend::interval() const { return interval_; }
QString MonitorBackend::selectedGpu() const { return selectedGpu_; }
QString MonitorBackend::lastError() const { return lastError_; }
void MonitorBackend::setActive(bool value)
{
    if (active_ == value) return;
    active_ = value; updateSubscription(); emit activeChanged();
}
void MonitorBackend::setPaused(bool value)
{
    if (paused_ == value) return;
    paused_ = value; updateSubscription(); emit pausedChanged(); emit resourcesChanged();
}
void MonitorBackend::setInterval(int value)
{
    if (value != 500 && value != 1000 && value != 2000 && value != 5000) value = 1000;
    if (interval_ == value) return;
    interval_ = value; updateSubscription(); emit intervalChanged();
}
void MonitorBackend::setSelectedGpu(const QString &value)
{
    if (selectedGpu_ == value) return;
    selectedGpu_ = value; emit selectedGpuChanged(); emit resourcesChanged();
}
void MonitorBackend::acceptSnapshot(const Snapshot &snapshot) { snapshot_ = snapshot; emit resourcesChanged(); }
QVariantList MonitorBackend::resources() const
{
    const ResourceSample *cpu = nullptr, *gpu = nullptr, *memory = nullptr;
    for (const auto &sample : snapshot_.resources) {
        if (sample.kind == ResourceKind::Cpu) cpu = &sample;
        if (sample.kind == ResourceKind::Memory) memory = &sample;
        if (sample.kind == ResourceKind::Gpu && (!gpu || sample.id == selectedGpu_)) gpu = &sample;
    }
    QVariantList result;
    for (auto *sample : {cpu, gpu, memory}) result.append(sample ? projected(*sample) : QVariantMap{});
    return result;
}
QVariantList MonitorBackend::gpus() const
{
    QVariantList result;
    for (const auto &sample : snapshot_.resources) if (sample.kind == ResourceKind::Gpu) result.append(projected(sample));
    return result;
}
QString MonitorBackend::tooltip() const
{
    QStringList text{tr("从左到右：CPU、GPU、内存；每组左柱利用率，右柱温度（0–120 °C）")};
    const auto projectedResources = resources();
    for (const auto &entry : projectedResources) {
        const auto map = entry.toMap(); if (map.isEmpty()) continue;
        const auto usage = map.value("utilization"), temperature = map.value("temperature");
        text.append(map.value("label").toString() + " · " + map.value("name").toString() + "\n" +
                    (usage.isValid() ? QString::number(usage.toDouble(), 'f', 1) + "%" : map.value("utilizationReason").toString()) + " / " +
                    (temperature.isValid() ? QString::number(temperature.toDouble(), 'f', 1) + " °C" : map.value("temperatureReason").toString()));
    }
    if (snapshot_.isWsl) text.append(tr("CPU / 内存反映 WSL 环境"));
    if (paused_) text.append(tr("已暂停，显示最后一次读数"));
    text.append(tr("斜纹表示暂无读数；双击查看详情，右键打开系统菜单"));
    if (!lastError_.isEmpty()) text.append(lastError_);
    return text.join('\n');
}
bool MonitorBackend::openDetails()
{
    QString executable = QFileInfo::exists("/usr/bin/kylin-monitor") ? QStringLiteral("/usr/bin/kylin-monitor")
                                                                 : QStandardPaths::findExecutable("kylin-monitor");
    const bool ok = !executable.isEmpty() && QProcess::startDetached(executable, {"--detail"});
    lastError_ = ok ? QString() : tr("无法打开详细窗口，请确认 kylin-monitor 已正确安装");
    emit lastErrorChanged(); emit resourcesChanged(); return ok;
}
} // namespace monitor::widget

#include "app/snapshot_endpoint.h"
#include "domain/snapshot_json.h"
#include "ui/monitor_model.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>

namespace monitor {
namespace {
const QString serviceName = QStringLiteral("org.openKylin.KylinMonitor");
const QString objectPath = QStringLiteral("/org/openKylin/KylinMonitor");
} // namespace

SnapshotEndpoint::SnapshotEndpoint(const ui::MonitorModel &model) : model_(model) {}

void SnapshotEndpoint::publish()
{
    if (registered_) return;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.registerObject(objectPath, this, QDBusConnection::ExportScriptableSlots)) return;
    // A second window in another process can take over when the first exits.
    const auto reply = bus.interface()->registerService(serviceName, QDBusConnectionInterface::QueueService);
    registered_ = reply.isValid() && reply.value() != QDBusConnectionInterface::ServiceNotRegistered;
    if (!registered_) bus.unregisterObject(objectPath);
}

SnapshotEndpoint::~SnapshotEndpoint()
{
    if (registered_) {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
    }
}

void SnapshotEndpoint::setPaused(bool paused) { paused_ = paused; }

QString SnapshotEndpoint::GetSnapshot() const
{
    return QString::fromUtf8(snapshotJson(model_.snapshot(), paused_).toJson(QJsonDocument::Compact));
}
} // namespace monitor

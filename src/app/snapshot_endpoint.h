#pragma once

#include <QObject>

namespace monitor {
namespace ui { class MonitorModel; }

class SnapshotEndpoint final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.openKylin.KylinMonitor1")
public:
    explicit SnapshotEndpoint(const ui::MonitorModel &model);
    ~SnapshotEndpoint() override;
    void publish();
    void setPaused(bool paused);
public slots:
    Q_SCRIPTABLE QString GetSnapshot() const;
private:
    const ui::MonitorModel &model_;
    bool registered_ = false;
    bool paused_ = false;
};
} // namespace monitor

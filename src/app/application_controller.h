#pragma once
#include "backend/monitor_service.h"
#include "ui/monitor_model.h"
#include "ui/detail_window.h"
#include "ui/mini_window.h"
#include "app/process_controller.h"
#include "app/snapshot_endpoint.h"
#include <QObject>
#include <QSettings>
class QSystemTrayIcon;

namespace monitor {
class ApplicationController final : public QObject {
    Q_OBJECT
public:
    explicit ApplicationController(QObject *parent = nullptr);
    ~ApplicationController() override;
    void start(bool forceMini, bool forceDetail);
    void showDetail();
    void showMini();
    void showProcesses();
    bool savePreviews(const QString &directory);
private:
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void saveSettings();
    QSettings settings_;
    MonitorService service_;
    ui::MonitorModel model_;
    SnapshotEndpoint endpoint_;
    ui::DetailWindow detail_;
    ui::MiniWindow mini_;
    ProcessController processes_;
    QSystemTrayIcon *tray_ = nullptr;
    int interval_ = 1000;
    bool miniMode_ = false;
    bool saveState_ = true;
};
} // namespace monitor

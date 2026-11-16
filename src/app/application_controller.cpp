#include "app/application_controller.h"
#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QScreen>
#include <QShortcut>
#include <QSystemTrayIcon>

namespace monitor {
namespace {
void ensureOnScreen(QWidget &window)
{
    for (auto *screen : QGuiApplication::screens())
        if (screen->availableGeometry().contains(window.frameGeometry())) return;
    if (auto *screen = QGuiApplication::primaryScreen()) {
        const auto area = screen->availableGeometry();
        window.move(area.topLeft() + QPoint(qMax(0, (area.width() - window.width()) / 2), qMax(0, (area.height() - window.height()) / 2)));
    }
}
} // namespace

ApplicationController::ApplicationController(QObject *parent)
    : QObject(parent), detail_(&model_), mini_(&model_)
{
    connect(&service_, &MonitorService::snapshotReady, &model_, &ui::MonitorModel::acceptSnapshot);
    connect(&detail_, &ui::DetailWindow::miniRequested, this, &ApplicationController::showMini);
    connect(&mini_, &ui::MiniWindow::detailRequested, this, &ApplicationController::showDetail);
    connect(&detail_, &ui::DetailWindow::pauseRequested, this, &ApplicationController::setPaused);
    connect(&mini_, &ui::MiniWindow::pauseRequested, this, &ApplicationController::setPaused);
    connect(&detail_, &ui::DetailWindow::intervalRequested, this, &ApplicationController::setInterval);
    connect(&mini_, &ui::MiniWindow::intervalRequested, this, &ApplicationController::setInterval);
    connect(&mini_, &ui::MiniWindow::quitRequested, qApp, &QApplication::quit);
    connect(qApp, &QApplication::aboutToQuit, this, &ApplicationController::saveSettings);
    auto *shortcut = new QShortcut(QKeySequence("Ctrl+M"), &detail_);
    connect(shortcut, &QShortcut::activated, this, &ApplicationController::showMini);
    detail_.restoreGeometry(settings_.value("detailGeometry").toByteArray());
    mini_.move(settings_.value("miniPosition", QPoint(80, 80)).toPoint());
    mini_.setAlwaysOnTop(settings_.value("alwaysOnTop", true).toBool());
    ensureOnScreen(detail_); ensureOnScreen(mini_);
    setInterval(settings_.value("interval", 1000).toInt());
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_ = new QSystemTrayIcon(QIcon(":/kylin-monitor.svg"), this);
        auto *menu = new QMenu(&detail_);
        menu->addAction(QStringLiteral("详细模式"), this, &ApplicationController::showDetail);
        menu->addAction(QStringLiteral("极简模式"), this, &ApplicationController::showMini);
        menu->addSeparator(); menu->addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
        tray_->setContextMenu(menu); tray_->setToolTip(QStringLiteral("Kylin Monitor · 系统监测")); tray_->show();
        connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) showDetail();
        });
    }
}
ApplicationController::~ApplicationController() = default;
void ApplicationController::start(bool forceMini, bool forceDetail)
{
    if (forceMini || (!forceDetail && settings_.value("miniMode", false).toBool())) showMini();
    else showDetail();
    service_.start();
}
void ApplicationController::showDetail()
{
    miniMode_ = false; detail_.show(); mini_.hide(); detail_.raise(); detail_.activateWindow();
}
void ApplicationController::showMini()
{
    miniMode_ = true; mini_.show(); detail_.hide(); mini_.raise();
}
void ApplicationController::setPaused(bool paused)
{
    service_.setPaused(paused); detail_.setPaused(paused); mini_.setPaused(paused);
}
void ApplicationController::setInterval(int milliseconds)
{
    interval_ = (milliseconds == 500 || milliseconds == 2000 || milliseconds == 5000) ? milliseconds : 1000;
    service_.setInterval(interval_); detail_.setInterval(interval_); mini_.setInterval(interval_);
}
void ApplicationController::saveSettings()
{
    if (!saveState_) return;
    settings_.setValue("detailGeometry", detail_.saveGeometry());
    settings_.setValue("miniPosition", mini_.pos()); settings_.setValue("alwaysOnTop", mini_.alwaysOnTop());
    settings_.setValue("interval", interval_); settings_.setValue("miniMode", miniMode_);
}
bool ApplicationController::savePreviews(const QString &directory)
{
    saveState_ = false;
    if (!QDir().mkpath(directory)) return false;
    bool ok = true;
    for (const auto &sample : model_.snapshot().resources) {
        detail_.selectResource(sample.id); detail_.show();
        const auto name = sample.kind == ResourceKind::Cpu ? "cpu" : sample.kind == ResourceKind::Gpu ? "gpu" : "memory";
        ok = detail_.grab().save(QDir(directory).filePath(QString::fromLatin1(name) + ".png")) && ok;
    }
    mini_.show(); ok = mini_.grab().save(QDir(directory).filePath("mini.png")) && ok;
    return ok;
}
} // namespace monitor

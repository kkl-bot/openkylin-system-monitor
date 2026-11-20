// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/theme_watcher.h"
#include "ui/widget_appearance.h"
#include <QApplication>
#include <QStandardPaths>

namespace monitor::ui {
ThemeWatcher::ThemeWatcher(QApplication &application)
    : QObject(&application), application_(application), original_(application.palette())
{
    if (QStandardPaths::findExecutable("gsettings").isEmpty()) {
        applyWidgetAppearance(application_, original_, original_.color(QPalette::Window).lightness() < 128);
        return;
    }
    updateFromSystem();
    connect(&monitor_, &QProcess::readyReadStandardOutput, this, [this]() {
        if (monitor_.readAllStandardOutput().contains("style-name")) updateFromSystem();
    });
    monitor_.start("gsettings", {"monitor", "org.ukui.style", "style-name"});
}
ThemeWatcher::~ThemeWatcher()
{
    if (monitor_.state() != QProcess::NotRunning) {
        monitor_.kill();
        monitor_.waitForFinished(200);
    }
}
void ThemeWatcher::updateFromSystem()
{
    bool dark = original_.color(QPalette::Window).lightness() < 128;
    QProcess query;
    query.start("gsettings", {"get", "org.ukui.style", "style-name"});
    if (query.waitForFinished(300)) {
        if (query.exitStatus() == QProcess::NormalExit && query.exitCode() == 0) {
            const auto name = query.readAllStandardOutput().toLower();
            if (name.contains("dark") || name.contains("black")) dark = true;
            else if (name.contains("light") || name.contains("white")) dark = false;
        }
    } else {
        query.kill();
        query.waitForFinished(100);
    }
    applyWidgetAppearance(application_, original_, dark);
}
} // namespace monitor::ui

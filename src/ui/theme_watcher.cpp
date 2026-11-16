// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/theme_watcher.h"
#include <QApplication>
#include <QStandardPaths>

namespace monitor::ui {
ThemeWatcher::ThemeWatcher(QApplication &application)
    : QObject(&application), application_(application), original_(application.palette())
{
    if (QStandardPaths::findExecutable("gsettings").isEmpty()) return;
    updateFromSystem();
    connect(&monitor_, &QProcess::readyReadStandardOutput, this, [this]() {
        if (monitor_.readAllStandardOutput().contains("style-name")) updateFromSystem();
    });
    monitor_.start("gsettings", {"monitor", "org.ukui.style", "style-name"});
}
ThemeWatcher::~ThemeWatcher()
{
    monitor_.kill();
    monitor_.waitForFinished(200);
}
void ThemeWatcher::updateFromSystem()
{
    QProcess query;
    query.start("gsettings", {"get", "org.ukui.style", "style-name"});
    if (!query.waitForFinished(300)) { query.kill(); query.waitForFinished(100); return; }
    if (query.exitCode() != 0) return;
    const auto name = query.readAllStandardOutput();
    const bool dark = name.contains("dark") || name.contains("black");
    if (dark == (application_.palette().color(QPalette::Window).lightness() < 128)) return;
    QPalette palette = original_;
    // Fallback for sessions without a UKUI palette. Native sessions supply
    // their own palette, fonts and style when they already match the theme.
    palette.setColor(QPalette::Window, QColor(dark ? "#24262c" : "#ffffff"));
    palette.setColor(QPalette::Base, QColor(dark ? "#1c1e23" : "#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor(dark ? "#2d3038" : "#f5f7fa"));
    palette.setColor(QPalette::Button, QColor(dark ? "#30333b" : "#ffffff"));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(role, QColor(dark ? "#e6e8ed" : "#243248"));
    palette.setColor(QPalette::Mid, QColor(dark ? "#515661" : "#dce4ee"));
    palette.setColor(QPalette::PlaceholderText, QColor(dark ? "#aeb6c6" : "#7e8ba0"));
    palette.setColor(QPalette::Highlight, QColor(dark ? "#416da8" : "#2979e8"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    application_.setPalette(palette);
}
} // namespace monitor::ui

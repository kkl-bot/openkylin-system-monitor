// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPalette>
#include <QProcess>
class QApplication;

namespace monitor::ui {
class ThemeWatcher final : public QObject {
public:
    explicit ThemeWatcher(QApplication &application);
    ~ThemeWatcher() override;
private:
    void updateFromSystem();
    QApplication &application_;
    QPalette original_;
    QProcess monitor_;
};
} // namespace monitor::ui

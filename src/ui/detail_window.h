// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ui/monitor_model.h"
#include <QWidget>

class QStackedWidget;
namespace monitor::ui {
class PerformanceWorkspace;
class TaskNavigation;
class ProcessPage;
class NetworkPage;

class DetailWindow final : public QWidget {
    Q_OBJECT
public:
    explicit DetailWindow(MonitorModel *model, QWidget *parent = nullptr);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void selectResource(const QString &id);
    QString selectedResource() const;
    void setNavigationCollapsed(bool collapsed);
    bool navigationCollapsed() const;
    void showPerformance();
    void showProcesses();
    bool showingProcesses() const;
    ProcessPage *processPage() const;
    void showNetwork();
    bool showingNetwork() const;
    NetworkPage *networkPage() const;
signals:
    void miniRequested();
    void pauseRequested(bool paused);
    void intervalRequested(int milliseconds);
    void processPageActiveChanged(bool active);
    void networkPageActiveChanged(bool active);
protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    void updateActivePage();
    TaskNavigation *navigation_;
    QStackedWidget *pages_;
    PerformanceWorkspace *performance_;
    ProcessPage *processPage_;
    NetworkPage *networkPage_;
};
} // namespace monitor::ui

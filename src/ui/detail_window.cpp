// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/detail_window.h"
#include "ui/network_page.h"
#include "ui/performance_page.h"
#include "ui/performance_workspace.h"
#include "ui/process_page.h"
#include "ui/task_navigation.h"
#include "ui/typography.h"
#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QStackedWidget>

namespace monitor::ui {
DetailWindow::DetailWindow(MonitorModel *model, QWidget *parent) : QWidget(parent)
{
    setObjectName("detailWindow");
    const QString name = tr("Kylin Monitor 任务管理器");
    const QString version = QApplication::applicationVersion();
    setWindowTitle(version.isEmpty() ? name : tr("%1 · %2").arg(name, version));
    resize(1180, 780);
    setMinimumSize(760, 500);
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    navigation_ = new TaskNavigation;
    root->addWidget(navigation_);
    pages_ = new QStackedWidget;
    root->addWidget(pages_, 1);
    performance_ = new PerformanceWorkspace(model);
    pages_->addWidget(performance_);
    processPage_ = new ProcessPage;
    pages_->addWidget(processPage_);
    networkPage_ = new NetworkPage;
    pages_->addWidget(networkPage_);
    connect(navigation_, &TaskNavigation::sectionRequested, this, [this](TaskSection section) {
        switch (section) {
        case TaskSection::Processes: showProcesses(); break;
        case TaskSection::Performance: showPerformance(); break;
        case TaskSection::Network: showNetwork(); break;
        }
    });
    connect(pages_, &QStackedWidget::currentChanged, this, &DetailWindow::updateActivePage);
    connect(performance_->page(), &PerformancePage::miniRequested, this, &DetailWindow::miniRequested);
    connect(performance_->page(), &PerformancePage::pauseRequested, this, &DetailWindow::pauseRequested);
    connect(performance_->page(), &PerformancePage::intervalRequested, this, &DetailWindow::intervalRequested);
    connect(processPage_, &ProcessPage::pauseRequested, this, &DetailWindow::pauseRequested);
    connect(processPage_, &ProcessPage::intervalRequested, this, &DetailWindow::intervalRequested);
    connect(networkPage_, &NetworkPage::pauseRequested, this, &DetailWindow::pauseRequested);
    connect(networkPage_, &NetworkPage::intervalRequested, this, &DetailWindow::intervalRequested);
    enableWindowTypography(this);
    updateActivePage();
}
void DetailWindow::setPaused(bool paused)
{
    performance_->page()->setPaused(paused);
    processPage_->setPaused(paused);
    networkPage_->setPaused(paused);
}
void DetailWindow::setInterval(int milliseconds)
{
    performance_->page()->setInterval(milliseconds);
    processPage_->setInterval(milliseconds);
    networkPage_->setInterval(milliseconds);
}
void DetailWindow::selectResource(const QString &id)
{
    performance_->setResource(id);
    showPerformance();
}
QString DetailWindow::selectedResource() const { return performance_->selectedResource(); }
void DetailWindow::setNavigationCollapsed(bool collapsed) { navigation_->setCollapsed(collapsed); }
bool DetailWindow::navigationCollapsed() const { return navigation_->isCollapsed(); }
void DetailWindow::showPerformance() { pages_->setCurrentWidget(performance_); }
void DetailWindow::showProcesses() { pages_->setCurrentWidget(processPage_); }
bool DetailWindow::showingProcesses() const { return pages_->currentWidget() == processPage_; }
ProcessPage *DetailWindow::processPage() const { return processPage_; }
void DetailWindow::showNetwork() { pages_->setCurrentWidget(networkPage_); }
bool DetailWindow::showingNetwork() const { return pages_->currentWidget() == networkPage_; }
NetworkPage *DetailWindow::networkPage() const { return networkPage_; }
void DetailWindow::updateActivePage()
{
    navigation_->setSection(showingProcesses() ? TaskSection::Processes :
                            showingNetwork() ? TaskSection::Network : TaskSection::Performance);
    const bool active = isVisible() && !isMinimized();
    emit processPageActiveChanged(active && showingProcesses());
    emit networkPageActiveChanged(active && showingNetwork());
}
void DetailWindow::closeEvent(QCloseEvent *event) { event->accept(); qApp->quit(); }
void DetailWindow::showEvent(QShowEvent *event) { QWidget::showEvent(event); updateActivePage(); }
void DetailWindow::hideEvent(QHideEvent *event) { QWidget::hideEvent(event); updateActivePage(); }
void DetailWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) updateActivePage();
}
} // namespace monitor::ui

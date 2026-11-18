// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/process_controller.h"
#include "backend/process_control.h"
#include "ui/process_page.h"

namespace monitor {
ProcessController::ProcessController(ui::ProcessPage *page, QObject *parent)
    : QObject(parent), page_(page)
{
    connect(&service_, &ProcessService::snapshotReady, page_, &ui::ProcessPage::acceptSnapshot);
    connect(page_, &ui::ProcessPage::refreshRequested, &service_, &ProcessService::requestRefresh);
    connect(page_, &ui::ProcessPage::terminateRequested, this, &ProcessController::terminate);
    service_.start();
}
void ProcessController::setActive(bool active) { service_.setActive(active); }
void ProcessController::setPaused(bool paused) { service_.setPaused(paused); }
void ProcessController::setInterval(int milliseconds) { service_.setInterval(milliseconds); }
void ProcessController::terminate(const ProcessIdentity &identity, bool force)
{
    const QString error = ProcessControl::sendSignal(identity, force);
    page_->showActionResult(error.isEmpty()
        ? tr("已向 PID %1 发送%2请求，等待进程状态更新。")
              .arg(identity.pid).arg(force ? tr("强制结束") : tr("结束"))
        : error);
    service_.requestRefresh();
}
} // namespace monitor

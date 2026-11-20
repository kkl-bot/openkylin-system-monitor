// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/network_controller.h"
#include "ui/network_page.h"

namespace monitor {
NetworkController::NetworkController(ui::NetworkPage *page, QObject *parent) : QObject(parent)
{
    connect(&service_, &NetworkService::snapshotReady, page, &ui::NetworkPage::acceptSnapshot);
    connect(page, &ui::NetworkPage::refreshRequested, &service_, &NetworkService::requestRefresh);
    service_.start();
}
void NetworkController::setActive(bool active) { service_.setActive(active); }
void NetworkController::setPaused(bool paused) { service_.setPaused(paused); }
void NetworkController::setInterval(int milliseconds) { service_.setInterval(milliseconds); }
} // namespace monitor

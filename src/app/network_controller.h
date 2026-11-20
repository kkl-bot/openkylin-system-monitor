// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "backend/network_service.h"
#include <QObject>

namespace monitor {
namespace ui { class NetworkPage; }
class NetworkController final : public QObject {
    Q_OBJECT
public:
    explicit NetworkController(ui::NetworkPage *page, QObject *parent = nullptr);
    void setActive(bool active);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
private:
    NetworkService service_;
};
} // namespace monitor

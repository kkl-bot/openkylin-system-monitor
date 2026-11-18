// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "backend/process_service.h"
#include <QObject>

namespace monitor::ui { class ProcessPage; }
namespace monitor {
class ProcessController final : public QObject {
    Q_OBJECT
public:
    explicit ProcessController(ui::ProcessPage *page, QObject *parent = nullptr);
    void setActive(bool active);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
private:
    void terminate(const ProcessIdentity &identity, bool force);
    ui::ProcessPage *page_;
    ProcessService service_;
};
} // namespace monitor

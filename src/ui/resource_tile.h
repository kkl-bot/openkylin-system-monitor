#pragma once
#include "ui/monitor_model.h"
#include <QPushButton>

namespace monitor::ui {
class ResourceTile final : public QPushButton {
    Q_OBJECT
public:
    ResourceTile(MonitorModel *model, const QString &id, QWidget *parent = nullptr);
    QString resourceId() const;
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    MonitorModel *model_;
    QString id_;
};
} // namespace monitor::ui

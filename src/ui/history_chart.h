#pragma once
#include "ui/monitor_model.h"
#include <QWidget>

namespace monitor::ui {
class HistoryChart final : public QWidget {
    Q_OBJECT
public:
    explicit HistoryChart(MonitorModel *model, bool temperature, QWidget *parent = nullptr);
    void setResource(const QString &id);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    MonitorModel *model_;
    QString resourceId_;
    bool temperature_;
};
} // namespace monitor::ui

#pragma once
#include "ui/monitor_model.h"
#include <QPushButton>

namespace monitor::ui {
class ResourceTile final : public QPushButton {
    Q_OBJECT
public:
    ResourceTile(MonitorModel *model, const QString &id, QWidget *parent = nullptr);
    QString resourceId() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int heightForWidth(int width) const override;
protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    void refresh();
    MonitorModel *model_;
    QString id_;
};
} // namespace monitor::ui

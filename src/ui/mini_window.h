#pragma once
#include "ui/monitor_model.h"
#include <QWidget>

namespace monitor::ui {
class MiniWindow final : public QWidget {
    Q_OBJECT
public:
    explicit MiniWindow(MonitorModel *model, QWidget *parent = nullptr);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void setAlwaysOnTop(bool enabled);
    bool alwaysOnTop() const;
signals:
    void detailRequested();
    void pauseRequested(bool paused);
    void intervalRequested(int milliseconds);
    void quitRequested();
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    QVector<const ResourceSample *> displayedResources() const;
    void refreshTooltip();
    MonitorModel *model_;
    QString gpuId_;
    QPoint dragOffset_;
    bool dragging_ = false;
    bool paused_ = false;
    bool alwaysOnTop_ = true;
    int interval_ = 1000;
};
} // namespace monitor::ui

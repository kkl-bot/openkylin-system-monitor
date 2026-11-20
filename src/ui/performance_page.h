// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ui/monitor_model.h"
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
namespace monitor::ui {
class HistoryChart;
class MemoryCompositionBar;
class PerformanceMetric;
class PerformancePage final : public QWidget {
    Q_OBJECT
public:
    explicit PerformancePage(MonitorModel *model, QWidget *parent = nullptr);
    void setResource(const QString &id);
    void setInterval(int milliseconds);
    void setPaused(bool paused);
signals:
    void miniRequested();
    void intervalRequested(int milliseconds);
    void pauseRequested(bool paused);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void refresh();
    void scheduleLayoutUpdate();
    void updateLayout();
    MonitorModel *model_;
    QString resourceId_ = QStringLiteral("cpu");
    bool layoutUpdatePending_ = false;
    QScrollArea *scroll_;
    QVBoxLayout *contentLayout_;
    QLabel *title_;
    QLabel *capacity_;
    QLabel *deviceName_;
    QLabel *usageHeading_;
    QLabel *temperatureHeading_;
    QLabel *details_;
    QLabel *notice_;
    QLabel *temperatureNotice_;
    QLabel *environment_;
    QLabel *timestamp_;
    QLabel *status_;
    QWidget *composition_;
    MemoryCompositionBar *compositionBar_;
    QLabel *usedLabel_;
    QLabel *availableLabel_;
    QLabel *compositionNotice_;
    PerformanceMetric *usageMetric_;
    PerformanceMetric *capacityMetric_;
    PerformanceMetric *extraMetric_;
    PerformanceMetric *temperatureMetric_;
    QPushButton *pause_;
    QComboBox *interval_;
    HistoryChart *usageChart_;
    HistoryChart *temperatureChart_;
};
} // namespace monitor::ui

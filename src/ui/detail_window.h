#pragma once
#include "ui/monitor_model.h"
#include <QWidget>

class QLabel;
class QPushButton;
class QComboBox;
class QVBoxLayout;
namespace monitor::ui {
class HistoryChart;
class ResourceTile;

class DetailWindow final : public QWidget {
    Q_OBJECT
public:
    explicit DetailWindow(MonitorModel *model, QWidget *parent = nullptr);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void selectResource(const QString &id);
signals:
    void miniRequested();
    void pauseRequested(bool paused);
    void intervalRequested(int milliseconds);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void refresh();
    void rebuildResources();
    MonitorModel *model_;
    QString selectedId_ = "cpu";
    QStringList resourceIds_;
    QVector<ResourceTile *> tiles_;
    QVBoxLayout *resourcesLayout_;
    QLabel *title_;
    QLabel *subtitle_;
    QLabel *usage_;
    QLabel *temperature_;
    QLabel *capacity_;
    QLabel *capacityLabel_;
    QLabel *notice_;
    QLabel *details_;
    QLabel *environment_;
    QLabel *status_;
    QLabel *timestamp_;
    QPushButton *pause_;
    QComboBox *interval_;
    HistoryChart *usageChart_;
    HistoryChart *temperatureChart_;
};
} // namespace monitor::ui

#pragma once
#include "ui/monitor_model.h"
#include <QWidget>

class QLabel;
class QPushButton;
class QComboBox;
class QVBoxLayout;
class QStackedWidget;
class QShowEvent;
class QHideEvent;
class QResizeEvent;
class QFrame;
class QGridLayout;
class QScrollArea;
namespace monitor::ui {
class HistoryChart;
class ResourceTile;
class ProcessPage;

class DetailWindow final : public QWidget {
    Q_OBJECT
public:
    explicit DetailWindow(MonitorModel *model, QWidget *parent = nullptr);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void selectResource(const QString &id);
    void showProcesses();
    bool showingProcesses() const;
    ProcessPage *processPage() const;
signals:
    void miniRequested();
    void pauseRequested(bool paused);
    void intervalRequested(int milliseconds);
    void processPageActiveChanged(bool active);
protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void updateLayout();
    void updateActivePage();
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
    QStackedWidget *pages_;
    ProcessPage *processPage_;
    QPushButton *processNavigation_;
    QFrame *sidebar_ = nullptr;
    QScrollArea *performanceScroll_ = nullptr;
    QVBoxLayout *performanceLayout_ = nullptr;
    QGridLayout *cardsLayout_ = nullptr;
    QVector<QFrame *> cards_;
    int cardColumns_ = 0;
};
} // namespace monitor::ui

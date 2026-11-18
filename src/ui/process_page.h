#pragma once

#include "domain/process_types.h"
#include <QByteArray>
#include <QWidget>
#include <optional>

class QComboBox;
class QLabel;
class QPushButton;

namespace monitor::ui {
class ProcessFilterModel;
class ProcessTableModel;
class ProcessTableView;

class ProcessPage final : public QWidget {
    Q_OBJECT
public:
    explicit ProcessPage(QWidget *parent = nullptr);
    void acceptSnapshot(const ProcessSnapshot &snapshot);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    void showActionResult(const QString &message);
    QByteArray headerState() const;
    void restoreHeaderState(const QByteArray &state);
signals:
    void pauseRequested(bool paused);
    void intervalRequested(int milliseconds);
    void refreshRequested();
    void terminateRequested(const monitor::ProcessIdentity &identity, bool force);
private:
    std::optional<ProcessInfo> selectedProcess() const;
    bool canTerminate(const ProcessInfo &process) const;
    void refreshSelection();
    void refreshStatus();
    void openDetails();
    void confirmTermination(bool force);
    ProcessTableModel *model_;
    ProcessFilterModel *filter_;
    ProcessTableView *table_;
    QLabel *summary_;
    QLabel *status_;
    QLabel *actionResult_;
    QPushButton *details_;
    QPushButton *terminate_;
    QPushButton *force_;
    QPushButton *pause_;
    QComboBox *interval_;
    QDateTime timestamp_;
    QString snapshotError_;
    bool paused_ = false;
};
} // namespace monitor::ui

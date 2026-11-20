// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "domain/network_types.h"
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
namespace monitor::ui {
class NetworkHistoryChart;
class NetworkPage final : public QWidget {
    Q_OBJECT
public:
    explicit NetworkPage(QWidget *parent = nullptr);
    void acceptSnapshot(const NetworkSnapshot &snapshot);
    void setPaused(bool paused);
    void setInterval(int milliseconds);
    QString selectedInterface() const;
    void setSelectedInterface(const QString &name);
signals:
    void pauseRequested(bool paused);
    void intervalRequested(int milliseconds);
    void refreshRequested();
    void selectedInterfaceChanged(const QString &name);
private:
    const NetworkInterfaceInfo *currentInterface() const;
    QString defaultInterface() const;
    void rebuildInterfaces();
    void refreshView();
    void openDetails();
    NetworkSnapshot snapshot_;
    QString selectedInterface_;
    bool paused_ = false;
    QComboBox *interfaces_;
    QComboBox *interval_;
    QLabel *interfaceName_;
    QLabel *download_;
    QLabel *upload_;
    QLabel *link_;
    QLabel *systemProxy_;
    QLabel *environmentProxy_;
    QLabel *status_;
    QLabel *notice_;
    QPushButton *details_;
    QPushButton *pause_;
    NetworkHistoryChart *chart_;
};
} // namespace monitor::ui

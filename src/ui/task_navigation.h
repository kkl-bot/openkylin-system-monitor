// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFrame>

class QButtonGroup;
class QPushButton;
namespace monitor::ui {
enum class TaskSection { Processes, Performance, Network };

class TaskNavigation final : public QFrame {
    Q_OBJECT
public:
    explicit TaskNavigation(QWidget *parent = nullptr);
    bool isCollapsed() const;
    void setCollapsed(bool collapsed);
    void setSection(TaskSection section);
signals:
    void sectionRequested(TaskSection section);
protected:
    void changeEvent(QEvent *event) override;
private:
    void updateDimensions();
    QButtonGroup *group_ = nullptr;
    QPushButton *toggle_ = nullptr;
    bool collapsed_ = false;
};
} // namespace monitor::ui

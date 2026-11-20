// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QWidget>

class QScrollArea;
class QComboBox;
class QLabel;
class QVBoxLayout;
namespace monitor::ui {
class MonitorModel;
class PerformancePage;
class ResourceTile;

class PerformanceWorkspace final : public QWidget {
    Q_OBJECT
public:
    explicit PerformanceWorkspace(MonitorModel *model, QWidget *parent = nullptr);
    PerformancePage *page() const;
    QString selectedResource() const;
    void setResource(const QString &id);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    void refresh();
    void updateSelectorLayout();
    MonitorModel *model_;
    QScrollArea *selector_ = nullptr;
    QComboBox *picker_ = nullptr;
    QLabel *environment_ = nullptr;
    QVBoxLayout *resources_ = nullptr;
    PerformancePage *page_ = nullptr;
    QVector<ResourceTile *> tiles_;
    QStringList ids_;
    QString selectedId_ = QStringLiteral("cpu");
};
} // namespace monitor::ui

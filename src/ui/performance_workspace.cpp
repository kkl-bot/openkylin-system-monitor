// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/performance_workspace.h"
#include "ui/performance_page.h"
#include "ui/resource_tile.h"
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace monitor::ui {
PerformanceWorkspace::PerformanceWorkspace(MonitorModel *model, QWidget *parent)
    : QWidget(parent), model_(model)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto *header = new QHBoxLayout;
    header->setContentsMargins(18, 9, 18, 9);
    header->setSpacing(12);
    auto *title = new QLabel(tr("性能"));
    title->setProperty("textRole", "section");
    header->addWidget(title);
    picker_ = new QComboBox;
    picker_->setObjectName("performanceResourcePicker");
    picker_->setAccessibleName(tr("选择性能资源"));
    picker_->setMinimumContentsLength(5);
    picker_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    header->addWidget(picker_);
    header->addStretch();
    environment_ = new QLabel;
    environment_->setObjectName(QStringLiteral("performanceEnvironmentName"));
    header->addWidget(environment_);
    root->addLayout(header);
    auto *line = new QFrame;
    line->setObjectName(QStringLiteral("performanceDivider"));
    line->setFixedHeight(1);
    line->setFrameShape(QFrame::HLine);
    root->addWidget(line);
    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    root->addLayout(body, 1);
    selector_ = new QScrollArea;
    selector_->setObjectName("resourceSelector");
    selector_->setWidgetResizable(true);
    selector_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *list = new QWidget;
    list->setObjectName(QStringLiteral("resourceList"));
    resources_ = new QVBoxLayout(list);
    resources_->setContentsMargins(5, 8, 5, 8);
    resources_->setSpacing(3);
    resources_->setSizeConstraint(QLayout::SetMinAndMaxSize);
    resources_->addStretch();
    selector_->setWidget(list);
    body->addWidget(selector_);
    page_ = new PerformancePage(model);
    body->addWidget(page_, 1);
    connect(picker_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0) setResource(picker_->itemData(index).toString());
    });
    connect(model_, &MonitorModel::changed, this, &PerformanceWorkspace::refresh);
    refresh();
    updateSelectorLayout();
}
PerformancePage *PerformanceWorkspace::page() const { return page_; }
QString PerformanceWorkspace::selectedResource() const { return selectedId_; }
void PerformanceWorkspace::setResource(const QString &id)
{
    selectedId_ = ids_.isEmpty() || ids_.contains(id) ? id : ids_.first();
    const int index = ids_.indexOf(selectedId_);
    const QSignalBlocker blocker(picker_);
    picker_->setCurrentIndex(index);
    if (index >= 0) {
        tiles_.at(index)->setChecked(true);
        selector_->ensureWidgetVisible(tiles_.at(index));
    }
    page_->setResource(selectedId_);
}
void PerformanceWorkspace::refresh()
{
    const auto &snapshot = model_->snapshot();
    environment_->setText(snapshot.isWsl ? tr("openKylin · WSL 2") : tr("openKylin · Linux"));
    environment_->setToolTip(snapshot.isWsl ? tr("CPU 和内存反映 WSL 环境") : environment_->text());
    QStringList ids;
    for (auto kind : {ResourceKind::Cpu, ResourceKind::Memory, ResourceKind::Gpu})
        for (const auto &sample : snapshot.resources)
            if (sample.kind == kind) ids.append(sample.id);
    const QSignalBlocker blocker(picker_);
    const bool topologyChanged = ids != ids_;
    if (topologyChanged) {
        qDeleteAll(tiles_);
        tiles_.clear();
        picker_->clear();
        ids_ = ids;
        for (const auto &id : ids) {
            auto *tile = new ResourceTile(model_, id);
            resources_->insertWidget(resources_->count() - 1, tile);
            tile->setFont(window()->font());
            tiles_.append(tile);
            picker_->addItem(QString(), id);
            connect(tile, &QPushButton::clicked, this, [this, id] { setResource(id); });
        }
    }
    for (int index = 0; index < ids.size(); ++index) {
        const auto *sample = model_->resource(ids.at(index));
        QString label = model_->resourceTitle(*sample);
        if (sample->kind == ResourceKind::Gpu && !sample->name.isEmpty())
            label += QStringLiteral(" · ") + sample->name;
        if (picker_->itemText(index) != label) picker_->setItemText(index, label);
    }
    if (topologyChanged) setResource(selectedId_);
}
void PerformanceWorkspace::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateSelectorLayout();
}
void PerformanceWorkspace::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) updateSelectorLayout();
}
void PerformanceWorkspace::updateSelectorLayout()
{
    if (!page_) return;
    const int column = qBound(215, fontMetrics().horizontalAdvance(QStringLiteral("0.0 / 00.0 GiB (100%)")) + 64, 300);
    const int chartArea = qMax(505, fontMetrics().height() * 17);
    const bool compact = width() < column + chartArea;
    selector_->setFixedWidth(column);
    selector_->setVisible(!compact);
    picker_->setVisible(compact);
}
} // namespace monitor::ui

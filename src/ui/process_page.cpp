#include "ui/process_page.h"
#include "ui/process_details_dialog.h"
#include "ui/process_filter_model.h"
#include "ui/process_table_model.h"
#include "ui/process_table_view.h"
#include "ui/flow_layout.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableView>
#include <QVBoxLayout>

namespace monitor::ui {
namespace {
QLabel *plainLabel(const QString &text, const QString &objectName, QWidget *parent, bool flexibleWidth = true)
{
    auto *label = new QLabel(text, parent); label->setTextFormat(Qt::PlainText);
    label->setObjectName(objectName); label->setWordWrap(true);
    if (flexibleWidth) label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
}
} // namespace
ProcessPage::ProcessPage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("processPage"));
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this); scroll->setObjectName(QStringLiteral("processContentScroll"));
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget; scroll->setWidget(content); outer->addWidget(scroll);
    auto *root = new QVBoxLayout(content); root->setContentsMargins(24, 22, 24, 18); root->setSpacing(10);
    root->setSizeConstraint(QLayout::SetMinAndMaxSize);
    root->addWidget(plainLabel(QStringLiteral("进程"), QStringLiteral("pageTitle"), this));
    root->addWidget(plainLabel(QStringLiteral("CPU 按整机总容量计量 · 内存为驻留内存 · 点击列标题排序"), QStringLiteral("subtitle"), this));
    auto *searchRow = new QHBoxLayout;
    auto *search = new QLineEdit(this); search->setObjectName(QStringLiteral("processSearch"));
    search->setPlaceholderText(QStringLiteral("搜索名称、PID 或用户")); search->setClearButtonEnabled(true);
    search->setAccessibleName(QStringLiteral("搜索进程")); searchRow->addWidget(search, 1);
    auto *own = new QCheckBox(QStringLiteral("仅我的进程"), this); own->setObjectName(QStringLiteral("processOwnOnly"));
    searchRow->addWidget(own); root->addLayout(searchRow);
    auto *actions = new FlowLayout;
    auto *refresh = new QPushButton(QStringLiteral("刷新"), this); refresh->setObjectName(QStringLiteral("processRefresh"));
    details_ = new QPushButton(QStringLiteral("详情"), this); details_->setObjectName(QStringLiteral("processDetails"));
    terminate_ = new QPushButton(QStringLiteral("结束进程"), this); terminate_->setObjectName(QStringLiteral("processTerminate"));
    force_ = new QPushButton(QStringLiteral("强制结束…"), this); force_->setObjectName(QStringLiteral("processForceTerminate"));
    actions->addWidget(refresh); actions->addWidget(details_);
    actions->addWidget(terminate_); actions->addWidget(force_); root->addLayout(actions);

    model_ = new ProcessTableModel(this); model_->setObjectName(QStringLiteral("processTableModel"));
    filter_ = new ProcessFilterModel(this); filter_->setObjectName(QStringLiteral("processFilterModel")); filter_->setSourceModel(model_);
    table_ = new ProcessTableView(this); table_->setObjectName(QStringLiteral("processTable")); table_->setModel(filter_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setAlternatingRowColors(true);
    table_->setShowGrid(false); table_->setWordWrap(false);
    table_->setSortingEnabled(true); table_->sortByColumn(ProcessTableModel::Cpu, Qt::DescendingOrder);
    root->addWidget(table_, 1);
    summary_ = plainLabel({}, QStringLiteral("processSummary"), this); root->addWidget(summary_);
    actionResult_ = plainLabel({}, QStringLiteral("processActionResult"), this); actionResult_->hide(); root->addWidget(actionResult_);
    status_ = plainLabel(QStringLiteral("等待首次进程采样"), QStringLiteral("processStatus"), this); root->addWidget(status_);
    root->addWidget(plainLabel(QStringLiteral("仅可结束当前用户的普通进程"), QStringLiteral("processControlHint"), this));
    auto *bottom = new FlowLayout;
    bottom->addWidget(plainLabel(QStringLiteral("刷新间隔"), QStringLiteral("muted"), this, false));
    interval_ = new QComboBox(this); interval_->setObjectName(QStringLiteral("processInterval"));
    interval_->addItem(QStringLiteral("0.5 秒"), 500); interval_->addItem(QStringLiteral("1 秒"), 1000);
    interval_->addItem(QStringLiteral("2 秒"), 2000); interval_->addItem(QStringLiteral("5 秒"), 5000); interval_->setCurrentIndex(1);
    bottom->addWidget(interval_);
    pause_ = new QPushButton(QStringLiteral("暂停"), this); pause_->setObjectName(QStringLiteral("processPause")); pause_->setCheckable(true);
    bottom->addWidget(pause_); root->addLayout(bottom);

    connect(search, &QLineEdit::textChanged, filter_, &ProcessFilterModel::setSearchText);
    connect(own, &QCheckBox::toggled, filter_, &ProcessFilterModel::setOwnProcessesOnly);
    connect(search, &QLineEdit::textChanged, this, [this]() { refreshSelection(); refreshStatus(); });
    connect(own, &QCheckBox::toggled, this, [this]() { refreshSelection(); refreshStatus(); });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ProcessPage::refreshSelection);
    connect(table_, &QTableView::doubleClicked, this, [this]() { openDetails(); });
    connect(details_, &QPushButton::clicked, this, &ProcessPage::openDetails);
    connect(terminate_, &QPushButton::clicked, this, [this]() { confirmTermination(false); });
    connect(force_, &QPushButton::clicked, this, [this]() { confirmTermination(true); });
    connect(refresh, &QPushButton::clicked, this, &ProcessPage::refreshRequested);
    connect(pause_, &QPushButton::toggled, this, [this](bool paused) { setPaused(paused); emit pauseRequested(paused); });
    connect(interval_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { emit intervalRequested(interval_->currentData().toInt()); });
    refreshSelection(); refreshStatus();
}
void ProcessPage::acceptSnapshot(const ProcessSnapshot &snapshot)
{
    const auto selected = selectedProcess();
    const QSignalBlocker blocker(table_->selectionModel());
    timestamp_ = snapshot.timestamp; snapshotError_ = snapshot.error;
    model_->acceptSnapshot(snapshot);
    if (selected) {
        const auto index = filter_->mapFromSource(model_->indexForIdentity(selected->identity));
        if (index.isValid()) table_->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        else { table_->clearSelection(); table_->setCurrentIndex({}); }
    } else { table_->clearSelection(); table_->setCurrentIndex({}); }
    refreshSelection(); refreshStatus();
}
void ProcessPage::setPaused(bool paused)
{
    paused_ = paused; const QSignalBlocker blocker(pause_); pause_->setChecked(paused);
    pause_->setText(paused ? QStringLiteral("继续") : QStringLiteral("暂停")); refreshStatus();
}
void ProcessPage::setInterval(int milliseconds)
{
    const int index = interval_->findData(milliseconds); if (index < 0) return;
    const QSignalBlocker blocker(interval_); interval_->setCurrentIndex(index);
}
void ProcessPage::showActionResult(const QString &message)
{
    actionResult_->setText(message); actionResult_->setVisible(!message.isEmpty());
}
QByteArray ProcessPage::headerState() const { return table_->savedHeaderState(); }
void ProcessPage::restoreHeaderState(const QByteArray &state)
{
    if (!state.isEmpty()) table_->restoreSavedHeaderState(state);
}
std::optional<ProcessInfo> ProcessPage::selectedProcess() const
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.size() != 1) return {};
    const auto *process = model_->processAt(filter_->mapToSource(rows.first()).row());
    return process ? std::optional<ProcessInfo>(*process) : std::nullopt;
}
bool ProcessPage::canTerminate(const ProcessInfo &process) const
{
    return snapshotError_.isEmpty() && process.canTerminate && process.identity.uid != 0
        && process.identity.uid == filter_->currentUserId() && process.identity.pid > 1
        && process.identity.pid != QCoreApplication::applicationPid();
}
void ProcessPage::refreshSelection()
{
    const auto process = selectedProcess(); const bool allowed = process && canTerminate(*process);
    details_->setEnabled(process.has_value()); terminate_->setEnabled(allowed); force_->setEnabled(allowed);
    const auto reason = allowed ? QStringLiteral("请求进程正常退出（SIGTERM）")
        : QStringLiteral("请选择当前用户的普通进程；系统进程、其他用户及本程序不可结束。");
    terminate_->setToolTip(reason);
    force_->setToolTip(allowed ? QStringLiteral("立即强制终止（SIGKILL）；未保存的数据可能丢失。") : reason);
}
void ProcessPage::refreshStatus()
{
    summary_->setText(QStringLiteral("显示 %1 / %2 个进程%3").arg(filter_->rowCount()).arg(model_->rowCount())
        .arg(filter_->rowCount() == 0 && model_->rowCount() > 0 ? QStringLiteral(" · 没有匹配项") : QString()));
    if (!snapshotError_.isEmpty()) status_->setText(QStringLiteral("读取失败：") + snapshotError_);
    else if (!timestamp_.isValid()) status_->setText(QStringLiteral("等待首次进程采样"));
    else status_->setText((paused_ ? QStringLiteral("已暂停 · ") : QStringLiteral("实时监测 · "))
        + QStringLiteral("更新于 ") + timestamp_.toLocalTime().toString(QStringLiteral("HH:mm:ss")));
}
void ProcessPage::openDetails()
{
    const auto process = selectedProcess(); if (!process) return;
    ProcessDetailsDialog dialog(*process, timestamp_, this); dialog.exec();
}
void ProcessPage::confirmTermination(bool force)
{
    // Capture the entire identity before the modal loop; incoming samples can reorder the table.
    const auto target = selectedProcess(); if (!target || !canTerminate(*target)) return;
    QMessageBox confirmation(force ? QMessageBox::Warning : QMessageBox::Question,
        force ? QStringLiteral("强制结束进程") : QStringLiteral("结束进程"), {}, QMessageBox::Cancel, this);
    confirmation.setObjectName(QStringLiteral("processTerminationConfirmation"));
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.setText(QStringLiteral("%1\nPID：%2\n用户：%3（UID %4）")
        .arg(target->name, QString::number(target->identity.pid), target->user, QString::number(target->identity.uid)));
    confirmation.setInformativeText(force
        ? QStringLiteral("强制结束会立即停止此进程，不会让它保存数据或清理资源。未保存的数据可能丢失。仅在正常结束无效时使用。")
        : QStringLiteral("将请求此进程退出。请先保存正在处理的文件；程序可以自行处理或拒绝退出请求。"));
    auto *confirm = confirmation.addButton(force ? QStringLiteral("强制结束") : QStringLiteral("结束进程"), QMessageBox::AcceptRole);
    confirm->setObjectName(QStringLiteral("processConfirmTermination"));
    confirmation.setDefaultButton(QMessageBox::Cancel); confirmation.setEscapeButton(QMessageBox::Cancel);
    confirmation.exec();
    if (confirmation.clickedButton() != confirm) return;
    const auto index = model_->indexForIdentity(target->identity);
    const auto *current = index.isValid() ? model_->processAt(index.row()) : nullptr;
    if (!current || !canTerminate(*current)) {
        showActionResult(QStringLiteral("目标进程已退出、身份已变化或当前不可操作；未发送结束请求。")); return;
    }
    showActionResult(QStringLiteral("正在请求结束 PID %1…").arg(target->identity.pid));
    emit terminateRequested(target->identity, force);
}
} // namespace monitor::ui

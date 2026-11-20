#include "ui/process_page.h"
#include "ui/process_details_dialog.h"
#include "ui/process_filter_model.h"
#include "ui/process_heat_delegate.h"
#include "ui/process_table_model.h"
#include "ui/process_table_view.h"
#include "ui/flow_layout.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace monitor::ui {
namespace {
QLabel *plainLabel(const QString &text, const QString &objectName, QWidget *parent, bool flexibleWidth = true)
{
    auto *label = new QLabel(text, parent); label->setTextFormat(Qt::PlainText);
    label->setObjectName(objectName); label->setWordWrap(true);
    if (objectName == QStringLiteral("pageTitle")) label->setProperty("textRole", QStringLiteral("title"));
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
    content_ = new QWidget; content_->setObjectName(QStringLiteral("processContent")); scroll->setWidget(content_); outer->addWidget(scroll);
    auto *root = new QVBoxLayout(content_); root->setContentsMargins(16, 14, 16, 12); root->setSpacing(8);
    root->setSizeConstraint(QLayout::SetMinAndMaxSize);
    searchLayout_ = new QHBoxLayout;
    title_ = plainLabel(QStringLiteral("进程"), QStringLiteral("pageTitle"), this, false);
    searchLayout_->addWidget(title_, 0, Qt::AlignLeft);
    search_ = new QLineEdit(this); search_->setObjectName(QStringLiteral("processSearch"));
    search_->setPlaceholderText(QStringLiteral("搜索名称、PID 或用户")); search_->setClearButtonEnabled(true);
    const auto searchIcon = QIcon::fromTheme(QStringLiteral("edit-find"));
    if (!searchIcon.isNull()) {
        auto *iconAction = search_->addAction(searchIcon, QLineEdit::LeadingPosition);
        iconAction->setObjectName(QStringLiteral("processSearchIcon"));
        iconAction->setText(QStringLiteral("搜索"));
        iconAction->setEnabled(false); // Decoration: filtering already follows text changes.
    }
    search_->setToolTip(search_->placeholderText());
    search_->setAccessibleName(QStringLiteral("搜索进程")); searchLayout_->addWidget(search_, 1);
    ownOnly_ = new QCheckBox(QStringLiteral("仅我的进程"), this); ownOnly_->setObjectName(QStringLiteral("processOwnOnly"));
    ownOnly_->setToolTip(QStringLiteral("按当前用户筛选进程，输入框可继续按名称、PID 或用户搜索。"));
    root->addLayout(searchLayout_);
    auto *commands = new QWidget(content_); commands->setObjectName(QStringLiteral("processCommandBar"));
    auto *actions = new FlowLayout(commands, 8);
    auto *refresh = new QPushButton(QStringLiteral("刷新"), this); refresh->setObjectName(QStringLiteral("processRefresh"));
    details_ = new QPushButton(QStringLiteral("详情"), this); details_->setObjectName(QStringLiteral("processDetails"));
    terminate_ = new QPushButton(QStringLiteral("结束进程"), this); terminate_->setObjectName(QStringLiteral("processTerminate"));
    terminate_->setProperty("textRole", QStringLiteral("section"));
    terminate_->setProperty("actionRole", QStringLiteral("primary"));
    force_ = new QPushButton(QStringLiteral("强制结束…"), this); force_->setObjectName(QStringLiteral("processForceTerminate"));
    force_->setProperty("actionRole", QStringLiteral("danger"));
    actions->addWidget(terminate_); actions->addWidget(details_); actions->addWidget(force_);
    actions->addWidget(ownOnly_); actions->addWidget(refresh);
    interval_ = new QComboBox(this); interval_->setObjectName(QStringLiteral("processInterval"));
    interval_->setAccessibleName(QStringLiteral("进程刷新间隔")); interval_->setToolTip(QStringLiteral("自动刷新间隔"));
    interval_->addItem(QStringLiteral("刷新：0.5 秒"), 500); interval_->addItem(QStringLiteral("刷新：1 秒"), 1000);
    interval_->addItem(QStringLiteral("刷新：2 秒"), 2000); interval_->addItem(QStringLiteral("刷新：5 秒"), 5000); interval_->setCurrentIndex(1);
    actions->addWidget(interval_);
    pause_ = new QPushButton(QStringLiteral("暂停"), this); pause_->setObjectName(QStringLiteral("processPause")); pause_->setCheckable(true);
    actions->addWidget(pause_); root->addWidget(commands);

    model_ = new ProcessTableModel(this); model_->setObjectName(QStringLiteral("processTableModel"));
    filter_ = new ProcessFilterModel(this); filter_->setObjectName(QStringLiteral("processFilterModel")); filter_->setSourceModel(model_);
    table_ = new ProcessTableView(this); table_->setObjectName(QStringLiteral("processTable")); table_->setModel(filter_);
    table_->setItemDelegate(new ProcessHeatDelegate(table_));
    table_->setAccessibleName(QStringLiteral("进程列表"));
    table_->setAccessibleDescription(QStringLiteral("点击列标题排序；CPU 和内存底色表示当前进程的真实占比。"));
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setAlternatingRowColors(true);
    table_->setShowGrid(false); table_->setWordWrap(false);
    // Keep the main measurements beside the name on first use. A restored
    // header state still takes precedence over this initial visual order.
    auto *header = table_->horizontalHeader();
    header->moveSection(header->visualIndex(ProcessTableModel::Cpu), 1);
    header->moveSection(header->visualIndex(ProcessTableModel::Memory), 2);
    table_->setSortingEnabled(true); table_->sortByColumn(ProcessTableModel::Cpu, Qt::DescendingOrder);
    root->addWidget(table_, 1);
    auto *footer = new QHBoxLayout;
    summary_ = plainLabel({}, QStringLiteral("processSummary"), this); footer->addWidget(summary_, 1);
    summary_->setToolTip(QStringLiteral("CPU 按整机总容量计量；内存为驻留内存。仅可结束当前用户的普通进程。"));
    status_ = plainLabel(QStringLiteral("等待首次进程采样"), QStringLiteral("processStatus"), this);
    status_->setAlignment(Qt::AlignRight | Qt::AlignVCenter); footer->addWidget(status_, 1); root->addLayout(footer);
    actionResult_ = plainLabel({}, QStringLiteral("processActionResult"), this); actionResult_->hide(); root->addWidget(actionResult_);

    const QList<QWidget *> tabOrder{search_, terminate_, details_, force_, ownOnly_, refresh, interval_, pause_, table_};
    for (int index = 1; index < tabOrder.size(); ++index) QWidget::setTabOrder(tabOrder[index - 1], tabOrder[index]);

    connect(search_, &QLineEdit::textChanged, filter_, &ProcessFilterModel::setSearchText);
    connect(ownOnly_, &QCheckBox::toggled, filter_, &ProcessFilterModel::setOwnProcessesOnly);
    connect(search_, &QLineEdit::textChanged, this, [this]() { refreshSelection(); refreshStatus(); });
    connect(ownOnly_, &QCheckBox::toggled, this, [this]() { refreshSelection(); refreshStatus(); });
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ProcessPage::refreshSelection);
    connect(table_, &QTableView::doubleClicked, this, [this]() { openDetails(); });
    connect(details_, &QPushButton::clicked, this, &ProcessPage::openDetails);
    connect(terminate_, &QPushButton::clicked, this, [this]() { confirmTermination(false); });
    connect(force_, &QPushButton::clicked, this, [this]() { confirmTermination(true); });
    connect(refresh, &QPushButton::clicked, this, &ProcessPage::refreshRequested);
    connect(pause_, &QPushButton::toggled, this, [this](bool paused) { setPaused(paused); emit pauseRequested(paused); });
    connect(interval_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { emit intervalRequested(interval_->currentData().toInt()); });
    content_->installEventFilter(this);
    scheduleSearchLayoutUpdate();
    refreshSelection(); refreshStatus();
}
bool ProcessPage::eventFilter(QObject *object, QEvent *event)
{
    if (object == content_ && event->type() == QEvent::Resize) scheduleSearchLayoutUpdate();
    return QWidget::eventFilter(object, event);
}
void ProcessPage::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange
        || event->type() == QEvent::StyleChange) scheduleSearchLayoutUpdate();
}
void ProcessPage::scheduleSearchLayoutUpdate()
{
    if (searchLayoutUpdatePending_) return;
    searchLayoutUpdatePending_ = true;
    QTimer::singleShot(0, this, [this]() { searchLayoutUpdatePending_ = false; updateSearchLayout(); });
}
void ProcessPage::updateSearchLayout()
{
    const int available = searchLayout_->geometry().width();
    if (available <= 0) return;
    const QFontMetrics metrics(search_->font(), search_);
    const int spacing = qMax(8, metrics.height() / 3);
    const int iconWidth = search_->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, search_);
    const int searchWidth = metrics.horizontalAdvance(search_->placeholderText())
        + metrics.horizontalAdvance(QLatin1Char('M')) * 2
        + iconWidth + (search_->findChild<QAction *>(QStringLiteral("processSearchIcon")) ? iconWidth + spacing : 0)
        + search_->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, search_) * 2;
    // Keep the search readable at the inherited font size; a compact viewport
    // moves it below the title while the command bar wraps independently.
    const bool stack = available < searchWidth + spacing + title_->sizeHint().width();
    const auto direction = stack ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    if (searchLayout_->direction() != direction) searchLayout_->setDirection(direction);
    if (searchLayout_->spacing() != spacing) searchLayout_->setSpacing(spacing);
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
    confirmation.setFont(font());
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

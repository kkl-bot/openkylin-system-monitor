#include "ui/process_filter_model.h"
#include "ui/process_table_model.h"
#include <limits>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace monitor::ui {
namespace {
quint32 processUserId()
{
#ifdef Q_OS_UNIX
    return static_cast<quint32>(geteuid());
#else
    return std::numeric_limits<quint32>::max();
#endif
}
} // namespace
ProcessFilterModel::ProcessFilterModel(QObject *parent) : QSortFilterProxyModel(parent), currentUserId_(processUserId())
{
    setSortRole(ProcessTableModel::SortRole);
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}
void ProcessFilterModel::setSearchText(const QString &text) { searchText_ = text.trimmed(); invalidateFilter(); }
void ProcessFilterModel::setOwnProcessesOnly(bool enabled) { ownProcessesOnly_ = enabled; invalidateFilter(); }
void ProcessFilterModel::setCurrentUserId(quint32 uid) { currentUserId_ = uid; invalidateFilter(); }
quint32 ProcessFilterModel::currentUserId() const { return currentUserId_; }
bool ProcessFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    const auto *model = qobject_cast<const ProcessTableModel *>(sourceModel());
    const auto *process = model ? model->processAt(sourceRow) : nullptr;
    if (!process || (ownProcessesOnly_ && process->identity.uid != currentUserId_)) return false;
    return searchText_.isEmpty() || process->name.contains(searchText_, Qt::CaseInsensitive)
        || process->user.contains(searchText_, Qt::CaseInsensitive)
        || QString::number(process->identity.pid).contains(searchText_);
}
bool ProcessFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const auto a = sourceModel()->data(left, sortRole());
    const auto b = sourceModel()->data(right, sortRole());
    // Unavailable metrics stay below real measurements in either sort direction.
    if (!a.isValid() || !b.isValid()) {
        if (a.isValid() != b.isValid())
            return a.isValid() ? sortOrder() == Qt::AscendingOrder : sortOrder() == Qt::DescendingOrder;
    } else if (left.column() == ProcessTableModel::Pid || left.column() >= ProcessTableModel::Cpu) {
        if (a.toDouble() != b.toDouble()) return a.toDouble() < b.toDouble();
    } else {
        const int comparison = QString::compare(a.toString(), b.toString(), Qt::CaseInsensitive);
        if (comparison != 0) return comparison < 0;
    }
    const auto *model = qobject_cast<const ProcessTableModel *>(sourceModel());
    const auto *first = model ? model->processAt(left.row()) : nullptr;
    const auto *second = model ? model->processAt(right.row()) : nullptr;
    return first && second ? first->identity.pid < second->identity.pid : left.row() < right.row();
}
} // namespace monitor::ui

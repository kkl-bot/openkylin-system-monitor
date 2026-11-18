#include "ui/process_table_model.h"
#include "ui/process_presentation.h"
#include "domain/metrics.h"
#include <QStringList>
#include <cmath>

namespace monitor::ui {
namespace {
QVariant optionalNumber(const std::optional<double> &value)
{
    return value && std::isfinite(*value) ? QVariant(*value) : QVariant();
}
QString percentage(const std::optional<double> &value)
{
    return value && std::isfinite(*value) ? QString::number(*value, 'f', 1) + QStringLiteral(" %") : QStringLiteral("—");
}
QString rate(const std::optional<double> &value)
{
    if (!value || !std::isfinite(*value) || *value < 0) return QStringLiteral("—");
    const QStringList units = {QStringLiteral("B/s"), QStringLiteral("KiB/s"), QStringLiteral("MiB/s"),
        QStringLiteral("GiB/s"), QStringLiteral("TiB/s"), QStringLiteral("PiB/s"), QStringLiteral("EiB/s")};
    double amount = *value; int unit = 0;
    while (amount >= 1024 && unit + 1 < units.size()) { amount /= 1024; ++unit; }
    return QString::number(amount, 'f', unit == 0 ? 0 : 1) + ' ' + units[unit];
}
QString singleLine(QString value)
{
    value.replace('\n', ' '); value.replace('\r', ' '); value.replace('\t', ' ');
    return value;
}
} // namespace

ProcessTableModel::ProcessTableModel(QObject *parent) : QAbstractTableModel(parent) {}
int ProcessTableModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : processes_.size(); }
int ProcessTableModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : ColumnCount; }
QVariant ProcessTableModel::data(const QModelIndex &index, int role) const
{
    const auto *process = processAt(index.row());
    if (!index.isValid() || !process || index.column() < 0 || index.column() >= ColumnCount) return {};
    if (role == Qt::ToolTipRole) {
        QString fullText;
        if (index.column() == Name) fullText = process->name;
        else if (index.column() == User) fullText = process->user;
        else fullText = data(index, Qt::DisplayRole).toString();
        return QStringLiteral("<qt>") + fullText.toHtmlEscaped().replace('\n', QStringLiteral("<br/>")) + QStringLiteral("</qt>");
    }
    if (role == Qt::TextAlignmentRole)
        return static_cast<int>((index.column() == Pid || index.column() >= Cpu ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    if (role != Qt::DisplayRole && role != SortRole) return {};
    const bool sorting = role == SortRole;
    switch (index.column()) {
    case Name: return singleLine(process->name);
    case Pid: return process->identity.pid;
    case User: return singleLine(process->user);
    case State: return singleLine(sorting ? process->state : processStateLabel(process->state));
    case Cpu: return sorting ? optionalNumber(process->cpuPercent) : QVariant(percentage(process->cpuPercent));
    case Memory: return sorting ? QVariant::fromValue(process->residentBytes) : QVariant(formatBytes(process->residentBytes));
    case MemoryPercent: return sorting ? optionalNumber(process->memoryPercent) : QVariant(percentage(process->memoryPercent));
    case Threads: return process->threads;
    case ReadRate: return sorting ? optionalNumber(process->readBytesPerSecond) : QVariant(rate(process->readBytesPerSecond));
    case WriteRate: return sorting ? optionalNumber(process->writeBytesPerSecond) : QVariant(rate(process->writeBytesPerSecond));
    default: return {};
    }
}
QVariant ProcessTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) return {};
    if (role == Qt::ToolTipRole) {
        if (section == Cpu) return QStringLiteral("占整机总 CPU 容量的比例；多核合计最高为 100%。首次采样显示 —。");
        if (section == Memory) return QStringLiteral("进程当前驻留内存（RSS）；共享内存可能被多个进程计入。");
        if (section == ReadRate || section == WriteRate) return QStringLiteral("进程存储 I/O 速率；权限不足或缺少连续样本时显示 —。");
    }
    if (role != Qt::DisplayRole) return {};
    const QStringList labels = {QStringLiteral("名称"), QStringLiteral("PID"), QStringLiteral("用户"),
        QStringLiteral("状态"), QStringLiteral("CPU"), QStringLiteral("内存"), QStringLiteral("内存 %"),
        QStringLiteral("线程"), QStringLiteral("读取"), QStringLiteral("写入")};
    return section >= 0 && section < labels.size() ? QVariant(labels[section]) : QVariant();
}
void ProcessTableModel::acceptSnapshot(const ProcessSnapshot &snapshot)
{
    beginResetModel(); processes_ = snapshot.processes; endResetModel();
}
const ProcessInfo *ProcessTableModel::processAt(int row) const
{
    return row >= 0 && row < processes_.size() ? &processes_[row] : nullptr;
}
QModelIndex ProcessTableModel::indexForIdentity(const ProcessIdentity &identity) const
{
    for (int row = 0; row < processes_.size(); ++row)
        if (processes_[row].identity == identity) return index(row, Name);
    return {};
}
} // namespace monitor::ui

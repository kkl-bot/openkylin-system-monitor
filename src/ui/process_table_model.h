#pragma once

#include "domain/process_types.h"
#include <QAbstractTableModel>

namespace monitor::ui {

class ProcessTableModel final : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Name, Pid, User, State, Cpu, Memory, MemoryPercent, Threads, ReadRate, WriteRate, ColumnCount };
    enum Role { SortRole = Qt::UserRole + 1 };

    explicit ProcessTableModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void acceptSnapshot(const ProcessSnapshot &snapshot);
    const ProcessInfo *processAt(int row) const;
    QModelIndex indexForIdentity(const ProcessIdentity &identity) const;
private:
    QVector<ProcessInfo> processes_;
};

} // namespace monitor::ui

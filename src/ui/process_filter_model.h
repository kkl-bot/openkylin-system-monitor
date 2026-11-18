#pragma once

#include <QSortFilterProxyModel>

namespace monitor::ui {
class ProcessFilterModel final : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit ProcessFilterModel(QObject *parent = nullptr);
    void setSearchText(const QString &text);
    void setOwnProcessesOnly(bool enabled);
    void setCurrentUserId(quint32 uid);
    quint32 currentUserId() const;
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
private:
    QString searchText_;
    bool ownProcessesOnly_ = false;
    quint32 currentUserId_;
};
} // namespace monitor::ui

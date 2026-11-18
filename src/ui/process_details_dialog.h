#pragma once

#include "domain/process_types.h"
#include <QDialog>

namespace monitor::ui {
class ProcessDetailsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit ProcessDetailsDialog(const ProcessInfo &process, const QDateTime &timestamp, QWidget *parent = nullptr);
protected:
    void showEvent(QShowEvent *event) override;
};
} // namespace monitor::ui

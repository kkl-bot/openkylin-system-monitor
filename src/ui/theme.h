#pragma once
#include "domain/metrics.h"
#include <QColor>
class QApplication;

namespace monitor::ui {
QColor resourceColor(ResourceKind kind);
void applyTheme(QApplication &application);
} // namespace monitor::ui

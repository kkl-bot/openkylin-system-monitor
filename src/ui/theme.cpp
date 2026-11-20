#include "ui/theme.h"
#include "ui/theme_watcher.h"
#include "ui/ui_font.h"
#include <QApplication>

namespace monitor::ui {
QColor resourceColor(ResourceKind kind)
{
    switch (kind) {
    case ResourceKind::Cpu: return QColor("#2979e8");
    case ResourceKind::Gpu: return QColor("#1b9b82");
    case ResourceKind::Memory: return QColor("#ac74dc");
    }
    return QColor("#2979e8");
}
void applyTheme(QApplication &application)
{
    if (application.property("kylinThemeWatcher").toBool()) return;
    application.setProperty("kylinThemeWatcher", true);
    applyUiFont(application);
    new ThemeWatcher(application);
}
} // namespace monitor::ui

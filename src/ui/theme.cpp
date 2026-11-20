#include "ui/theme.h"
#include "ui/theme_watcher.h"
#include <QApplication>
#include <QFontDatabase>
#include <QPalette>

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
    const QStringList candidates = {"Noto Sans CJK SC", "Noto Sans SC", "Microsoft YaHei", "WenQuanYi Micro Hei"};
    const auto families = QFontDatabase().families();
    QFont font = application.font();
    for (const auto &candidate : candidates) {
        if (families.contains(candidate)) { font.setFamily(candidate); break; }
    }
    if (font.pointSizeF() <= 0 && font.pixelSize() <= 0) font.setPointSize(10);
    application.setFont(font);
    application.setStyleSheet(QStringLiteral(R"(
        QWidget { color: palette(window-text); }
        QWidget#detailWindow { background: palette(window); }
        QFrame#sidebar { background: palette(alternate-base); border-right: 1px solid palette(mid); }
        QWidget#resourceHost { background: palette(alternate-base); }
        QFrame#metricCard { background: palette(alternate-base); border: 1px solid palette(mid); border-radius: 10px; }
        QLabel#live { color: palette(highlighted-text); background: palette(highlight); border-radius: 10px; padding: 5px 10px; }
        QPushButton { border: 1px solid palette(mid); border-radius: 6px; padding: 7px 13px; background: palette(button); }
        QPushButton:hover { background: palette(alternate-base); }
        QPushButton:checked { background: palette(highlight); color: palette(highlighted-text); }
        QPushButton#primary { background: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight); }
        QComboBox { border: 1px solid palette(mid); border-radius: 6px; padding: 6px 10px; min-width: 65px; background: palette(base); }
        QComboBox::drop-down { border: none; width: 20px; }
        QMenu { background: palette(window); border: 1px solid palette(mid); padding: 5px; }
        QMenu::item { padding: 7px 20px; }
        QMenu::item:selected { background: palette(highlight); color: palette(highlighted-text); }
        QScrollArea { border: none; background: transparent; }
        QToolTip { background: palette(tool-tip-base); color: palette(tool-tip-text); border: none; padding: 8px; }
    )"));
    if (!application.property("kylinThemeWatcher").toBool()) {
        application.setProperty("kylinThemeWatcher", true);
        new ThemeWatcher(application);
    }
}
} // namespace monitor::ui

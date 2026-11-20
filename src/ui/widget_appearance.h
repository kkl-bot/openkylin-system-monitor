// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
class QApplication;
class QPalette;
namespace monitor::ui {
void applyWidgetAppearance(QApplication &application, const QPalette &systemPalette, bool dark);
} // namespace monitor::ui

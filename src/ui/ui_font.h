// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFont>
#include <QStringList>

class QApplication;

namespace monitor::ui {
// A humanist Latin face, installed STHeiti, then a Simplified Chinese sans fallback.
// Discovery is separate from application so it can be tested without installing fonts.
QStringList uiFontFamilies(const QStringList &availableFamilies);
// Keeps size, weight and scaling; selects the desktop family's Latin/CJK fallback chain.
QFont uiFont(const QFont &base);
// Call after constructing QApplication. Also preserves this family chain when a
// platform theme supplies a new font, while accepting its new point/pixel size.
void applyUiFont(QApplication &application);
} // namespace monitor::ui

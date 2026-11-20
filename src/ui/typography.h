// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFont>

class QWidget;

namespace monitor::ui {
enum class TextRole { Body, Caption, Section, Title, Metric };

QFont typographyFont(const QFont &body, TextRole role);
void applyTypography(QWidget *root);
void enableWindowTypography(QWidget *window);
} // namespace monitor::ui

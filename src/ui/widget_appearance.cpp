// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/widget_appearance.h"
#include <QApplication>
#include <QFile>
#include <QPalette>

static void initializeWidgetResources()
{
    Q_INIT_RESOURCE(resources);
}

namespace monitor::ui {
namespace {
QPalette lightPalette(QPalette palette)
{
    palette.setColor(QPalette::Window, QColor("#f7f9fc"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f4f7fb"));
    palette.setColor(QPalette::Button, Qt::white);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(role, QColor("#23324a"));
    palette.setColor(QPalette::Mid, QColor("#c5cfdf"));
    palette.setColor(QPalette::Midlight, QColor("#dde4ee"));
    palette.setColor(QPalette::Light, Qt::white);
    palette.setColor(QPalette::Dark, QColor("#5b6981"));
    palette.setColor(QPalette::PlaceholderText, QColor("#586880"));
    palette.setColor(QPalette::Highlight, QColor("#4557b4"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor("#364aa8"));
    palette.setColor(QPalette::ToolTipBase, QColor("#24324b"));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, QColor("#69778c"));
    return palette;
}

QPalette darkPalette(QPalette palette)
{
    if (palette.color(QPalette::Window).lightness() < 128) return palette;
    palette.setColor(QPalette::Window, QColor("#24262c"));
    palette.setColor(QPalette::Base, QColor("#1c1e23"));
    palette.setColor(QPalette::AlternateBase, QColor("#2d3038"));
    palette.setColor(QPalette::Button, QColor("#30333b"));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(role, QColor("#e6e8ed"));
    palette.setColor(QPalette::Mid, QColor("#515661"));
    palette.setColor(QPalette::Midlight, QColor("#454b59"));
    palette.setColor(QPalette::PlaceholderText, QColor("#aeb6c6"));
    palette.setColor(QPalette::Highlight, QColor("#416da8"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor("#e3e9f4"));
    palette.setColor(QPalette::ToolTipText, QColor("#202939"));
    return palette;
}

QString readStyleSheet(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Cannot open widget stylesheet: %s", qPrintable(path));
        return {};
    }
    return QString::fromUtf8(file.readAll());
}
} // namespace

void applyWidgetAppearance(QApplication &application, const QPalette &systemPalette, bool dark)
{
    static const auto baseStyle = [] {
        initializeWidgetResources();
        return readStyleSheet(QStringLiteral(":/styles/widgets.qss"));
    }();
    static const auto lightStyle = baseStyle + QLatin1Char('\n') + readStyleSheet(QStringLiteral(":/styles/light.qss"));
    const QPalette palette = dark ? darkPalette(systemPalette) : lightPalette(systemPalette);
    if (application.palette() != palette) application.setPalette(palette);
    const QString &style = dark ? baseStyle : lightStyle;
    if (application.styleSheet() != style) application.setStyleSheet(style);
}
} // namespace monitor::ui

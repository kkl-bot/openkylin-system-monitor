// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/ui_font.h"
#include <QApplication>
#include <QFontDatabase>
#include <QVariant>

namespace monitor::ui {
namespace {

QString firstInstalled(const QStringList &preferred, const QStringList &available)
{
    for (const auto &candidate : preferred) {
        for (const auto &family : available) {
            if (family.compare(candidate, Qt::CaseInsensitive) == 0) return family;
        }
    }
    return {};
}

class UiFontWatcher final : public QObject {
public:
    explicit UiFontWatcher(QApplication &application) : QObject(&application), application_(application)
    {
        connect(&application_, &QGuiApplication::fontChanged, this, [this](const QFont &font) {
            update(font);
        });
        update(application_.font());
    }
private:
    void update(const QFont &font)
    {
        if (updating_) return;
        const auto selected = uiFont(font);
        if (selected == font) return;
        updating_ = true;
        application_.setFont(selected);
        updating_ = false;
    }
    QApplication &application_;
    bool updating_ = false;
};

} // namespace

QStringList uiFontFamilies(const QStringList &availableFamilies)
{
    const auto latin = firstInstalled({QStringLiteral("Ubuntu"), QStringLiteral("Ubuntu Sans"),
        QStringLiteral("Inter"), QStringLiteral("Noto Sans"), QStringLiteral("Segoe UI"),
        QStringLiteral("DejaVu Sans"), QStringLiteral("Liberation Sans")}, availableFamilies);
    const auto chinese = firstInstalled({QStringLiteral("华文黑体"), QStringLiteral("STHeiti")}, availableFamilies);
    const auto chineseFallback = firstInstalled({QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Source Han Sans SC"),
        QStringLiteral("Source Han Sans CN"), QStringLiteral("Noto Sans SC"),
        QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"), QStringLiteral("PingFang SC"),
        QStringLiteral("WenQuanYi Micro Hei"), QStringLiteral("WenQuanYi Zen Hei"),
        QStringLiteral("Droid Sans Fallback"),
        QStringLiteral("国标黑体"), QStringLiteral("GB_HT_GB18030")}, availableFamilies);
    QStringList families;
    if (!latin.isEmpty()) families.append(latin);
    if (!chinese.isEmpty() && !families.contains(chinese)) families.append(chinese);
    if (!chineseFallback.isEmpty() && !families.contains(chineseFallback)) families.append(chineseFallback);
    families.append(QStringLiteral("sans-serif"));
    return families;
}

QFont uiFont(const QFont &base)
{
    // System font files are discovered once; role/resize updates only copy the
    // chosen chain instead of enumerating hundreds of installed faces per widget.
    static const QStringList families = uiFontFamilies(QFontDatabase().families());
    QFont font(base);
    font.setFamilies(families);
    font.setStyleName(QString()); // A former face's "Book"/"Condensed" style is not transferable.
    font.setStyleHint(QFont::SansSerif);
    font.setStyleStrategy(QFont::StyleStrategy(int(base.styleStrategy()) & ~int(QFont::NoFontMerging)));
    return font;
}

void applyUiFont(QApplication &application)
{
    if (!application.property("kylinUiFontWatcher").toBool()) {
        application.setProperty("kylinUiFontWatcher", true);
        new UiFontWatcher(application);
    } else {
        const auto selected = uiFont(application.font());
        if (application.font() != selected) application.setFont(selected);
    }
}

} // namespace monitor::ui

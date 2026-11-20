// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/ui_font.h"
#include "ui/typography.h"
#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGlyphRun>
#include <QLabel>
#include <QRawFont>
#include <QSet>
#include <QTextLayout>
#include <QVBoxLayout>
#include <QtTest>

using namespace monitor::ui;

class UiFontTests final : public QObject {
    Q_OBJECT
private slots:
    void installedFamiliesChooseLatinAndSimplifiedChinese();
    void fontSelectionPreservesPointAndPixelSizes();
    void typographyKeepsFamiliesAndRoleScale();
    void desktopFontChangesPreserveFamilyAndNewSize();
    void windowThemeFontChangesRestoreReadableFamily();
    void actualChineseGlyphsUseSelectedSansFace();
    void missingChineseGlyphUsesSansFallback();
};

void UiFontTests::installedFamiliesChooseLatinAndSimplifiedChinese()
{
    QCOMPARE(uiFontFamilies({QStringLiteral("Ubuntu"), QStringLiteral("华文黑体"), QStringLiteral("Noto Sans CJK SC")}),
             QStringList({QStringLiteral("Ubuntu"), QStringLiteral("华文黑体"), QStringLiteral("Noto Sans CJK SC"), QStringLiteral("sans-serif")}));
    QCOMPARE(uiFontFamilies({QStringLiteral("Ubuntu"), QStringLiteral("STHeiti"), QStringLiteral("Noto Sans CJK SC")}),
             QStringList({QStringLiteral("Ubuntu"), QStringLiteral("STHeiti"), QStringLiteral("Noto Sans CJK SC"), QStringLiteral("sans-serif")}));
    QCOMPARE(uiFontFamilies({QStringLiteral("Noto Serif CJK SC"), QStringLiteral("Noto Sans CJK JP"),
                             QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Ubuntu"), QStringLiteral("Inter")}),
             QStringList({QStringLiteral("Ubuntu"), QStringLiteral("Noto Sans CJK SC"), QStringLiteral("sans-serif")}));
    QCOMPARE(uiFontFamilies({QStringLiteral("Source Han Sans SC"), QStringLiteral("Inter")}),
             QStringList({QStringLiteral("Inter"), QStringLiteral("Source Han Sans SC"), QStringLiteral("sans-serif")}));
    QCOMPARE(uiFontFamilies({QStringLiteral("Noto Sans CJK SC")}),
             QStringList({QStringLiteral("Noto Sans CJK SC"), QStringLiteral("sans-serif")}));
    QCOMPARE(uiFontFamilies({QStringLiteral("Noto Serif CJK SC")}), QStringList({QStringLiteral("sans-serif")}));
}

void UiFontTests::fontSelectionPreservesPointAndPixelSizes()
{
    QFont base(QStringLiteral("serif"));
    base.setPointSizeF(13.5);
    base.setWeight(QFont::Bold);
    base.setItalic(true);
    base.setStyleName(QStringLiteral("Book"));
    base.setStyleStrategy(QFont::NoFontMerging);
    const auto points = uiFont(base);
    QCOMPARE(points.pointSizeF(), 13.5);
    QCOMPARE(points.weight(), base.weight());
    QCOMPARE(points.italic(), base.italic());
    QVERIFY(points.styleName().isEmpty());
    QVERIFY(!(points.styleStrategy() & QFont::NoFontMerging));
    base.setPixelSize(23);
    const auto pixels = uiFont(base);
    QCOMPARE(pixels.pixelSize(), 23);
    QCOMPARE(pixels.families(), points.families());
}

void UiFontTests::typographyKeepsFamiliesAndRoleScale()
{
    QFont base(QStringLiteral("serif"));
    base.setPointSizeF(14);
    const auto families = uiFont(base).families();
    for (const auto role : {TextRole::Body, TextRole::Caption, TextRole::Section, TextRole::Title, TextRole::Metric}) {
        const auto roleFont = typographyFont(base, role);
        QCOMPARE(roleFont.families(), families);
        QVERIFY(roleFont.pointSizeF() >= base.pointSizeF());
    }
    QCOMPARE(typographyFont(base, TextRole::Body).pointSizeF(), 14.0);
    QCOMPARE(typographyFont(base, TextRole::Title).pointSizeF(), 21.0);
    QCOMPARE(typographyFont(base, TextRole::Metric).weight(), QFont::DemiBold);
}

void UiFontTests::desktopFontChangesPreserveFamilyAndNewSize()
{
    const auto previous = QApplication::font();
    applyUiFont(*qApp);
    const auto families = QApplication::font().families();
    QFont theme(QStringLiteral("serif"));
    theme.setPointSizeF(18.5);
    QApplication::setFont(theme);
    QCOMPARE(QApplication::font().families(), families);
    QCOMPARE(QApplication::font().pointSizeF(), 18.5);
    theme.setPixelSize(27);
    QApplication::setFont(theme);
    QCOMPARE(QApplication::font().families(), families);
    QCOMPARE(QApplication::font().pixelSize(), 27);
    // A second application of the policy must be harmless.
    applyUiFont(*qApp);
    QCOMPARE(QApplication::font().pixelSize(), 27);
    QApplication::setFont(previous);
}

void UiFontTests::windowThemeFontChangesRestoreReadableFamily()
{
    applyUiFont(*qApp);
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *label = new QLabel(QStringLiteral("CPU 性能 0123456789"));
    layout->addWidget(label);
    window.resize(760, 500);
    enableWindowTypography(&window);
    window.show();
    QTest::qWait(10);
    const auto expected = window.font();
    auto replaced = expected;
    replaced.setFamily(QStringLiteral("serif"));
    window.setFont(replaced);
    QCOMPARE(window.font().families(), expected.families());
    QCOMPARE(label->font().families(), expected.families());
    QCOMPARE(window.font().pointSizeF(), expected.pointSizeF());
    const auto normalHeight = label->fontMetrics().height();
    window.resize(1180, 780);
    QCoreApplication::processEvents();
    QVERIFY(label->fontMetrics().height() >= normalHeight);
}

void UiFontTests::actualChineseGlyphsUseSelectedSansFace()
{
    const auto installed = QFontDatabase().families();
    const bool hasHeiti = installed.contains(QStringLiteral("华文黑体")) || installed.contains(QStringLiteral("STHeiti"));
    if (!hasHeiti && !installed.contains(QStringLiteral("Noto Sans CJK SC")))
        QSKIP("The installed-glyph verification needs openKylin STHeiti or Noto Sans CJK SC.");
    auto font = uiFont(QApplication::font());
    font.setPointSizeF(14);
    QTextLayout layout(QStringLiteral("中文性能内存网络"), font);
    layout.beginLayout();
    auto line = layout.createLine();
    line.setLineWidth(1000);
    layout.endLayout();
    QSet<QString> actualFamilies;
    for (const auto &run : layout.glyphRuns()) {
        QVERIFY(run.rawFont().isValid());
        actualFamilies.insert(run.rawFont().familyName());
        for (const auto glyph : run.glyphIndexes()) QVERIFY(glyph != 0);
    }
    QCOMPARE(actualFamilies.size(), 1);
    if (hasHeiti)
        QVERIFY(actualFamilies.contains(QStringLiteral("华文黑体")) || actualFamilies.contains(QStringLiteral("STHeiti")));
    else
        QCOMPARE(actualFamilies, QSet<QString>({QStringLiteral("Noto Sans CJK SC")}));
    qInfo().noquote() << "Requested:" << font.families().join(QStringLiteral(" -> "))
                     << "Latin:" << QFontInfo(font).family() << "Chinese:" << actualFamilies.values().join(',');
}

void UiFontTests::missingChineseGlyphUsesSansFallback()
{
    const auto installed = QFontDatabase().families();
    const QString heiti = installed.contains(QStringLiteral("华文黑体")) ? QStringLiteral("华文黑体") : QStringLiteral("STHeiti");
    if (!installed.contains(heiti) || !installed.contains(QStringLiteral("Noto Sans CJK SC")))
        QSKIP("The CJK fallback verification needs both installed openKylin Chinese fonts.");
    const QRawFont primary = QRawFont::fromFont(QFont(heiti, 14));
    const QRawFont fallback = QRawFont::fromFont(QFont(QStringLiteral("Noto Sans CJK SC"), 14));
    if (primary.supportsCharacter(0x9fa6) || !fallback.supportsCharacter(0x9fa6))
        QSKIP("This font revision has a different U+9FA6 glyph coverage.");
    QTextLayout layout(QStringLiteral("性能\u9fa6"), uiFont(QApplication::font()));
    layout.beginLayout();
    auto line = layout.createLine();
    line.setLineWidth(1000);
    layout.endLayout();
    QSet<QString> actualFamilies;
    for (const auto &run : layout.glyphRuns()) {
        actualFamilies.insert(run.rawFont().familyName());
        for (const auto glyph : run.glyphIndexes()) QVERIFY(glyph != 0);
    }
    QVERIFY(actualFamilies.contains(QStringLiteral("华文黑体")) || actualFamilies.contains(QStringLiteral("STHeiti")));
    QVERIFY(actualFamilies.contains(QStringLiteral("Noto Sans CJK SC")));
}

QTEST_MAIN(UiFontTests)
#include "ui_font_tests.moc"

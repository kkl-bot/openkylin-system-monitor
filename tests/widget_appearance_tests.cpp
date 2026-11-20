// SPDX-License-Identifier: GPL-3.0-or-later
// Run only through scripts/test-peony.sh: this test changes the isolated
// org.ukui.style setting while a normal task-manager window remains open.
#include "backend/process_collector.h"
#include "ui/detail_window.h"
#include "ui/monitor_model.h"
#include "ui/process_page.h"
#include "ui/process_table_model.h"
#include "ui/theme.h"
#include "ui/ui_font.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFontInfo>
#include <QImage>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTableView>
#include <QtTest>
#include <cmath>

using namespace monitor;
using namespace monitor::ui;

namespace {

bool hasIsolatedUkuiSession()
{
    // Resolve symlinks before checking. An accidentally inherited user config
    // path must never be accepted merely because another isolation variable exists.
    const QString config = QFileInfo(qEnvironmentVariable("XDG_CONFIG_HOME")).canonicalFilePath();
    // openKylin WSL links /tmp to /var/tmp. Compare both paths in canonical
    // form rather than rejecting the wrapper's legitimate resolved directory.
    const QString temporaryDirectory = QFileInfo(QStringLiteral("/tmp")).canonicalFilePath();
    if (temporaryDirectory.isEmpty()) return false;
    const QRegularExpression pattern(QStringLiteral("^") + QRegularExpression::escape(temporaryDirectory)
        + QStringLiteral("/kylin-monitor-test\\.[A-Za-z0-9]+/config$"));
    if (!pattern.match(config).hasMatch()) return false;
    const QString root = QFileInfo(config).dir().canonicalPath();
    return QFileInfo(qEnvironmentVariable("XDG_CACHE_HOME")).canonicalFilePath() == root + QStringLiteral("/cache")
        && QFileInfo(qEnvironmentVariable("XDG_RUNTIME_DIR")).canonicalFilePath() == root + QStringLiteral("/runtime")
        && qEnvironmentVariable("GSETTINGS_BACKEND") == QStringLiteral("dconf")
        && qEnvironmentVariable("QT_QPA_PLATFORMTHEME") == QStringLiteral("ukui")
        && QGuiApplication::platformName() == QStringLiteral("xcb")
        && !qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS")
        && !qEnvironmentVariableIsEmpty("DISPLAY");
}

bool settingsCommand(const QStringList &arguments, QByteArray *output, QString *error)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), arguments);
    if (!process.waitForStarted(500)) {
        *error = process.errorString();
        return false;
    }
    if (!process.waitForFinished(1500)) {
        process.kill();
        process.waitForFinished(500);
        *error = QStringLiteral("gsettings timed out");
        return false;
    }
    if (output) *output = process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = QString::fromUtf8(process.readAllStandardError()).left(500);
        return false;
    }
    return true;
}

QColor renderedPixel(QWidget *widget, const QPoint &logicalPosition)
{
    const QImage image = widget->grab().toImage();
    if (image.isNull()) return {};
    const qreal scale = image.devicePixelRatio();
    return image.pixelColor(qBound(0, qRound(logicalPosition.x() * scale), image.width() - 1),
                            qBound(0, qRound(logicalPosition.y() * scale), image.height() - 1));
}

QColor tableSurface(QTableView *table)
{
    // Only the current, genuinely collected process is displayed, so the area
    // below its row is an unobstructed main surface even on a busy machine.
    const QWidget *viewport = table->viewport();
    return renderedPixel(table->viewport(), QPoint(viewport->width() / 2, viewport->height() - 10));
}

int colorDistance(const QColor &a, const QColor &b)
{
    return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
}

qreal luminance(const QColor &color)
{
    const auto linear = [](qreal value) {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return linear(color.redF()) * 0.2126 + linear(color.greenF()) * 0.7152 + linear(color.blueF()) * 0.0722;
}

qreal contrast(const QColor &a, const QColor &b)
{
    const qreal first = luminance(a), second = luminance(b);
    return (qMax(first, second) + 0.05) / (qMin(first, second) + 0.05);
}

} // namespace

class WidgetAppearanceTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void openTaskManagerTracksLightDarkLight();
    void cleanupTestCase();
private:
    bool setStyle(const QString &style);
    QByteArray originalStyle_;
    QString commandError_;
    bool isolated_ = false;
};

void WidgetAppearanceTests::initTestCase()
{
    isolated_ = hasIsolatedUkuiSession();
    if (!isolated_)
        QSKIP("Run with scripts/test-peony.sh: refusing theme writes outside its isolated UKUI/config/D-Bus session.");
    QVERIFY2(!QStandardPaths::findExecutable(QStringLiteral("gsettings")).isEmpty(), "gsettings is required.");
    QVERIFY2(settingsCommand({QStringLiteral("get"), QStringLiteral("org.ukui.style"), QStringLiteral("style-name")},
                              &originalStyle_, &commandError_), qPrintable(commandError_));
    originalStyle_ = originalStyle_.trimmed();
    QVERIFY(!originalStyle_.isEmpty());
}

bool WidgetAppearanceTests::setStyle(const QString &style)
{
    // Check again immediately before each write, including restoration.
    if (!isolated_ || !hasIsolatedUkuiSession()) {
        commandError_ = QStringLiteral("The isolated session guard no longer holds.");
        return false;
    }
    return settingsCommand({QStringLiteral("set"), QStringLiteral("org.ukui.style"),
                            QStringLiteral("style-name"), style}, nullptr, &commandError_);
}

void WidgetAppearanceTests::openTaskManagerTracksLightDarkLight()
{
    QVERIFY2(setStyle(QStringLiteral("ukui-light")), qPrintable(commandError_));
    applyTheme(*qApp);
    MonitorModel model;
    DetailWindow window(&model);
    window.resize(1180, 780);
    window.showProcesses();
    // The test never connects process action signals or changes any process.
    ProcessCollector collector;
    auto snapshot = collector.collect(0);
    QVector<ProcessInfo> ownProcess;
    for (const auto &process : snapshot.processes) {
        if (process.identity.pid == QCoreApplication::applicationPid()) ownProcess.append(process);
    }
    QVERIFY2(ownProcess.size() == 1, "Could not read the current test process from /proc.");
    snapshot.processes = ownProcess;
    window.processPage()->acceptSnapshot(snapshot);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *table = window.findChild<QTableView *>(QStringLiteral("processTable"));
    auto *search = window.findChild<QLineEdit *>(QStringLiteral("processSearch"));
    auto *navigation = window.findChild<QPushButton *>(QStringLiteral("processNavigation"));
    QVERIFY(table);
    QVERIFY(search);
    QVERIFY(navigation);
    QTRY_COMPARE(table->model()->rowCount(), 1);
    table->selectRow(0);
    table->setFocus(Qt::OtherFocusReason);
    QTest::qWait(200); // Allow the real gsettings monitor to subscribe after construction.

    const QFont initialApplicationFont = QApplication::font();
    const QFont initialTableFont = table->font();
    const int initialTextHeight = table->fontMetrics().height();
    const QStringList families = uiFont(initialApplicationFont).families();
    QCOMPARE(initialApplicationFont.families(), families);
    QCOMPARE(initialTableFont.families(), families);
    QVERIFY(initialTextHeight > 0);
    QColor lightNavigation, lightSelection;

    for (int phase = 0; phase < 3; ++phase) {
        const bool dark = phase == 1;
        if (phase > 0)
            QVERIFY2(setStyle(dark ? QStringLiteral("ukui-dark") : QStringLiteral("ukui-light")), qPrintable(commandError_));
        // Wait on both propagated palette and actually painted table surface.
        QTRY_VERIFY_WITH_TIMEOUT((QApplication::palette().color(QPalette::Window).lightness() < 128) == dark, 5000);
        QTRY_VERIFY_WITH_TIMEOUT((tableSurface(table).lightness() < 128) == dark, 5000);
        QTest::qWait(150); // Complete queued style/font/layout notifications before reading geometry.
        QVERIFY(window.isVisible());
        QVERIFY(window.showingProcesses());
        QVERIFY(navigation->isChecked());
        const QModelIndex index = table->model()->index(0, ProcessTableModel::Name);
        QVERIFY(table->selectionModel()->isSelected(index));
        const QRect cell = table->visualRect(index);
        QVERIFY(cell.height() > 10);
        const QColor surface = tableSurface(table);
        const QColor selection = renderedPixel(table->viewport(), QPoint(cell.center().x(), cell.top() + 3));
        const QColor navigationFill = renderedPixel(navigation, QPoint(navigation->width() - 12, navigation->height() / 2));
        const QColor marker = renderedPixel(navigation, QPoint(3, navigation->height() / 2));
        qInfo().noquote() << "Theme phase:" << phase << (dark ? "dark" : "light")
                         << "surface:" << surface.name() << "selected:" << selection.name()
                         << "selection text:" << table->palette().color(QPalette::HighlightedText).name()
                         << "navigation:" << navigationFill.name()
                         << "font:" << QFontInfo(table->font()).family()
                         << "pointSize:" << table->font().pointSizeF() << "height:" << initialTextHeight;
        QVERIFY(surface.isValid());
        QVERIFY(selection.isValid());
        QVERIFY(colorDistance(selection, surface) > 35);
        QVERIFY(contrast(selection, table->palette().color(QPalette::HighlightedText)) >= 3.0);
        QVERIFY(colorDistance(marker, navigation->parentWidget()->palette().color(QPalette::Highlight)) <= 3);
        QVERIFY(colorDistance(navigationFill, marker) > 35);
        if (dark) {
            QVERIFY(table->palette().color(QPalette::Text).lightness() > 128);
            QVERIFY(navigationFill.lightness() < 128);
            QVERIFY(colorDistance(navigationFill, lightNavigation) > 100);
        } else {
            QCOMPARE(surface, QColor(Qt::white));
            QCOMPARE(table->palette().color(QPalette::Text), QColor(QStringLiteral("#23324a")));
            QCOMPARE(search->palette().color(QPalette::Text), QColor(QStringLiteral("#23324a")));
            QVERIFY(navigationFill.lightness() > 128);
            if (phase == 0) {
                lightNavigation = navigationFill;
                lightSelection = selection;
            } else {
                QCOMPARE(navigationFill, lightNavigation);
                QCOMPARE(selection, lightSelection);
            }
        }
        QCOMPARE(QApplication::font().families(), families);
        QCOMPARE(QApplication::font().pointSizeF(), initialApplicationFont.pointSizeF());
        QCOMPARE(QApplication::font().pixelSize(), initialApplicationFont.pixelSize());
        QCOMPARE(table->font().families(), families);
        QCOMPARE(table->font().pointSizeF(), initialTableFont.pointSizeF());
        QCOMPARE(table->font().pixelSize(), initialTableFont.pixelSize());
        QCOMPARE(table->fontMetrics().height(), initialTextHeight);
    }
    window.hide();
}

void WidgetAppearanceTests::cleanupTestCase()
{
    if (!isolated_ || originalStyle_.isEmpty()) return;
    QVERIFY2(setStyle(QString::fromUtf8(originalStyle_)), qPrintable(commandError_));
}

QTEST_MAIN(WidgetAppearanceTests)
#include "widget_appearance_tests.moc"

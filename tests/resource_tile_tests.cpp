// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/resource_history_geometry.h"
#include "ui/resource_tile.h"
#include "ui/paint_palette.h"
#include "ui/theme.h"
#include <QSignalSpy>
#include <QTextDocument>
#include <QtTest>
#include <limits>

using namespace monitor;
using namespace monitor::ui;

namespace {

Snapshot memorySnapshot()
{
    Snapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTimeUtc();
    ResourceSample memory;
    memory.id = QStringLiteral("memory");
    memory.kind = ResourceKind::Memory;
    memory.name = QStringLiteral("测试内存");
    memory.usedBytes = quint64(8) * 1024 * 1024 * 1024;
    memory.totalBytes = quint64(16) * 1024 * 1024 * 1024;
    memory.utilization = Metric::available(50, QStringLiteral("fixture"));
    memory.temperature = Metric::unavailable(QStringLiteral("无可用温度传感器"));
    snapshot.resources.append(memory);
    return snapshot;
}

} // namespace

class ResourceTileTests final : public QObject {
    Q_OBJECT
private slots:
    void sparklineUsesElapsedTimeAndKeepsUnknownGaps();
    void sparklineRejectsInvalidSamplesAndDoesNotConnectLongGaps();
    void sharedHistorySupportsTemperatureAndScale();
    void memorySummaryShowsRealCapacityAndMissingValues();
    void cpuSummaryAddsOnlyMeasuredFrequency();
    void resourceRowsRemainClickableAndWrapAtLargeFonts();
    void gpuRowsExposeDistinctNames();
    void selectedRowKeepsNeutralBackgroundAndThemeMarker();
};

void ResourceTileTests::sparklineUsesElapsedTimeAndKeepsUnknownGaps()
{
    const auto end = QDateTime::currentDateTimeUtc();
    const QVector<HistoryPoint> history{
        {end.addMSecs(-6000), 0, {}}, {end.addMSecs(-5000), 50, {}},
        {end.addMSecs(-4000), {}, {}}, {end.addMSecs(-3000), 75, {}}, {end, 100, {}}
    };
    const auto segments = resourceHistorySegments(history, end, QRectF(10, 20, 60, 40), ResourceHistoryMetric::Utilization, 100);
    QCOMPARE(segments.size(), 2);
    QCOMPARE(segments[0].size(), 2);
    QCOMPARE(segments[1].size(), 2);
    QCOMPARE(segments[0][0], QPointF(64, 60));
    QCOMPARE(segments[0][1], QPointF(65, 40));
    QCOMPARE(segments[1][0], QPointF(67, 30));
    QCOMPARE(segments[1][1], QPointF(70, 20));
    // A measured zero is a real point at the baseline, never a missing sample.
    QVERIFY(!segments[0].isEmpty());
}

void ResourceTileTests::sparklineRejectsInvalidSamplesAndDoesNotConnectLongGaps()
{
    const auto end = QDateTime::currentDateTimeUtc();
    const QVector<HistoryPoint> history{
        {end.addMSecs(-61000), 90, {}}, {end.addMSecs(-60000), 0, {}},
        {end.addMSecs(-50000), 10, {}},
        {end.addMSecs(-49000), std::numeric_limits<double>::infinity(), {}},
        {end.addMSecs(-48000), 20, {}}, {end.addMSecs(1), 30, {}}
    };
    const auto segments = resourceHistorySegments(history, end, QRectF(0, 0, 60, 100), ResourceHistoryMetric::Utilization, 100);
    QCOMPARE(segments.size(), 3);
    for (const auto &segment : segments)
        QCOMPARE(segment.size(), 1);
    QCOMPARE(segments[0].first(), QPointF(0, 100));
    QCOMPARE(segments[1].first(), QPointF(10, 90));
    QCOMPARE(segments[2].first(), QPointF(12, 80));
    QVERIFY(resourceHistorySegments({{end, {}, {}}}, end, QRectF(0, 0, 60, 100), ResourceHistoryMetric::Utilization, 100).isEmpty());
    QVERIFY(resourceHistorySegments(history, {}, QRectF(0, 0, 60, 100), ResourceHistoryMetric::Utilization, 100).isEmpty());
}

void ResourceTileTests::sharedHistorySupportsTemperatureAndScale()
{
    const auto end = QDateTime::currentDateTimeUtc();
    QVector<HistoryPoint> history{{end.addMSecs(-2000), 100, 30}, {end.addMSecs(-1000), 100, 60}, {end, 0, 120}};
    const QRectF plot(0, 0, 60, 120);
    const auto temperature = resourceHistorySegments(history, end, plot, ResourceHistoryMetric::Temperature, 120);
    QCOMPARE(temperature.size(), 1);
    QCOMPARE(temperature.first().size(), 3);
    QCOMPARE(temperature.first()[0], QPointF(58, 90));
    QCOMPARE(temperature.first()[1], QPointF(59, 60));
    QCOMPARE(temperature.first()[2], QPointF(60, 0));
    const auto extendedScale = resourceHistorySegments(history, end, plot, ResourceHistoryMetric::Temperature, 240);
    QCOMPARE(extendedScale.first().last(), QPointF(60, 60));
    history[1].temperature = std::numeric_limits<double>::quiet_NaN();
    const auto missing = resourceHistorySegments(history, end, plot, ResourceHistoryMetric::Temperature, 120);
    QCOMPARE(missing.size(), 2);
    QCOMPARE(missing[0].size(), 1);
    QCOMPARE(missing[1].size(), 1);
    QVERIFY(resourceHistorySegments(history, end, plot, ResourceHistoryMetric::Temperature, 0).isEmpty());
    QVERIFY(resourceHistorySegments(history, end, plot, ResourceHistoryMetric::Temperature,
        std::numeric_limits<double>::infinity()).isEmpty());
}

void ResourceTileTests::memorySummaryShowsRealCapacityAndMissingValues()
{
    MonitorModel model;
    auto snapshot = memorySnapshot();
    model.acceptSnapshot(snapshot);
    ResourceTile tile(&model, QStringLiteral("memory"));
    QVERIFY(tile.accessibleDescription().contains(QStringLiteral("8.0 / 16.0 GiB (50%)")));
    QVERIFY(tile.accessibleDescription().contains(QStringLiteral("无可用温度传感器")));
    snapshot.resources[0].usedBytes.reset();
    snapshot.resources[0].utilization = Metric::unavailable(QStringLiteral("容量不可用"));
    snapshot.timestamp = snapshot.timestamp.addSecs(1);
    model.acceptSnapshot(snapshot);
    QVERIFY(tile.accessibleDescription().contains(QStringLiteral("— / 16.0 GiB (—)")));
    QVERIFY(!tile.accessibleDescription().contains(QStringLiteral("0%")));
    QVERIFY(tile.accessibleDescription().contains(QStringLiteral("容量不可用")));
}

void ResourceTileTests::cpuSummaryAddsOnlyMeasuredFrequency()
{
    MonitorModel model;
    Snapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTimeUtc();
    ResourceSample cpu;
    cpu.id = QStringLiteral("cpu");
    cpu.kind = ResourceKind::Cpu;
    cpu.name = QStringLiteral("测试 CPU");
    cpu.utilization = Metric::available(25, QStringLiteral("fixture"));
    cpu.temperature = Metric::available(45, QStringLiteral("fixture"));
    cpu.details.insert(QStringLiteral("平均频率"), QStringLiteral("3.25 GHz"));
    snapshot.resources.append(cpu);
    model.acceptSnapshot(snapshot);
    ResourceTile tile(&model, cpu.id);
    QVERIFY(tile.accessibleDescription().contains(QStringLiteral("25% · 3.25 GHz · 45 °C")));
    for (const auto &missing : {QString(), QStringLiteral("—"), QStringLiteral("0.00 GHz"), QStringLiteral("nan GHz")}) {
        snapshot.resources[0].details.insert(QStringLiteral("平均频率"), missing);
        snapshot.timestamp = snapshot.timestamp.addSecs(1);
        model.acceptSnapshot(snapshot);
        QVERIFY(tile.accessibleDescription().contains(QStringLiteral("25% · 45 °C")));
        QVERIFY(!tile.accessibleDescription().contains(QStringLiteral("GHz")));
    }
    snapshot.resources[0].details.clear();
    snapshot.resources[0].temperature = Metric::unavailable(QStringLiteral("无温度传感器"));
    snapshot.timestamp = snapshot.timestamp.addSecs(1);
    model.acceptSnapshot(snapshot);
    QVERIFY(!tile.accessibleDescription().contains(QStringLiteral(" · ")));
    QVERIFY(tile.accessibleDescription().contains(QStringLiteral("无温度传感器")));
}

void ResourceTileTests::resourceRowsRemainClickableAndWrapAtLargeFonts()
{
    MonitorModel model;
    model.acceptSnapshot(memorySnapshot());
    ResourceTile tile(&model, QStringLiteral("memory"));
    QFont body = tile.font();
    body.setPointSize(12);
    tile.setFont(body);
    const int normalHeight = tile.heightForWidth(225);
    body.setPointSize(24);
    tile.setFont(body);
    QVERIFY(tile.heightForWidth(225) > normalHeight);
    QVERIFY(tile.heightForWidth(225) >= tile.heightForWidth(360));
    tile.resize(225, tile.heightForWidth(225));
    tile.show();
    QTest::qWait(10);
    QSignalSpy clicked(&tile, &QPushButton::clicked);
    QTest::mouseClick(&tile, Qt::LeftButton, Qt::NoModifier, QPoint(tile.width() - 3, tile.height() / 2));
    QCOMPARE(clicked.size(), 1);
    QVERIFY(tile.isChecked());
    QCOMPARE(tile.font().pointSize(), 24);
    QVERIFY(!tile.grab().isNull());
}

void ResourceTileTests::gpuRowsExposeDistinctNames()
{
    MonitorModel model;
    Snapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTimeUtc();
    for (int index = 0; index < 2; ++index) {
        ResourceSample gpu;
        gpu.id = QStringLiteral("gpu-%1").arg(index);
        gpu.kind = ResourceKind::Gpu;
        gpu.name = QStringLiteral("<GPU device %1>").arg(index);
        gpu.utilization = Metric::available(index * 25, QStringLiteral("fixture"));
        snapshot.resources.append(gpu);
    }
    model.acceptSnapshot(snapshot);
    ResourceTile first(&model, QStringLiteral("gpu-0"));
    ResourceTile second(&model, QStringLiteral("gpu-1"));
    QVERIFY(first.accessibleName().startsWith(QStringLiteral("GPU 0 ")));
    QVERIFY(second.accessibleName().startsWith(QStringLiteral("GPU 1 ")));
    QCOMPARE(model.resourceTitle(snapshot.resources[0]), QStringLiteral("GPU 0"));
    QCOMPARE(model.resourceTitle(snapshot.resources[1]), QStringLiteral("GPU 1"));
    QTextDocument tooltip;
    tooltip.setHtml(first.toolTip());
    QVERIFY(tooltip.toPlainText().contains(QStringLiteral("<GPU device 0>")));
    QVERIFY(first.accessibleDescription().contains(QStringLiteral("利用率：0%")));
    QVERIFY(second.accessibleDescription().contains(QStringLiteral("利用率：25%")));
    snapshot.resources.removeLast();
    snapshot.timestamp = snapshot.timestamp.addSecs(1);
    model.acceptSnapshot(snapshot);
    QCOMPARE(model.resourceTitle(snapshot.resources[0]), QStringLiteral("GPU"));
    QVERIFY(first.accessibleName().startsWith(QStringLiteral("GPU <GPU device")));
}

void ResourceTileTests::selectedRowKeepsNeutralBackgroundAndThemeMarker()
{
    MonitorModel model;
    auto snapshot = memorySnapshot();
    ResourceSample cpu = snapshot.resources.first();
    cpu.id = QStringLiteral("cpu"); cpu.kind = ResourceKind::Cpu;
    ResourceSample gpu = snapshot.resources.first();
    gpu.id = QStringLiteral("gpu"); gpu.kind = ResourceKind::Gpu;
    snapshot.resources.append(cpu);
    snapshot.resources.append(gpu);
    model.acceptSnapshot(snapshot);
    const auto closeColor = [](const QColor &actual, const QColor &expected) {
        // Painting quantizes the palette's floating-point blends to image pixels.
        return qAbs(actual.red() - expected.red()) <= 1
            && qAbs(actual.green() - expected.green()) <= 1
            && qAbs(actual.blue() - expected.blue()) <= 1;
    };
    const auto colorDistance = [](const QColor &a, const QColor &b) {
        return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) + qAbs(a.blue() - b.blue());
    };
    QVector<QColor> themeMarkers;
    for (const bool dark : {false, true}) {
        QWidget host;
        auto palette = host.palette();
        const QColor background = dark ? QColor(32, 34, 42) : QColor(244, 247, 251);
        const QColor surface = dark ? QColor(28, 30, 35) : QColor(Qt::white);
        palette.setColor(QPalette::Window, background);
        palette.setColor(QPalette::Base, surface);
        palette.setColor(QPalette::AlternateBase, background);
        palette.setColor(QPalette::WindowText, dark ? QColor(230, 232, 237) : QColor(35, 50, 74));
        palette.setColor(QPalette::Highlight, dark ? QColor(96, 135, 210) : QColor(69, 87, 180));
        host.setPalette(palette);
        host.setAutoFillBackground(true);
        // Generic button rules must not produce a filled rectangle behind this custom row.
        host.setStyleSheet(QStringLiteral("QPushButton { background: #aabbcc; } QPushButton:checked { background: #0000ff; color: white; }"));
        QVector<QColor> resourceMarkers;
        for (const auto &sample : snapshot.resources) {
            ResourceTile tile(&model, sample.id, &host);
            tile.setFocusPolicy(Qt::NoFocus);
            tile.resize(250, tile.heightForWidth(250));
            host.resize(tile.size());
            host.show();
            tile.show();
            QTest::qWait(10);
            tile.setAttribute(Qt::WA_UnderMouse, false);
            const QPoint fillProbe(tile.width() - 10, tile.height() - 6);
            const QPoint markerProbe(3, tile.height() / 2);
            const QPoint edgeProbe(tile.width() - 1, tile.height() / 2);
            const auto pixel = [](const QImage &image, const QPoint &point) {
                const qreal scale = image.devicePixelRatio();
                return image.pixelColor(qRound(point.x() * scale), qRound(point.y() * scale));
            };
            const auto unselected = host.grab().toImage();
            QCOMPARE(pixel(unselected, fillProbe), background);
            QCOMPARE(pixel(unselected, markerProbe), background);
            tile.setChecked(true);
            const auto selected = host.grab().toImage();
            const QColor fill = pixel(selected, fillProbe);
            // Selection uses a restrained theme tint and border, while the
            // narrow left marker conveys the actual CPU/memory/GPU resource.
            QVERIFY(closeColor(fill, blendPaintColor(surface, palette.color(QPalette::Highlight), 0.07)));
            QVERIFY(fill != background);
            QVERIFY(fill != palette.color(QPalette::Highlight));
            QVERIFY(fill.blue() > fill.red());
            // The one-pixel border may be antialiased at fractional display scales.
            QVERIFY(colorDistance(pixel(selected, edgeProbe), palette.color(QPalette::Highlight))
                < colorDistance(fill, palette.color(QPalette::Highlight)));
            const QColor marker = pixel(selected, markerProbe);
            QVERIFY(closeColor(marker, chartAccentColor(resourceColor(sample.kind), palette)));
            QVERIFY(marker != fill);
            QVERIFY(marker != palette.color(QPalette::Highlight));
            if (sample.kind == ResourceKind::Cpu)
                QVERIFY(marker.blue() > marker.green() && marker.green() > marker.red());
            else if (sample.kind == ResourceKind::Memory)
                QVERIFY(marker.blue() > marker.red() && marker.red() > marker.green());
            else
                QVERIFY(marker.green() > marker.blue() && marker.blue() > marker.red());
            resourceMarkers.append(marker);
            themeMarkers.append(marker);
        }
        QCOMPARE(resourceMarkers.size(), 3);
        for (int i = 0; i < resourceMarkers.size(); ++i) {
            for (int j = i + 1; j < resourceMarkers.size(); ++j) {
                QVERIFY(colorDistance(resourceMarkers[i], resourceMarkers[j]) > 60);
            }
        }
    }
    for (int i = 0; i < 3; ++i) QVERIFY(themeMarkers[i] != themeMarkers[i + 3]);
}

QTEST_MAIN(ResourceTileTests)
#include "resource_tile_tests.moc"

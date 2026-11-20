// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/monitor_service.h"
#include "backend/process_service.h"
#include "ui/detail_window.h"
#include "ui/monitor_model.h"
#include "ui/process_page.h"
#include "ui/theme.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QPixmap>
#include <QTableView>
#include <QTextStream>
#include <QTimer>
#include <QVector>
#include <functional>

namespace {

enum class CapturePage { Processes, Cpu, Memory };

struct CaptureStep {
    QSize requestedSize;
    CapturePage page = CapturePage::Cpu;
    bool largeFont = false;
};

QString captureName(const CaptureStep &step)
{
    return QStringLiteral("%1-%2x%3%4.png")
        .arg(step.page == CapturePage::Processes ? QStringLiteral("processes")
            : step.page == CapturePage::Memory ? QStringLiteral("memory") : QStringLiteral("cpu"))
        .arg(step.requestedSize.width()).arg(step.requestedSize.height())
        .arg(step.largeFont ? QStringLiteral("-font150") : QString());
}

QFont enlargedFont(const QFont &base)
{
    QFont result(base);
    if (base.pointSizeF() > 0)
        result.setPointSizeF(base.pointSizeF() * 1.5);
    else if (base.pixelSize() > 0)
        result.setPixelSize(qMax(1, qRound(base.pixelSize() * 1.5)));
    return result;
}

bool hasWarmHistory(const monitor::ui::MonitorModel &model, const QString &resourceId)
{
    QDateTime firstSample;
    QDateTime lastSample;
    int sampleCount = 0;
    for (const auto &point : model.history(resourceId)) {
        if (!point.timestamp.isValid() || !point.utilization) {
            firstSample = {};
            lastSample = {};
            sampleCount = 0;
            continue;
        }
        if (!firstSample.isValid())
            firstSample = point.timestamp;
        lastSample = point.timestamp;
        ++sampleCount;
    }
    return sampleCount >= 16 && firstSample.msecsTo(lastSample) >= 15000;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    QApplication::setApplicationName(QStringLiteral("kylin-monitor-layout-preview"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/kylin-monitor.svg")));
    monitor::ui::applyTheme(app);
    if (app.arguments().size() != 2) {
        QTextStream(stderr) << "Usage: layout_preview OUTPUT_DIRECTORY\n";
        return 2;
    }
    const QDir output(app.arguments().at(1));
    if (!QDir().mkpath(output.absolutePath())) {
        QTextStream(stderr) << "Cannot create output directory: " << output.absolutePath() << '\n';
        return 2;
    }

    monitor::ui::MonitorModel model;
    monitor::ui::DetailWindow window(&model);
    monitor::MonitorService resourceService;
    monitor::ProcessService processService;
    bool receivedProcesses = false;
    QObject::connect(&resourceService, &monitor::MonitorService::snapshotReady,
                     &model, &monitor::ui::MonitorModel::acceptSnapshot);
    QObject::connect(&processService, &monitor::ProcessService::snapshotReady,
                     window.processPage(), &monitor::ui::ProcessPage::acceptSnapshot);
    QObject::connect(&processService, &monitor::ProcessService::snapshotReady, &app,
                     [&receivedProcesses](const monitor::ProcessSnapshot &snapshot) {
        receivedProcesses = snapshot.timestamp.isValid() && !snapshot.processes.isEmpty();
    });
    // Keep both read-only samplers active for the capture sequence so switching
    // pages does not clear the process rate baseline between screenshots.
    QElapsedTimer samplingTimer;
    samplingTimer.start();
    resourceService.setInterval(1000);
    processService.setInterval(1000);
    processService.setActive(true);
    resourceService.start();
    processService.start();
    window.showProcesses();
    window.show();

    const QFont normalFont = QApplication::font();
    QFont largeFont;
    auto *processTable = window.findChild<QTableView *>(QStringLiteral("processTable"));
    if (!processTable) {
        QTextStream(stderr) << "Process table was not found.\n";
        return 4;
    }
    int normalTableFontHeight = 0;
    const QVector<CaptureStep> steps{
        {{760, 500}, CapturePage::Processes, false}, {{760, 500}, CapturePage::Memory, false}, {{760, 500}, CapturePage::Cpu, false},
        {{1024, 720}, CapturePage::Processes, false}, {{1024, 720}, CapturePage::Memory, false}, {{1024, 720}, CapturePage::Cpu, false},
        {{1180, 780}, CapturePage::Processes, false}, {{1180, 780}, CapturePage::Memory, false}, {{1180, 780}, CapturePage::Cpu, false},
        {{760, 500}, CapturePage::Processes, true}, {{760, 500}, CapturePage::Memory, true}, {{760, 500}, CapturePage::Cpu, true}
    };
    int stepIndex = 0;
    std::function<void()> captureNext;
    const auto saveStep = [&](CaptureStep step) {
        const int tableFontHeight = processTable->fontMetrics().height();
        if (!step.largeFont && step.page == CapturePage::Processes && step.requestedSize == QSize(760, 500)) {
            normalTableFontHeight = tableFontHeight;
            largeFont = enlargedFont(processTable->font());
        }
        const auto *memory = model.resource(QStringLiteral("memory"));
        if (step.page == CapturePage::Memory && (!memory || !memory->usedBytes || !memory->totalBytes
            || *memory->totalBytes == 0 || *memory->usedBytes > *memory->totalBytes)) {
            QTextStream(stderr) << "Valid real memory capacity was not received; refusing to save a memory screenshot.\n";
            app.exit(4);
            return;
        }
        const bool fontEnlarged = tableFontHeight >= qMax(normalTableFontHeight + 1,
                                                       qRound(normalTableFontHeight * 1.4));
        if (step.largeFont && !fontEnlarged) {
            QTextStream(stderr) << "Font stress request did not enlarge the actual table font: baselineHeight="
                << normalTableFontHeight << " actualHeight=" << tableFontHeight
                << ". Refusing to save a misleading font150 screenshot.\n";
            app.exit(5);
            return;
        }
        const QString filename = captureName(step);
        const QPixmap screenshot = window.grab();
        if (screenshot.isNull() || !screenshot.save(output.filePath(filename))) {
            QTextStream(stderr) << "Failed to save " << output.filePath(filename) << '\n';
            app.exit(3);
            return;
        }
        QTextStream(stdout) << filename
            << " requested=" << step.requestedSize.width() << 'x' << step.requestedSize.height()
            << " actual=" << window.width() << 'x' << window.height()
            << " pixels=" << screenshot.width() << 'x' << screenshot.height()
            << " devicePixelRatio=" << screenshot.devicePixelRatio()
            << " tablePointSize=" << processTable->font().pointSizeF()
            << " tablePixelSize=" << processTable->font().pixelSize()
            << " tableFontHeight=" << tableFontHeight
            << " baselineTableFontHeight=" << normalTableFontHeight
            << " memoryUsedBytes=" << (memory && memory->usedBytes ? QString::number(*memory->usedBytes) : QStringLiteral("unavailable"))
            << " memoryTotalBytes=" << (memory && memory->totalBytes ? QString::number(*memory->totalBytes) : QStringLiteral("unavailable")) << '\n';
        ++stepIndex;
        captureNext();
    };
    captureNext = [&]() {
        if (stepIndex >= steps.size()) {
            app.exit(0);
            return;
        }
        const CaptureStep step = steps.at(stepIndex);
        QApplication::setFont(step.largeFont ? largeFont : normalFont);
        if (step.page == CapturePage::Processes) {
            window.showProcesses();
            if (processTable->model()->rowCount() > 0) processTable->selectRow(0);
        } else {
            window.selectResource(step.page == CapturePage::Memory ? QStringLiteral("memory") : QStringLiteral("cpu"));
        }
        window.resize(step.requestedSize);
        // Returning to the Qt event loop lets deferred layout, resize and paint
        // events complete while real sampler updates continue normally.
        QTimer::singleShot(250, &app, [&, step]() { saveStep(step); });
    };
    QTimer warmupTimer;
    warmupTimer.setInterval(250);
    QObject::connect(&warmupTimer, &QTimer::timeout, &app, [&]() {
        if (samplingTimer.elapsed() < 15000 || !receivedProcesses
            || !hasWarmHistory(model, QStringLiteral("cpu"))
            || !hasWarmHistory(model, QStringLiteral("memory")))
            return;
        warmupTimer.stop();
        QTextStream(stdout) << "Real CPU and memory histories span at least 15 seconds; starting capture after "
            << samplingTimer.elapsed() << " ms.\n";
        captureNext();
    });
    warmupTimer.start();
    QTimer::singleShot(45000, &app, [&]() {
        if (!warmupTimer.isActive())
            return;
        warmupTimer.stop();
        QTextStream(stderr) << "Timed out waiting for 15 seconds of real CPU/memory history and process samples; no screenshots saved.\n";
        app.exit(4);
    });
    QTimer::singleShot(60000, &app, [&]() {
        QTextStream(stderr) << "Layout capture exceeded the 60-second overall deadline.\n";
        app.exit(6);
    });
    return app.exec();
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/monitor_service.h"
#include "backend/network_service.h"
#include "ui/detail_window.h"
#include "ui/monitor_model.h"
#include "ui/network_details_dialog.h"
#include "ui/network_page.h"
#include "ui/theme.h"
#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QPixmap>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    QApplication::setApplicationName(QStringLiteral("kylin-monitor-network-preview"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/kylin-monitor.svg")));
    monitor::ui::applyTheme(app);
    if (app.arguments().size() != 2) {
        QTextStream(stderr) << "Usage: network_preview OUTPUT_DIRECTORY\n";
        return 2;
    }
    const QDir output(app.arguments().at(1));
    if (!QDir().mkpath(output.absolutePath())) return 2;
    monitor::ui::MonitorModel model;
    monitor::ui::DetailWindow window(&model);
    monitor::MonitorService resources;
    monitor::NetworkService network;
    monitor::NetworkSnapshot latest;
    QObject::connect(&resources, &monitor::MonitorService::snapshotReady,
                     &model, &monitor::ui::MonitorModel::acceptSnapshot);
    QObject::connect(&network, &monitor::NetworkService::snapshotReady,
                     window.networkPage(), &monitor::ui::NetworkPage::acceptSnapshot);
    QObject::connect(&network, &monitor::NetworkService::snapshotReady, &app,
                     [&](const monitor::NetworkSnapshot &snapshot) { latest = snapshot; });
    resources.start(); network.setActive(true); network.start();
    window.showNetwork(); window.resize(1180, 780); window.show();
    const auto save = [&](QWidget &widget, const QString &name) {
        const auto pixels = widget.grab();
        if (pixels.isNull() || !pixels.save(output.filePath(name))) return false;
        QTextStream(stdout) << name << " actual=" << widget.width() << 'x' << widget.height()
            << " sampled=" << latest.timestamp.toString(Qt::ISODate) << '\n';
        return true;
    };
    QTimer::singleShot(4500, &app, [&]() {
        const QString name = window.networkPage()->selectedInterface();
        bool sampled = false;
        for (const auto &interface : latest.interfaces)
            if (interface.name == name && interface.receivedBytes && interface.transmittedBytes
                && interface.downloadBytesPerSecond && interface.uploadBytesPerSecond) sampled = true;
        if (!sampled || !model.snapshot().timestamp.isValid()) {
            QTextStream(stderr) << "Real network/resource samples missing; no screenshots saved.\n";
            app.exit(4); return;
        }
        if (!save(window, QStringLiteral("network-1180x780.png"))) { app.exit(3); return; }
        window.resize(760, 500);
        QTimer::singleShot(300, &app, [&]() {
            if (!save(window, QStringLiteral("network-760x500.png"))) { app.exit(3); return; }
            for (const auto &interface : latest.interfaces) {
                if (interface.name != window.networkPage()->selectedInterface()) continue;
                auto *details = new monitor::ui::NetworkDetailsDialog(interface, latest.proxy, latest.timestamp, &window);
                details->resize(800, 800); details->show();
                QTimer::singleShot(300, &app, [&, details]() {
                    const bool saved = save(*details, QStringLiteral("network-details.png"));
                    details->close(); app.exit(saved ? 0 : 3);
                });
                return;
            }
            app.exit(4);
        });
    });
    return app.exec();
}

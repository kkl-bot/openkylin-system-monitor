#include "app/application_controller.h"
#include "domain/snapshot_json.h"
#include "ui/theme.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    bool probe = false;
    for (int i = 1; i < argc; ++i) if (QString::fromLocal8Bit(argv[i]) == "--probe") probe = true;
    if (probe) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    QApplication::setOrganizationName("KylinMonitor"); QApplication::setApplicationName("kylin-monitor");
    QApplication::setApplicationVersion("1.3.0"); QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon(":/kylin-monitor.svg"));
    monitor::ui::applyTheme(app);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("openKylin 任务管理器与桌面资源监测插件"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"mini", QStringLiteral("以极简悬浮窗启动")});
    parser.addOption({"detail", QStringLiteral("以详细模式启动")});
    parser.addOption({"processes", QStringLiteral("打开任务管理器进程页")});
    parser.addOption({"network", QStringLiteral("打开网络状态监控页")});
    parser.addOption({"probe", QStringLiteral("采样两次后输出真实硬件数据 JSON 并退出")});
    parser.addOption({"capture-dir", QStringLiteral("保存真实采样的界面预览后退出"), "directory"});
    parser.process(app);
    if (parser.isSet("probe")) {
        monitor::MonitorService service;
        int count = 0;
        QObject::connect(&service, &monitor::MonitorService::snapshotReady, &app, [&](const monitor::Snapshot &snapshot) {
            if (++count >= 2) {
                QTextStream(stdout) << monitor::snapshotJson(snapshot, false).toJson();
                app.quit();
            }
        });
        QTimer::singleShot(8000, &app, [&app]() { app.exit(2); });
        service.start(); return app.exec();
    }
    monitor::ApplicationController controller;
    controller.start(parser.isSet("mini"), parser.isSet("detail") || parser.isSet("capture-dir"));
    if (parser.isSet("processes") || parser.isSet("capture-dir")) controller.showProcesses();
    if (parser.isSet("network")) controller.showNetwork();
    if (parser.isSet("capture-dir")) {
        QTimer::singleShot(3500, &app, [&]() {
            app.exit(controller.savePreviews(parser.value("capture-dir")) ? 0 : 3);
        });
    }
    return app.exec();
}

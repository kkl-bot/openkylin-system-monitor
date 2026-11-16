#include "app/application_controller.h"
#include "ui/theme.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

namespace {
QJsonObject metricJson(const monitor::Metric &metric)
{
    return {{"value", metric.value ? QJsonValue(*metric.value) : QJsonValue(QJsonValue::Null)},
            {"reason", metric.reason}, {"source", metric.source}};
}
void printSnapshot(const monitor::Snapshot &snapshot)
{
    QJsonArray resources;
    for (const auto &resource : snapshot.resources) {
        resources.append(QJsonObject{{"id", resource.id}, {"name", resource.name},
                                      {"utilization_percent", metricJson(resource.utilization)},
                                      {"temperature_celsius", metricJson(resource.temperature)}});
    }
    QTextStream(stdout) << QJsonDocument(QJsonObject{{"timestamp", snapshot.timestamp.toString(Qt::ISODate)},
                                                    {"wsl", snapshot.isWsl}, {"resources", resources}}).toJson();
}
} // namespace

int main(int argc, char *argv[])
{
    bool probe = false;
    for (int i = 1; i < argc; ++i) if (QString::fromLocal8Bit(argv[i]) == "--probe") probe = true;
    if (probe) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    QApplication::setOrganizationName("KylinMonitor"); QApplication::setApplicationName("kylin-monitor");
    QApplication::setApplicationVersion("1.1.0"); QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon(":/kylin-monitor.svg"));
    monitor::ui::applyTheme(app);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("openKylin CPU / GPU / 内存监测小组件"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"mini", QStringLiteral("以极简悬浮窗启动")});
    parser.addOption({"detail", QStringLiteral("以详细模式启动")});
    parser.addOption({"probe", QStringLiteral("采样两次后输出真实硬件数据 JSON 并退出")});
    parser.addOption({"capture-dir", QStringLiteral("保存真实采样的界面预览后退出"), "directory"});
    parser.process(app);
    if (parser.isSet("probe")) {
        monitor::MonitorService service;
        int count = 0;
        QObject::connect(&service, &monitor::MonitorService::snapshotReady, &app, [&](const monitor::Snapshot &snapshot) {
            if (++count >= 2) { printSnapshot(snapshot); app.quit(); }
        });
        QTimer::singleShot(8000, &app, [&app]() { app.exit(2); });
        service.start(); return app.exec();
    }
    monitor::ApplicationController controller;
    controller.start(parser.isSet("mini"), parser.isSet("detail") || parser.isSet("capture-dir"));
    if (parser.isSet("capture-dir")) {
        QTimer::singleShot(3500, &app, [&]() {
            app.exit(controller.savePreviews(parser.value("capture-dir")) ? 0 : 3);
        });
    }
    return app.exec();
}

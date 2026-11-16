// SPDX-License-Identifier: GPL-3.0-or-later
#include <widget-loader.h>
#include <widget.h>
#include <widget-item.h>
#include <widget-container.h>
#include <config.h>
#include <shared-engine-component.h>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QQuickWindow>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

using namespace UkuiQuick;
namespace {
constexpr auto widgetId = "org.ukui.kylin-monitor";
bool theme(const QString &name)
{
    return QProcess::execute("gsettings", {"set", "org.ukui.style", "style-name", name}) == 0;
}
}

class Host : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QObject *peonyShell READ shell CONSTANT)
public:
    explicit Host(QQuickItem *parent) : QQuickItem(parent) {}
    QObject *shell() { return this; }
    bool allowResize = true;
    int removedId = -1;
    bool managerShown = false;
    Q_INVOKABLE bool canResizeWidgetTo(QObject *, int width, int height) { return allowResize && width <= 296 && height <= 296; }
    Q_INVOKABLE void removeWidgetInstance(int id) { removedId = id; }
    Q_INVOKABLE void showWidgetManager() { managerShown = true; }
};

class PeonyIntegrationTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void discoversNativeMetadata();
    void loadsAllSizesAndPreviewHasNoPersistentSideEffects();
    void sharedActionsRespectHostAndCollisions();
    void settingsPersistAcrossDestructionAndReload();
    void liveThemeAndScreenshots();
    void cleanupTestCase();
private:
    struct Instance {
        QQuickWindow window;
        Host host{window.contentItem()};
        std::unique_ptr<QQmlContext> context;
        std::unique_ptr<Config> config;
        std::unique_ptr<Widget> widget;
        std::unique_ptr<WidgetQuickItem> item;
    };
    std::unique_ptr<Instance> create(const QString &variant, bool preview = false, const QString &file = {});
    bool invoke(Instance &instance, const char *method, const QVariant &arg);
    QAction *action(Instance &instance, const QString &text);
};
void PeonyIntegrationTests::initTestCase()
{
    qRegisterMetaType<UkuiQuick::WidgetContainer *>("UkuiQuick::WidgetContainer*");
    SharedEngineComponent::sharedEngine()->addImportPath(QStringLiteral(MONITOR_QML_PATH));
    WidgetLoader::globalLoader().addWidgetSearchPath(QStringLiteral(MONITOR_WIDGET_PATH));
    QVERIFY(theme("ukui-light"));
}
std::unique_ptr<PeonyIntegrationTests::Instance> PeonyIntegrationTests::create(const QString &variant, bool preview, const QString &file)
{
    auto result = std::make_unique<Instance>();
    const bool large = variant == "large", small = variant == "small";
    const int width = small ? 136 : 296, height = large ? 296 : 136;
    QVariantMap config{{"id", widgetId}, {"variantId", variant}, {"width", width}, {"height", height}};
    result->config.reset(file.isEmpty() ? new Config(config, static_cast<QObject *>(nullptr)) : new Config(file));
    result->widget.reset(WidgetLoader::globalLoader().loadWidget(widgetId));
    if (!result->widget) return {};
    result->widget->setInstanceId(preview ? -1 : 42);
    result->widget->setConfig(result->config.get());
    result->context = std::make_unique<QQmlContext>(SharedEngineComponent::sharedEngine()->rootContext());
    result->context->setContextProperty("widgetPreviewMode", preview);
    result->item.reset(WidgetQuickItem::loadWidgetItem(result->widget.get(), result->context.get()));
    if (!result->item) { qWarning() << result->widget->uiError(); return {}; }
    result->host.setSize(QSizeF(width, height));
    result->item->setParentItem(&result->host);
    result->item->setSize(QSizeF(width, height));
    result->window.resize(width, height); result->window.setColor(Qt::transparent); result->window.show();
    QCoreApplication::processEvents();
    return result;
}
bool PeonyIntegrationTests::invoke(Instance &instance, const char *method, const QVariant &arg)
{
    QVariant result;
    const bool called = QMetaObject::invokeMethod(instance.item.get(), method, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, arg));
    return called && result.toBool();
}
QAction *PeonyIntegrationTests::action(Instance &instance, const QString &text)
{
    for (auto *action : instance.widget->actions()) if (action->text() == text) return action;
    return nullptr;
}
void PeonyIntegrationTests::discoversNativeMetadata()
{
    WidgetLoader loader; loader.addWidgetSearchPath(QStringLiteral(MONITOR_WIDGET_PATH));
    loader.setShowInFilter(WidgetMetadata::Desktop);
    bool found = false;
    for (const auto &entry : loader.widgetsMetadata()) if (entry.id() == widgetId) found = true;
    QVERIFY(found);
    const auto metadata = loader.loadMetadata(widgetId);
    QCOMPARE(metadata.id(), QString(widgetId));
    const auto content = metadata.contents();
    QVERIFY(content.value("ShowInManager").toBool());
    QCOMPARE(content.value("ManagerSizes").toList().size(), 3);
}
void PeonyIntegrationTests::loadsAllSizesAndPreviewHasNoPersistentSideEffects()
{
    for (const auto &size : {"small", "medium", "large"}) {
        auto instance = create(size, true); QVERIFY(instance); QVERIFY(!instance->widget->hasUiError());
        QCOMPARE(instance->item->property("sizeMode").toString(), QString(size));
        QVERIFY(!instance->config->contains("settingsVersion"));
        QVERIFY(!invoke(*instance, "switchToVariant", "large"));
        QVERIFY(!instance->config->contains("paused"));
        for (auto *action : instance->widget->actions()) if (!action->isSeparator()) QVERIFY(!action->isEnabled());
    }
}
void PeonyIntegrationTests::sharedActionsRespectHostAndCollisions()
{
    auto instance = create("small"); QVERIFY(instance);
    QVERIFY(invoke(*instance, "switchToVariant", "medium"));
    QCOMPARE(instance->item->width(), 296.0); QCOMPARE(instance->item->height(), 136.0);
    QCOMPARE(instance->config->getValue("variantId").toString(), QString("medium"));
    instance->host.allowResize = false;
    QVERIFY(!invoke(*instance, "switchToVariant", "large")); QCOMPARE(instance->item->height(), 136.0);
    auto *manager = action(*instance, QStringLiteral("更多插件")); QVERIFY(manager); manager->trigger(); QVERIFY(instance->host.managerShown);
    auto *remove = action(*instance, QStringLiteral("移除本插件")); QVERIFY(remove); remove->trigger(); QCOMPARE(instance->host.removedId, 42);
}
void PeonyIntegrationTests::settingsPersistAcrossDestructionAndReload()
{
    QTemporaryDir directory; const auto path = directory.filePath("instance.json");
    { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"variantId\":\"small\",\"width\":136,\"height\":136}"); }
    {
        auto instance = create("small", false, path); QVERIFY(instance);
        QVERIFY(invoke(*instance, "switchToVariant", "large"));
        QVERIFY(QMetaObject::invokeMethod(instance->item.get(), "setInterval", Q_ARG(QVariant, 2000)));
        QVERIFY(QMetaObject::invokeMethod(instance->item.get(), "togglePause"));
        QVERIFY(QMetaObject::invokeMethod(instance->item.get(), "writeSetting", Q_ARG(QVariant, "selectedGpu"), Q_ARG(QVariant, "persistent-gpu")));
        QTRY_COMPARE(instance->config->getValue("interval").toInt(), 2000);
        QTRY_VERIFY(instance->config->getValue("paused").toBool());
        instance->config->forceSync();
        QTRY_VERIFY([&]() {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return false;
            return QJsonDocument::fromJson(file.readAll()).object().value("interval").toInt() == 2000;
        }());
    }
    auto restored = create("large", false, path); QVERIFY(restored);
    auto *backend = restored->item->findChild<QObject *>("monitorBackend"); QVERIFY(backend);
    QCOMPARE(backend->property("interval").toInt(), 2000); QVERIFY(backend->property("paused").toBool());
    QCOMPARE(backend->property("selectedGpu").toString(), QString("persistent-gpu"));
    QCOMPARE(restored->config->getValue("variantId").toString(), QString("large"));
}
void PeonyIntegrationTests::liveThemeAndScreenshots()
{
    auto instance = create("large"); QVERIFY(instance);
    QVERIFY(theme("ukui-light")); QTRY_VERIFY(!instance->item->property("darkMode").toBool());
    const auto lightColor = instance->item->property("surfaceColor").value<QColor>();
    QVERIFY(theme("ukui-dark")); QTRY_VERIFY_WITH_TIMEOUT(instance->item->property("darkMode").toBool(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(instance->item->property("surfaceColor").value<QColor>() != lightColor, 5000);
    const auto output = qEnvironmentVariable("KYLIN_MONITOR_CAPTURE_DIR");
    if (!output.isEmpty()) {
        QVERIFY(QDir().mkpath(output));
        for (const auto &mode : {"ukui-light", "ukui-dark"}) {
            QVERIFY(theme(mode)); QTest::qWait(400);
            for (const auto &size : {"small", "medium", "large"}) {
                auto capture = create(size); QVERIFY(capture); QTest::qWait(350);
                const auto image = capture->window.grabWindow(); QVERIFY(!image.isNull());
                QVERIFY(image.save(QDir(output).filePath(QString(mode) + "-" + size + ".png")));
            }
        }
    }
}
void PeonyIntegrationTests::cleanupTestCase()
{
    // The installed UKUI style helper posts theme initialization on QQmlThread.
    // Let those cross-thread callbacks finish while QApplication still exists.
    // Its helper otherwise dereferences qApp during an immediate test shutdown.
    QTest::qWait(500);
}
QTEST_MAIN(PeonyIntegrationTests)
#include "peony_integration_tests.moc"

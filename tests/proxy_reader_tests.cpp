// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/proxy_reader.h"
#include <QtTest>

using namespace monitor;

namespace {

ProxyReader::EnvironmentProvider emptyEnvironment()
{
    return []() { return QProcessEnvironment{}; };
}

QString visibleText(const ProxyInfo &info)
{
    return info.source + info.detail + info.environmentVariables.join(QLatin1Char(' '));
}

} // namespace

class ProxyReaderTests final : public QObject {
    Q_OBJECT
private slots:
    void readsDesktopModesOnly_data();
    void readsDesktopModesOnly();
    void failuresRemainUnknown_data();
    void failuresRemainUnknown();
    void environmentExposesNamesWithoutCredentials();
    void noProxyExceptionsDoNotEnableProxy();
    void ignoresAptAndUnrelatedProxyEnvironment();
    void readReflectsChangedConfiguration();
};

void ProxyReaderTests::readsDesktopModesOnly_data()
{
    QTest::addColumn<QByteArray>("output");
    QTest::addColumn<int>("mode");
    QTest::newRow("disabled") << QByteArray("'none'\n") << int(ProxyMode::Disabled);
    QTest::newRow("manual") << QByteArray("'manual'\n") << int(ProxyMode::Manual);
    QTest::newRow("pac") << QByteArray("  'auto'\n") << int(ProxyMode::Automatic);
}

void ProxyReaderTests::readsDesktopModesOnly()
{
    QFETCH(QByteArray, output);
    QFETCH(int, mode);
    int calls = 0;
    QStringList arguments;
    int timeout = 0;
    ProxyReader reader([&](const QStringList &args, int limit) {
        ++calls;
        arguments = args;
        timeout = limit;
        return ProxyCommandResult{ProxyCommandResult::Status::Success, output};
    }, emptyEnvironment());
    const auto info = reader.read();
    QCOMPARE(int(info.mode), mode);
    QCOMPARE(calls, 1);
    QCOMPARE(arguments, (QStringList{QStringLiteral("get"), QStringLiteral("org.gnome.system.proxy"),
                                     QStringLiteral("mode")}));
    QCOMPARE(timeout, 800);
    QVERIFY(info.environmentVariables.isEmpty());
    QVERIFY(!info.source.contains(QStringLiteral("apt")));
    QVERIFY(!info.source.contains(QStringLiteral("org.ukui.proxy")));
    if (info.mode == ProxyMode::Automatic)
        QVERIFY(info.detail.contains(QStringLiteral("未读取、下载或执行 PAC")));
    if (info.mode != ProxyMode::Disabled)
        QVERIFY(info.detail.contains(QStringLiteral("不代表所有流量")));
}

void ProxyReaderTests::failuresRemainUnknown_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("output");
    QTest::newRow("missing-command") << int(ProxyCommandResult::Status::Unavailable) << QByteArray();
    QTest::newRow("timeout") << int(ProxyCommandResult::Status::TimedOut) << QByteArray("'none'");
    QTest::newRow("missing-schema") << int(ProxyCommandResult::Status::Failed) << QByteArray("'none'");
    QTest::newRow("unrecognized") << int(ProxyCommandResult::Status::Success) << QByteArray("'other'");
    QTest::newRow("invalid-secret-output") << int(ProxyCommandResult::Status::Success)
        << QByteArray("http://fixture-user:fixture-secret@proxy.invalid:1234");
}

void ProxyReaderTests::failuresRemainUnknown()
{
    QFETCH(int, status);
    QFETCH(QByteArray, output);
    ProxyReader reader([&](const QStringList &, int) {
        return ProxyCommandResult{static_cast<ProxyCommandResult::Status>(status), output};
    }, emptyEnvironment());
    const auto info = reader.read();
    QCOMPARE(int(info.mode), int(ProxyMode::Unknown));
    QVERIFY(info.detail.contains(QStringLiteral("不能据此判定代理已关闭")));
    QVERIFY(!visibleText(info).contains(QStringLiteral("fixture-secret")));
    QVERIFY(!visibleText(info).contains(QStringLiteral("proxy.invalid")));
}

void ProxyReaderTests::environmentExposesNamesWithoutCredentials()
{
    QProcessEnvironment environment;
    const QString value = QStringLiteral("http://fixture-user:fixture-secret@proxy.invalid:1234/private");
    for (const auto &name : {"http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY", "all_proxy", "ALL_PROXY"})
        environment.insert(QString::fromLatin1(name), value);
    environment.insert(QStringLiteral("NO_PROXY"), QStringLiteral("private.invalid"));
    ProxyReader reader([](const QStringList &, int) {
        return ProxyCommandResult{ProxyCommandResult::Status::Success, "'none'"};
    }, [environment]() { return environment; });
    const auto info = reader.read();
    QCOMPARE(int(info.mode), int(ProxyMode::Disabled)); // This is the desktop mode, not every application's route.
    QCOMPARE(info.environmentVariables, (QStringList{QStringLiteral("http_proxy"), QStringLiteral("HTTP_PROXY"),
        QStringLiteral("https_proxy"), QStringLiteral("HTTPS_PROXY"), QStringLiteral("all_proxy"), QStringLiteral("ALL_PROXY")}));
    QVERIFY(info.detail.contains(QStringLiteral("环境变量")));
    QVERIFY(info.detail.contains(QStringLiteral("各应用")));
    for (const auto &secret : {"fixture-user", "fixture-secret", "proxy.invalid", "private.invalid", "/private"})
        QVERIFY(!visibleText(info).contains(QString::fromLatin1(secret)));
}

void ProxyReaderTests::noProxyExceptionsDoNotEnableProxy()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("NO_PROXY"), QStringLiteral("*"));
    environment.insert(QStringLiteral("no_proxy"), QStringLiteral("localhost"));
    environment.insert(QStringLiteral("http_proxy"), QString());
    environment.insert(QStringLiteral("ALL_PROXY"), QStringLiteral("  "));
    ProxyReader reader([](const QStringList &, int) {
        return ProxyCommandResult{ProxyCommandResult::Status::Success, "'none'"};
    }, [environment]() { return environment; });
    const auto info = reader.read();
    QCOMPARE(int(info.mode), int(ProxyMode::Disabled));
    QVERIFY(info.environmentVariables.isEmpty());
}

void ProxyReaderTests::ignoresAptAndUnrelatedProxyEnvironment()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("APT_CONFIG"), QStringLiteral("/some/private/path"));
    environment.insert(QStringLiteral("FTP_PROXY"), QStringLiteral("fixture-secret"));
    environment.insert(QStringLiteral("MY_HTTP_PROXY_BACKUP"), QStringLiteral("fixture-secret"));
    ProxyReader reader([](const QStringList &, int) {
        return ProxyCommandResult{ProxyCommandResult::Status::Unavailable, {}};
    }, [environment]() { return environment; });
    const auto info = reader.read();
    QCOMPARE(int(info.mode), int(ProxyMode::Unknown));
    QVERIFY(info.environmentVariables.isEmpty());
    QVERIFY(!visibleText(info).contains(QStringLiteral("fixture-secret")));
}

void ProxyReaderTests::readReflectsChangedConfiguration()
{
    QByteArray currentMode("'manual'");
    QProcessEnvironment environment;
    ProxyReader reader([&](const QStringList &, int) {
        return ProxyCommandResult{ProxyCommandResult::Status::Success, currentMode};
    }, [&environment]() { return environment; });
    QCOMPARE(int(reader.read().mode), int(ProxyMode::Manual));
    currentMode = "'auto'";
    environment.insert(QStringLiteral("https_proxy"), QStringLiteral("private-value"));
    const auto updated = reader.read();
    QCOMPARE(int(updated.mode), int(ProxyMode::Automatic));
    QCOMPARE(updated.environmentVariables, QStringList{QStringLiteral("https_proxy")});
    QVERIFY(!visibleText(updated).contains(QStringLiteral("private-value")));
}

QTEST_GUILESS_MAIN(ProxyReaderTests)
#include "proxy_reader_tests.moc"

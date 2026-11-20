// SPDX-License-Identifier: GPL-3.0-or-later
#include "backend/proxy_reader.h"
#include <QElapsedTimer>
#include <QProcess>
#include <utility>

namespace monitor {

ProxyReader::ProxyReader(CommandRunner runner, EnvironmentProvider environment,
                         int timeoutMilliseconds)
    : runner_(runner ? std::move(runner) : CommandRunner(runCommand)),
      environment_(environment ? std::move(environment)
                               : EnvironmentProvider(QProcessEnvironment::systemEnvironment)),
      timeoutMilliseconds_(qBound(50, timeoutMilliseconds, 2000))
{
}

ProxyCommandResult ProxyReader::runCommand(const QStringList &arguments, int timeoutMilliseconds)
{
    QElapsedTimer elapsed;
    elapsed.start();
    QProcess process;
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(QStringLiteral("gsettings"), arguments, QIODevice::ReadOnly);
    if (!process.waitForStarted(timeoutMilliseconds)) {
        const bool timedOut = process.error() == QProcess::Timedout;
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(150);
        }
        return {timedOut ? ProxyCommandResult::Status::TimedOut
                         : ProxyCommandResult::Status::Unavailable, {}};
    }
    const int remaining = qMax(1, timeoutMilliseconds - int(elapsed.elapsed()));
    if (!process.waitForFinished(remaining)) {
        process.kill();
        process.waitForFinished(150);
        return {ProxyCommandResult::Status::TimedOut, {}};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {ProxyCommandResult::Status::Failed, {}};
    // Only a mode enum is requested. Never query hosts, authentication, or PAC URLs.
    const QByteArray output = process.readAllStandardOutput();
    if (output.size() > 64)
        return {ProxyCommandResult::Status::Failed, {}};
    return {ProxyCommandResult::Status::Success, output};
}

QStringList ProxyReader::environmentVariableNames(const QProcessEnvironment &environment)
{
    QStringList names;
    for (const auto &name : {"http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY", "all_proxy", "ALL_PROXY"}) {
        const QString variable = QString::fromLatin1(name);
        if (!environment.value(variable).trimmed().isEmpty())
            names.append(variable);
    }
    // NO_PROXY/no_proxy describe exceptions; they do not enable a proxy.
    return names;
}

ProxyInfo ProxyReader::read() const
{
    ProxyInfo info;
    info.source = QStringLiteral("GSettings · org.gnome.system.proxy");
    info.environmentVariables = environmentVariableNames(environment_());
    const ProxyCommandResult result = runner_({QStringLiteral("get"),
        QStringLiteral("org.gnome.system.proxy"), QStringLiteral("mode")}, timeoutMilliseconds_);
    if (result.status == ProxyCommandResult::Status::Success) {
        const QByteArray mode = result.output.trimmed();
        if (mode == "'none'") {
            info.mode = ProxyMode::Disabled;
            info.detail = QStringLiteral("桌面代理配置为关闭；应用仍可使用各自的代理设置。");
        } else if (mode == "'manual'") {
            info.mode = ProxyMode::Manual;
            info.detail = QStringLiteral("桌面已配置手动代理；配置启用不代表所有流量实际经过代理。");
        } else if (mode == "'auto'") {
            info.mode = ProxyMode::Automatic;
            info.detail = QStringLiteral("桌面已配置自动代理（PAC）；未读取、下载或执行 PAC，配置启用不代表所有流量实际经过代理。");
        } else {
            info.detail = QStringLiteral("桌面代理配置返回未知模式，不能据此判定代理已关闭。");
        }
    } else if (result.status == ProxyCommandResult::Status::TimedOut) {
        info.detail = QStringLiteral("读取桌面代理配置超时，不能据此判定代理已关闭。");
    } else if (result.status == ProxyCommandResult::Status::Unavailable) {
        info.detail = QStringLiteral("桌面代理查询工具不可用，不能据此判定代理已关闭。");
    } else {
        info.detail = QStringLiteral("无法读取当前用户的桌面代理配置，不能据此判定代理已关闭。");
    }
    if (!info.environmentVariables.isEmpty()) {
        info.source += QStringLiteral("；本进程环境");
        info.detail += QStringLiteral(" 检测到本程序继承的代理环境变量（仅显示名称），其生效范围由各应用决定。");
    }
    return info;
}

} // namespace monitor

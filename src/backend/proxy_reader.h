// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "domain/network_types.h"
#include <QProcessEnvironment>
#include <functional>

namespace monitor {

struct ProxyCommandResult {
    enum class Status { Success, Unavailable, Failed, TimedOut };
    Status status = Status::Unavailable;
    QByteArray output;
};

class ProxyReader final {
public:
    using CommandRunner = std::function<ProxyCommandResult(const QStringList &, int)>;
    using EnvironmentProvider = std::function<QProcessEnvironment()>;

    explicit ProxyReader(CommandRunner runner = {}, EnvironmentProvider environment = {},
                         int timeoutMilliseconds = 800);
    ProxyInfo read() const;

private:
    static ProxyCommandResult runCommand(const QStringList &arguments, int timeoutMilliseconds);
    static QStringList environmentVariableNames(const QProcessEnvironment &environment);
    CommandRunner runner_;
    EnvironmentProvider environment_;
    int timeoutMilliseconds_;
};

} // namespace monitor

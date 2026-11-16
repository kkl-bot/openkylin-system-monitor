#pragma once
#include <QString>
#include <optional>

namespace monitor::files {
QString readText(const QString &path);
std::optional<quint64> readUnsigned(const QString &path);
QStringList directories(const QString &root, const QString &pattern);
} // namespace monitor::files

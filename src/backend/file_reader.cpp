#include "backend/file_reader.h"
#include <QDir>
#include <QFile>

namespace monitor::files {
QString readText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

std::optional<quint64> readUnsigned(const QString &path)
{
    const auto text = readText(path);
    bool ok = false;
    const auto value = text.toULongLong(&ok);
    return ok && !text.startsWith('-') ? std::optional<quint64>(value) : std::nullopt;
}

QStringList directories(const QString &root, const QString &pattern)
{
    QStringList result;
    const QDir dir(root);
    for (const auto &name : dir.entryList({pattern}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        result.append(dir.filePath(name));
    return result;
}
} // namespace monitor::files

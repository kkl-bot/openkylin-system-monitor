#include "backend/process_collector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cerrno>
#include <limits>
#include <utility>

#ifdef Q_OS_LINUX
#include <pwd.h>
#include <unistd.h>
#endif

namespace monitor {
namespace {

struct ParsedProcess {
    ProcessInfo info;
    quint64 cpuTicks = 0;
    std::optional<quint64> readBytes;
    std::optional<quint64> writeBytes;
};

std::optional<QByteArray> readFile(const QString &path, qint64 limit = 65536)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    const auto data = file.read(limit);
    if (file.error() != QFileDevice::NoError) return std::nullopt;
    return data;
}

std::optional<quint64> unsignedNumber(const QByteArray &text)
{
    if (text.isEmpty() || text.front() == '-' || text.front() == '+') return std::nullopt;
    bool ok = false;
    const auto value = text.toULongLong(&ok);
    return ok ? std::optional<quint64>(value) : std::nullopt;
}

std::optional<ParsedProcess> parseStat(const QByteArray &data, qint64 expectedPid)
{
    // comm can contain spaces, newlines and parentheses. Only its final ')' ends field 2.
    const auto open = data.indexOf('(');
    const auto close = data.lastIndexOf(')');
    if (open <= 0 || close <= open || close + 2 >= data.size()) return std::nullopt;
    bool pidOk = false;
    const auto pid = data.left(open).trimmed().toLongLong(&pidOk);
    if (!pidOk || pid != expectedPid || pid <= 0) return std::nullopt;
    const auto fields = data.mid(close + 1).simplified().split(' ');
    if (fields.size() < 22 || fields[0].size() != 1) return std::nullopt;
    const auto parent = unsignedNumber(fields[1]);
    const auto userTicks = unsignedNumber(fields[11]);
    const auto systemTicks = unsignedNumber(fields[12]);
    const auto threads = unsignedNumber(fields[17]);
    const auto start = unsignedNumber(fields[19]);
    const auto rss = unsignedNumber(fields[21]);
    if (!parent || !userTicks || !systemTicks || !threads || !start || !rss
        || *parent > quint64(std::numeric_limits<qint64>::max())
        || *threads > quint64(std::numeric_limits<int>::max())
        || *userTicks > std::numeric_limits<quint64>::max() - *systemTicks) return std::nullopt;
#ifdef Q_OS_LINUX
    const auto pageSize = ::sysconf(_SC_PAGESIZE);
    if (pageSize <= 0 || *rss > std::numeric_limits<quint64>::max() / quint64(pageSize)) return std::nullopt;
#else
    return std::nullopt;
#endif
    ParsedProcess process;
    process.info.identity.pid = pid;
    process.info.identity.startTimeTicks = *start;
    process.info.identity.uid = std::numeric_limits<quint32>::max();
    process.info.parentPid = qint64(*parent);
    process.info.name = QString::fromLocal8Bit(data.mid(open + 1, close - open - 1));
    process.info.state = QString::fromLatin1(fields[0]);
    process.info.threads = int(*threads);
#ifdef Q_OS_LINUX
    process.info.residentBytes = *rss * quint64(pageSize);
#endif
    process.cpuTicks = *userTicks + *systemTicks;
    return process;
}

std::optional<std::array<quint32, 4>> readUids(const QByteArray &status)
{
    for (const auto &line : status.split('\n')) {
        if (!line.startsWith("Uid:")) continue;
        const auto fields = line.mid(4).simplified().split(' ');
        if (fields.size() != 4) return std::nullopt;
        std::array<quint32, 4> result{};
        for (int i = 0; i < 4; ++i) {
            const auto uid = unsignedNumber(fields[i]);
            if (!uid || *uid > std::numeric_limits<quint32>::max()) return std::nullopt;
            result[i] = quint32(*uid);
        }
        return result;
    }
    return std::nullopt;
}

std::optional<quint64> namedCounter(const QByteArray &data, const QByteArray &name)
{
    for (const auto &line : data.split('\n')) {
        const auto colon = line.indexOf(':');
        if (colon >= 0 && line.left(colon) == name) return unsignedNumber(line.mid(colon + 1).trimmed());
    }
    return std::nullopt;
}

std::optional<ParsedProcess> readProcess(const QString &directory, qint64 pid)
{
    const auto stat = readFile(directory + "/stat");
    if (!stat) return std::nullopt;
    auto process = parseStat(*stat, pid);
    if (!process) return std::nullopt;
    const auto status = readFile(directory + "/status");
    const auto uids = status ? readUids(*status) : std::nullopt;
    if (uids) {
        process->info.identity.uid = (*uids)[0];
        process->info.user = QString::number((*uids)[0]);
#ifdef Q_OS_LINUX
        const auto currentUid = quint32(::geteuid());
        process->info.canTerminate = currentUid != 0 && pid > 1 && pid != qint64(::getpid())
            && process->info.state != "Z" && process->info.state != "X" && process->info.state != "x"
            && std::all_of(uids->begin(), uids->end(), [currentUid](quint32 uid) { return uid == currentUid; });
#endif
    } else {
        process->info.user = QStringLiteral("未知");
    }
    if (const auto command = readFile(directory + "/cmdline")) {
        auto display = *command;
        display.replace('\0', ' ');
        process->info.commandLine = QString::fromLocal8Bit(display.trimmed());
    }
    process->info.executable = QFileInfo(directory + "/exe").symLinkTarget();
    if (const auto io = readFile(directory + "/io")) {
        process->readBytes = namedCounter(*io, "read_bytes");
        process->writeBytes = namedCounter(*io, "write_bytes");
    }
    // A process can exit and its PID can be reused between individual proc reads.
    const auto finalStat = readFile(directory + "/stat");
    const auto finalProcess = finalStat ? parseStat(*finalStat, pid) : std::nullopt;
    if (!finalProcess || finalProcess->info.identity.startTimeTicks != process->info.identity.startTimeTicks)
        return std::nullopt;
    return process;
}

std::optional<std::array<quint64, 8>> systemTicks(const QString &root)
{
    const auto data = readFile(root + "/stat");
    if (!data) return std::nullopt;
    const auto fields = data->split('\n').value(0).simplified().split(' ');
    if (fields.size() < 9 || fields[0] != "cpu") return std::nullopt;
    std::array<quint64, 8> result{};
    for (int i = 0; i < 8; ++i) {
        const auto value = unsignedNumber(fields[i + 1]);
        if (!value) return std::nullopt;
        result[i] = *value; // guest and guest_nice are already counted in user and nice.
    }
    return result;
}

std::optional<quint64> totalMemory(const QString &root)
{
    const auto data = readFile(root + "/meminfo");
    if (!data) return std::nullopt;
    for (const auto &line : data->split('\n')) {
        const auto fields = line.simplified().split(' ');
        if (fields.size() != 3 || fields[0] != "MemTotal:" || fields[2] != "kB") continue;
        const auto value = unsignedNumber(fields[1]);
        if (!value || *value == 0 || *value > std::numeric_limits<quint64>::max() / 1024)
            return std::nullopt;
        return *value * 1024;
    }
    return std::nullopt;
}

std::optional<double> rate(const std::optional<quint64> &current,
                           const std::optional<quint64> &previous, qint64 milliseconds)
{
    if (!current || !previous || *current < *previous || milliseconds <= 0) return std::nullopt;
    return double(*current - *previous) * 1000.0 / double(milliseconds);
}

} // namespace

ProcessCollector::ProcessCollector(const QString &procRoot) : procRoot_(procRoot) {}

std::optional<ProcessInfo> ProcessCollector::inspect(const QString &processDirectory, qint64 expectedPid)
{
    const auto process = readProcess(processDirectory, expectedPid);
    return process ? std::optional<ProcessInfo>(process->info) : std::nullopt;
}

QString ProcessCollector::userName(quint32 uid)
{
    const auto cached = userNames_.constFind(uid);
    if (cached != userNames_.cend()) return *cached;
    QString result = QString::number(uid);
#ifdef Q_OS_LINUX
    QByteArray buffer(4096, '\0');
    struct passwd entry {};
    struct passwd *found = nullptr;
    int error = 0;
    do {
        error = ::getpwuid_r(uid_t(uid), &entry, buffer.data(), size_t(buffer.size()), &found);
        if (error == ERANGE) buffer.resize(buffer.size() * 2);
    } while (error == ERANGE && buffer.size() <= 1024 * 1024);
    if (error == 0 && found && found->pw_name) result = QString::fromLocal8Bit(found->pw_name);
#endif
    userNames_.insert(uid, result);
    return result;
}

ProcessSnapshot ProcessCollector::collect(qint64 monotonicMilliseconds)
{
    ProcessSnapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTime();
#ifndef Q_OS_LINUX
    Q_UNUSED(monotonicMilliseconds)
    snapshot.error = QStringLiteral("进程监测仅支持 Linux /proc 接口");
    return snapshot;
#else
    const QDir root(procRoot_);
    if (!root.exists() || !QFileInfo(procRoot_).isReadable()) {
        previous_.clear();
        previousSystem_.reset();
        previousMilliseconds_.reset();
        snapshot.error = QStringLiteral("无法读取进程目录：%1").arg(procRoot_);
        return snapshot;
    }
    const auto currentSystem = systemTicks(procRoot_);
    const auto memory = totalMemory(procRoot_);
    std::optional<quint64> totalDelta;
    if (currentSystem && previousSystem_ && previousMilliseconds_
        && monotonicMilliseconds > *previousMilliseconds_) {
        quint64 sum = 0;
        bool valid = true;
        for (int i = 0; i < 8; ++i) {
            // Linux allows iowait to decrease; treat only that component's delta as zero.
            if ((*currentSystem)[i] < (*previousSystem_)[i] && i != 4) valid = false;
            const auto delta = (*currentSystem)[i] >= (*previousSystem_)[i]
                ? (*currentSystem)[i] - (*previousSystem_)[i] : 0;
            if (delta > std::numeric_limits<quint64>::max() - sum) valid = false;
            else sum += delta;
        }
        if (valid && sum > 0) totalDelta = sum;
    }
    if (!currentSystem) snapshot.error = QStringLiteral("系统 CPU 计数不可用，暂不显示进程 CPU 占用");
    if (!memory) {
        if (!snapshot.error.isEmpty()) snapshot.error += QStringLiteral("；");
        snapshot.error += QStringLiteral("内存总量不可用，暂不显示内存占比");
    }
    QHash<qint64, PreviousSample> next;
    for (const auto &directory : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        bool validPid = false;
        const auto pid = directory.toLongLong(&validPid);
        if (!validPid || pid <= 0 || directory != QString::number(pid)) continue;
        auto process = readProcess(root.filePath(directory), pid);
        if (!process) continue; // Exited or inaccessible processes must not leave stale rows.
        auto &info = process->info;
        if (info.identity.uid != std::numeric_limits<quint32>::max()) info.user = userName(info.identity.uid);
        if (memory) info.memoryPercent = std::clamp(100.0 * double(info.residentBytes) / double(*memory), 0.0, 100.0);
        const auto old = previous_.constFind(pid);
        if (old != previous_.cend() && old->identity == info.identity) {
            if (totalDelta && process->cpuTicks >= old->cpuTicks)
                info.cpuPercent = std::clamp(100.0 * double(process->cpuTicks - old->cpuTicks) / double(*totalDelta), 0.0, 100.0);
            const auto interval = monotonicMilliseconds > old->milliseconds
                ? monotonicMilliseconds - old->milliseconds : 0;
            info.readBytesPerSecond = rate(process->readBytes, old->readBytes, interval);
            info.writeBytesPerSecond = rate(process->writeBytes, old->writeBytes, interval);
        }
        next.insert(pid, {info.identity, process->cpuTicks, process->readBytes, process->writeBytes, monotonicMilliseconds});
        snapshot.processes.append(std::move(info));
    }
    previous_ = std::move(next);
    previousSystem_ = currentSystem;
    previousMilliseconds_ = monotonicMilliseconds;
    std::sort(snapshot.processes.begin(), snapshot.processes.end(), [](const ProcessInfo &a, const ProcessInfo &b) {
        return a.identity.pid < b.identity.pid;
    });
    return snapshot;
#endif
}

} // namespace monitor

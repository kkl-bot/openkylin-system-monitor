#include "backend/process_control.h"
#include "backend/process_collector.h"

#include <cerrno>
#include <cstring>
#include <limits>

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace monitor {
namespace {

#ifdef Q_OS_LINUX
class FileDescriptor final {
public:
    explicit FileDescriptor(int descriptor);
    ~FileDescriptor();
    int get() const;
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
private:
    int descriptor_;
};

FileDescriptor::FileDescriptor(int descriptor) : descriptor_(descriptor) {}
FileDescriptor::~FileDescriptor() { if (descriptor_ >= 0) ::close(descriptor_); }
int FileDescriptor::get() const { return descriptor_; }

QString systemError(const QString &operation, int error)
{
    if (error == ESRCH || error == ENOENT) return QStringLiteral("进程已经退出，请刷新列表");
    if (error == EPERM || error == EACCES) return QStringLiteral("没有权限操作此进程");
    if (error == ENOSYS || error == EINVAL) return QStringLiteral("当前内核不支持安全进程操作（pidfd）");
    return QStringLiteral("%1：%2").arg(operation, QString::fromLocal8Bit(std::strerror(error)));
}
#endif

} // namespace

QString ProcessControl::sendSignal(const ProcessIdentity &identity, bool force)
{
#if defined(Q_OS_LINUX) && defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
    if (identity.pid <= 1 || identity.pid > std::numeric_limits<pid_t>::max())
        return QStringLiteral("不能操作系统保留进程或无效进程");
    if (identity.pid == qint64(::getpid())) return QStringLiteral("不能结束任务管理器自身");
    if (identity.uid == 0 || ::geteuid() == 0) return QStringLiteral("不能结束系统用户的进程，请以普通用户运行任务管理器");
    if (identity.uid != quint32(::geteuid())) return QStringLiteral("只能结束当前用户自己的进程");

    // Pin the kernel process object before checking /proc. No PID-based kill fallback is used.
    const FileDescriptor pidfd(int(::syscall(SYS_pidfd_open, pid_t(identity.pid), 0)));
    if (pidfd.get() < 0) return systemError(QStringLiteral("无法确认进程身份"), errno);
    const auto directory = QStringLiteral("/proc/%1").arg(identity.pid).toLocal8Bit();
    const FileDescriptor procfd(::open(directory.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (procfd.get() < 0) return systemError(QStringLiteral("无法读取进程身份"), errno);
    const auto process = ProcessCollector::inspect(QStringLiteral("/proc/self/fd/%1").arg(procfd.get()), identity.pid);
    if (!process) return QStringLiteral("无法确认进程身份，进程可能已经退出");
    if (process->identity != identity) return QStringLiteral("进程身份已变化，请刷新列表后重试");
    if (process->state == "Z" || process->state == "X" || process->state == "x")
        return QStringLiteral("此进程已经退出，正在等待父进程回收");
    if (!process->canTerminate) return QStringLiteral("只能结束当前用户自己的普通进程");
    if (::syscall(SYS_pidfd_send_signal, pidfd.get(), force ? SIGKILL : SIGTERM, nullptr, 0) < 0)
        return systemError(QStringLiteral("发送结束请求失败"), errno);
    return {};
#else
    Q_UNUSED(identity)
    Q_UNUSED(force)
    return QStringLiteral("当前平台不支持安全进程操作（Linux pidfd）");
#endif
}

} // namespace monitor

#include "ui/process_presentation.h"

namespace monitor::ui {
QString processStateLabel(const QString &state)
{
    if (state.size() != 1) return state;
    switch (state.at(0).unicode()) {
    case 'R': return QStringLiteral("运行");
    case 'S': return QStringLiteral("睡眠");
    case 'D': return QStringLiteral("磁盘等待");
    case 'T': return QStringLiteral("已暂停");
    case 't': return QStringLiteral("调试暂停");
    case 'Z': return QStringLiteral("待回收");
    case 'X':
    case 'x': return QStringLiteral("已退出");
    case 'I': return QStringLiteral("空闲");
    default: return state;
    }
}
} // namespace monitor::ui

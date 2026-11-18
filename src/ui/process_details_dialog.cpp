#include "ui/process_details_dialog.h"
#include "ui/process_presentation.h"
#include "domain/metrics.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QGuiApplication>
#include <QScreen>
#include <QShowEvent>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTextOption>
#include <QVBoxLayout>
#include <QWindow>

namespace monitor::ui {
ProcessDetailsDialog::ProcessDetailsDialog(const ProcessInfo &process, const QDateTime &timestamp, QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("processDetailsDialog"));
    setWindowTitle(QStringLiteral("进程详情 · PID %1").arg(process.identity.pid));
    resize(qMax(640, fontMetrics().horizontalAdvance(QLatin1Char('M')) * 64), qMax(480, fontMetrics().height() * 24));
    auto *layout = new QVBoxLayout(this);
    auto *notice = new QLabel(QStringLiteral("此处显示打开时的进程快照，内容不会随列表刷新而切换。"), this);
    notice->setTextFormat(Qt::PlainText); notice->setWordWrap(true);
    notice->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); layout->addWidget(notice);
    auto *details = new QPlainTextEdit(this);
    details->setObjectName(QStringLiteral("processDetailsText"));
    details->setReadOnly(true);
    details->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    details->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    details->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    const auto value = [](const QString &text) { return text.isEmpty() ? QStringLiteral("不可用") : text; };
    const QStringList lines = {
        QStringLiteral("名称：") + value(process.name), QStringLiteral("PID：") + QString::number(process.identity.pid),
        QStringLiteral("父 PID：") + QString::number(process.parentPid),
        QStringLiteral("用户：%1（UID %2）").arg(value(process.user), QString::number(process.identity.uid)),
        QStringLiteral("状态：") + value(processStateLabel(process.state)), QStringLiteral("线程：") + QString::number(process.threads),
        QStringLiteral("驻留内存：") + formatBytes(process.residentBytes),
        QStringLiteral("启动标识：%1 ticks").arg(process.identity.startTimeTicks),
        QStringLiteral("采样时间：") + (timestamp.isValid() ? timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("不可用")),
        QString(), QStringLiteral("可执行文件："), value(process.executable), QString(), QStringLiteral("命令行："), value(process.commandLine)
    };
    details->setPlainText(lines.join('\n'));
    layout->addWidget(details, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); layout->addWidget(buttons);
}
void ProcessDetailsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    const auto *screen = windowHandle() ? windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect available = screen->availableGeometry().adjusted(16, 32, -16, -32);
    if (available.isEmpty()) return;
    resize(size().boundedTo(available.size()));
    const QPoint position = parentWidget() ? parentWidget()->mapToGlobal(parentWidget()->rect().center()) - rect().center()
        : available.center() - rect().center();
    move(qBound(available.left(), position.x(), qMax(available.left(), available.right() - width() + 1)),
         qBound(available.top(), position.y(), qMax(available.top(), available.bottom() - height() + 1)));
}
} // namespace monitor::ui

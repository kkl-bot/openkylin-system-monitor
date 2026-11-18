// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/process_table_view.h"
#include "ui/process_table_model.h"
#include <QEvent>
#include <QFontMetrics>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <algorithm>

namespace monitor::ui {
namespace {
const QByteArray statePrefix("KMPH1:");
}
ProcessTableView::ProcessTableView(QWidget *parent) : QTableView(parent)
{
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    horizontalHeader()->setMinimumSectionSize(32);
    horizontalHeader()->setStretchLastSection(false);
    verticalHeader()->hide();
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(horizontalHeader(), &QHeaderView::sectionResized, this, [this](int column, int, int) {
        if (!updatingMetrics_) { manualColumns_.insert(column); scheduleMetricsUpdate(); }
    });
    connect(horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this]() { scheduleMetricsUpdate(); });
}
void ProcessTableView::setModel(QAbstractItemModel *model)
{
    updatingMetrics_ = true;
    QTableView::setModel(model);
    updatingMetrics_ = false;
    updateMetrics();
}
QByteArray ProcessTableView::savedHeaderState() const
{
    QJsonArray manual;
    QList<int> columns = manualColumns_.values(); std::sort(columns.begin(), columns.end());
    for (int column : columns) manual.append(column);
    const QJsonObject state{{QStringLiteral("header"), QString::fromLatin1(horizontalHeader()->saveState().toBase64())},
        {QStringLiteral("manualColumns"), manual}};
    return statePrefix + QJsonDocument(state).toJson(QJsonDocument::Compact);
}
bool ProcessTableView::restoreSavedHeaderState(const QByteArray &state)
{
    if (state.isEmpty() || state.size() > 65536) return false;
    QByteArray header = state;
    QSet<int> manual;
    if (state.startsWith(statePrefix)) {
        const auto document = QJsonDocument::fromJson(state.mid(statePrefix.size()));
        const auto object = document.object();
        if (!document.isObject() || !object.value(QStringLiteral("header")).isString()
            || !object.value(QStringLiteral("manualColumns")).isArray()) return false;
        header = QByteArray::fromBase64(object.value(QStringLiteral("header")).toString().toLatin1());
        for (const auto value : object.value(QStringLiteral("manualColumns")).toArray()) {
            const int column = value.toInt(-1);
            if (!value.isDouble() || value.toDouble() != column || column < 0 || column >= horizontalHeader()->count()) return false;
            manual.insert(column);
        }
    } else {
        // Older releases stored only Qt's header bytes. Preserve those widths rather than guessing user intent.
        for (int column = 0; column < horizontalHeader()->count(); ++column) manual.insert(column);
    }
    updatingMetrics_ = true;
    const bool restored = horizontalHeader()->restoreState(header);
    if (restored) manualColumns_ = manual;
    updatingMetrics_ = false;
    if (restored) updateMetrics();
    return restored;
}
bool ProcessTableView::event(QEvent *event)
{
    const bool handled = QTableView::event(event);
    if (event->type() == QEvent::Polish || event->type() == QEvent::PolishRequest) scheduleMetricsUpdate();
    return handled;
}
void ProcessTableView::changeEvent(QEvent *event)
{
    QTableView::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange || event->type() == QEvent::StyleChange)
        scheduleMetricsUpdate();
}
bool ProcessTableView::viewportEvent(QEvent *event)
{
    const bool handled = QTableView::viewportEvent(event);
    if (event->type() == QEvent::Resize) scheduleMetricsUpdate();
    return handled;
}
void ProcessTableView::scheduleMetricsUpdate()
{
    if (metricsUpdatePending_ || updatingMetrics_) return;
    metricsUpdatePending_ = true;
    QTimer::singleShot(0, this, [this]() { metricsUpdatePending_ = false; updateMetrics(); });
}
void ProcessTableView::updateMetrics()
{
    if (!model() || updatingMetrics_) return;
    updatingMetrics_ = true;
    ensurePolished();
    horizontalHeader()->ensurePolished();
    const QFontMetrics text(font()), heading(horizontalHeader()->font());
    const int padding = qMax(12, text.height() / 2);
    const int headerMargin = qMax(4, style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, horizontalHeader()));
    const int rowHeight = text.height() + padding;
    verticalHeader()->setMinimumSectionSize(rowHeight);
    verticalHeader()->setDefaultSectionSize(rowHeight);
    horizontalHeader()->setMinimumHeight(heading.height() + headerMargin * 2 + 4);
    setMinimumHeight(rowHeight * 3 + horizontalHeader()->height() + style()->pixelMetric(QStyle::PM_ScrollBarExtent) + 4);

    const QStringList samples = {QStringLiteral("system-monitor-worker"), QStringLiteral("9999999"), QStringLiteral("openkylin-user"),
        QStringLiteral("调试暂停"), QStringLiteral("100.0 %"), QStringLiteral("999.9 GiB"), QStringLiteral("100.0 %"),
        QStringLiteral("99999"), QStringLiteral("999.9 MiB/s"), QStringLiteral("999.9 MiB/s")};
    QVector<int> widths(horizontalHeader()->count());
    int total = 0;
    for (int column = 0; column < widths.size(); ++column) {
        const QString label = model()->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
        const int headingWidth = qMax(horizontalHeader()->sectionSizeHint(column),
            heading.horizontalAdvance(label) + headerMargin * 2 + heading.height());
        const int contentWidth = text.horizontalAdvance(samples.value(column, label)) + padding;
        widths[column] = manualColumns_.contains(column) ? columnWidth(column)
            : qMax(horizontalHeader()->minimumSectionSize(), qMax(headingWidth, contentWidth));
        if (!isColumnHidden(column)) total += widths[column];
    }
    // Give spare space to text columns; a narrow viewport scrolls instead of crushing numeric values.
    QVector<int> flexible;
    for (int column : {ProcessTableModel::Name, ProcessTableModel::User})
        if (column < widths.size() && !manualColumns_.contains(column) && !isColumnHidden(column)) flexible.append(column);
    int extra = qMax(0, viewport()->width() - total);
    for (int i = 0; i < flexible.size(); ++i) {
        const int share = extra / (flexible.size() - i); widths[flexible[i]] += share; extra -= share;
    }
    for (int column = 0; column < widths.size(); ++column)
        if (!manualColumns_.contains(column) && columnWidth(column) != widths[column]) setColumnWidth(column, widths[column]);
    updatingMetrics_ = false;
}
} // namespace monitor::ui

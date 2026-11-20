// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/resource_tile.h"
#include "ui/paint_palette.h"
#include "ui/resource_sparkline.h"
#include "ui/theme.h"
#include "ui/typography.h"
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QTextDocument>
#include <QTextLayout>
#include <QtMath>

namespace monitor::ui {
namespace {

struct TileContent {
    QString title;
    QString summary;
    QString tooltip;
};

QString capacitySummary(const ResourceSample &sample)
{
    QString used = sample.usedBytes ? formatBytes(*sample.usedBytes) : QStringLiteral("—");
    const QString total = sample.totalBytes ? formatBytes(*sample.totalBytes) : QStringLiteral("—");
    const int unit = used.lastIndexOf(QLatin1Char(' '));
    if (unit >= 0 && total.endsWith(used.mid(unit)))
        used.truncate(unit); // One shared unit keeps a real used/total pair compact.
    return used + QStringLiteral(" / ") + total;
}

TileContent contentFor(const MonitorModel &model, const QString &id)
{
    const auto *sample = model.resource(id);
    if (!sample)
        return {id, QStringLiteral("等待采样"), QStringLiteral("等待资源采样")};
    TileContent content;
    content.title = model.resourceTitle(*sample);
    const QString utilization = formatMetric(sample->utilization, QStringLiteral("%"));
    if (sample->kind == ResourceKind::Memory && (sample->usedBytes || sample->totalBytes)) {
        content.summary = capacitySummary(*sample) + QStringLiteral(" (") + utilization + QLatin1Char(')');
    } else {
        content.summary = utilization;
        if (sample->kind == ResourceKind::Cpu) {
            const QString frequency = sample->details.value(QStringLiteral("平均频率")).simplified();
            bool measured = false;
            const double value = frequency.section(QLatin1Char(' '), 0, 0).toDouble(&measured);
            if (measured && qIsFinite(value) && value > 0
                && (frequency.endsWith(QStringLiteral(" GHz")) || frequency.endsWith(QStringLiteral(" MHz"))))
                content.summary += QStringLiteral(" · ") + frequency;
        }
        if (sample->temperature.value)
            content.summary += QStringLiteral(" · ") + formatMetric(sample->temperature, QStringLiteral(" °C"));
    }
    QStringList information{content.title, sample->name, content.summary,
        QStringLiteral("利用率：") + utilization,
        QStringLiteral("温度：") + formatMetric(sample->temperature, QStringLiteral(" °C")),
        QStringLiteral("小图显示最近 60 秒利用率；无读数或采样间断时折线断开。")};
    if (!sample->utilization.value && !sample->utilization.reason.isEmpty())
        information.append(sample->utilization.reason);
    if (!sample->temperature.value && !sample->temperature.reason.isEmpty())
        information.append(sample->temperature.reason);
    content.tooltip = information.join(QLatin1Char('\n'));
    return content;
}

qreal layoutSummary(QTextLayout &text, qreal width)
{
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    text.setTextOption(option);
    text.beginLayout();
    qreal height = 0;
    for (;;) {
        QTextLine line = text.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(qMax<qreal>(1, width));
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    text.endLayout();
    return height;
}

struct TileLayout {
    QFont titleFont;
    QFont summaryFont;
    QRectF sparkline;
    QRectF title;
    QRectF summary;
    int height = 0;
    int preferredWidth = 0;
    int minimumWidth = 0;
};

TileLayout measureTile(const QFont &font, const QPaintDevice *device, int width, const TileContent &content)
{
    TileLayout layout;
    layout.titleFont = typographyFont(font, TextRole::Section);
    layout.summaryFont = typographyFont(font, TextRole::Body);
    const QFontMetrics titleMetrics(layout.titleFont, device);
    const QFontMetrics metrics(layout.summaryFont, device);
    const int padding = qMax(8, metrics.height() / 3);
    const int gap = qMax(8, metrics.height() / 3);
    const int lineGap = qMax(2, metrics.height() / 10);
    const int preferredGraphWidth = qBound(52, metrics.height() * 2 + 8, 68);
    const int minimumGraphWidth = 44;
    const int graphHeight = qMax(38, qRound(metrics.height() * 1.65));
    const int minimumTextWidth = qMax(titleMetrics.horizontalAdvance(content.title),
                                      metrics.horizontalAdvance(QStringLiteral("100%"))) + 2;
    layout.minimumWidth = padding * 2 + minimumGraphWidth + gap + minimumTextWidth;
    layout.preferredWidth = qMax(225, padding * 2 + preferredGraphWidth + gap
        + qMax(minimumTextWidth, metrics.horizontalAdvance(QStringLiteral("8.0 / 16.0 GiB"))));

    const int available = qMax(1, width - padding * 2);
    int graphWidth = preferredGraphWidth;
    if (available < graphWidth + gap + minimumTextWidth)
        graphWidth = minimumGraphWidth;
    if (available < graphWidth + gap + minimumTextWidth)
        graphWidth = 0; // A forced extra-narrow row keeps readable text, without shrinking it.
    const int textLeft = padding + (graphWidth ? graphWidth + gap : 0);
    const int textWidth = qMax(1, width - textLeft - padding);
    QTextLayout summary(content.summary, layout.summaryFont, device);
    const int summaryHeight = qCeil(layoutSummary(summary, textWidth));
    const int textHeight = titleMetrics.height() + lineGap + summaryHeight;
    const int bodyHeight = qMax(textHeight, graphWidth ? graphHeight : 0);
    const qreal textTop = padding + (bodyHeight - textHeight) / 2.0;
    layout.title = QRectF(textLeft, textTop, textWidth, titleMetrics.height());
    layout.summary = QRectF(textLeft, textTop + titleMetrics.height() + lineGap, textWidth, summaryHeight);
    if (graphWidth)
        layout.sparkline = QRectF(padding, padding + (bodyHeight - graphHeight) / 2.0, graphWidth, graphHeight);
    layout.height = padding * 2 + bodyHeight;
    return layout;
}

} // namespace

ResourceTile::ResourceTile(MonitorModel *model, const QString &id, QWidget *parent)
    : QPushButton(parent), model_(model), id_(id)
{
    setCheckable(true);
    setAutoExclusive(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_StyledBackground, false);
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(id);
    connect(model, &MonitorModel::changed, this, &ResourceTile::refresh);
    refresh();
}

QString ResourceTile::resourceId() const { return id_; }

QSize ResourceTile::sizeHint() const
{
    const auto content = contentFor(*model_, id_);
    const auto layout = measureTile(font(), this, width(), content);
    return {layout.preferredWidth, heightForWidth(layout.preferredWidth)};
}

QSize ResourceTile::minimumSizeHint() const
{
    const auto content = contentFor(*model_, id_);
    const auto layout = measureTile(font(), this, width(), content);
    return {layout.minimumWidth, heightForWidth(layout.preferredWidth)};
}

int ResourceTile::heightForWidth(int width) const
{
    return measureTile(font(), this, width, contentFor(*model_, id_)).height;
}

void ResourceTile::refresh()
{
    const auto content = contentFor(*model_, id_);
    const auto *sample = model_->resource(id_);
    setAccessibleName(content.title + (sample ? QLatin1Char(' ') + sample->name : QString()));
    setAccessibleDescription(content.tooltip);
    setToolTip(Qt::convertFromPlainText(content.tooltip));
    updateGeometry();
    update();
}

void ResourceTile::changeEvent(QEvent *event)
{
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange
        || event->type() == QEvent::StyleChange) {
        updateGeometry();
        update();
    }
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) update();
}

bool ResourceTile::event(QEvent *event)
{
    const bool handled = QPushButton::event(event);
    if (event->type() == QEvent::Polish || event->type() == QEvent::StyleChange) {
        // The row paints its own states; generic QPushButton styling must not fill it first.
        setAutoFillBackground(false);
        setAttribute(Qt::WA_StyledBackground, false);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
    return handled;
}

void ResourceTile::paintEvent(QPaintEvent *)
{
    const auto content = contentFor(*model_, id_);
    const auto layout = measureTile(font(), this, width(), content);
    const auto *sample = model_->resource(id_);
    const QPalette rowPalette = parentWidget() ? parentWidget()->palette() : palette();
    const auto color = sample ? resourceColor(sample->kind) : rowPalette.color(QPalette::Highlight);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (isChecked() || underMouse() || isDown()) {
        const QColor surface = rowPalette.color(isChecked() ? QPalette::Base : QPalette::AlternateBase);
        const QColor accent = rowPalette.color(QPalette::Highlight);
        painter.setPen(isChecked() ? QPen(blendPaintColor(surface, accent, 0.22), 1) : QPen(Qt::NoPen));
        painter.setBrush(blendPaintColor(surface, accent, isDown() ? 0.15 : isChecked() ? 0.07 : 0.06));
        painter.drawRoundedRect(bounds, 7, 7);
    }
    if (isChecked()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(chartAccentColor(color, rowPalette));
        painter.drawRoundedRect(QRectF(2, 7, 3, qMax(0, height() - 14)), 1.5, 1.5);
    }
    painter.setFont(layout.summaryFont);
    paintResourceSparkline(painter, layout.sparkline, model_->history(id_), model_->snapshot().timestamp, color, rowPalette);
    const QColor textColor = rowPalette.color(QPalette::WindowText);
    painter.setPen(textColor);
    painter.setFont(layout.titleFont);
    painter.drawText(layout.title, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
        painter.fontMetrics().elidedText(content.title, Qt::ElideRight, qFloor(layout.title.width())));
    painter.setFont(layout.summaryFont);
    painter.setPen(secondaryPaintText(rowPalette));
    QTextLayout summary(content.summary, layout.summaryFont, this);
    layoutSummary(summary, layout.summary.width());
    painter.save();
    painter.setClipRect(layout.summary);
    summary.draw(&painter, layout.summary.topLeft());
    painter.restore();
    if (hasFocus()) {
        painter.setPen(QPen(rowPalette.color(QPalette::Highlight), 1, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(bounds.adjusted(3, 3, -3, -3), 3, 3);
    }
}

} // namespace monitor::ui

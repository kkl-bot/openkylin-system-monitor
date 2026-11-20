#include "ui/resource_tile.h"
#include "ui/theme.h"
#include "ui/typography.h"
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QTextDocument>

namespace monitor::ui {
namespace {
struct TileLayout {
    QFont titleFont;
    QFont detailFont;
    QFont valueFont;
    QRect title;
    QRect name;
    QRect usage;
    QRect temperature;
    QRect marker;
    int height = 0;
    int preferredWidth = 0;
    int minimumWidth = 0;
};

TileLayout measureTile(const QFont &font, const QPaintDevice *device, int width,
                       const ResourceSample *sample)
{
    TileLayout layout;
    layout.titleFont = typographyFont(font, TextRole::Section);
    layout.detailFont = typographyFont(font, TextRole::Body);
    layout.valueFont = typographyFont(font, TextRole::Metric);
    const QFontMetrics baseMetrics(font, device);
    const QFontMetrics titleMetrics(layout.titleFont, device);
    const QFontMetrics detailMetrics(layout.detailFont, device);
    const QFontMetrics valueMetrics(layout.valueFont, device);
    const int padding = qMax(8, qRound(baseMetrics.height() * 0.65));
    const int gap = qMax(4, baseMetrics.height() / 4);
    const int markerWidth = qMax(3, baseMetrics.height() / 5);
    const int left = padding + markerWidth + gap * 2;
    const QString usage = sample ? formatMetric(sample->utilization, "%") : QStringLiteral("—");
    const QString temperature = sample ? formatMetric(sample->temperature, " °C") : QStringLiteral("—");
    // Reserve normal full-scale values so each new sample does not change the row layout.
    const int usageWidth = qMax(valueMetrics.horizontalAdvance(usage), valueMetrics.horizontalAdvance("100%")) + 2;
    const int temperatureWidth = qMax(detailMetrics.horizontalAdvance(temperature), detailMetrics.horizontalAdvance("−120 °C")) + 2;
    const int valueGap = gap * 2;
    const int titleWidth = titleMetrics.horizontalAdvance(sample ? resourceLabel(sample->kind) : QStringLiteral("CPU"));
    layout.minimumWidth = left + qMax(titleWidth, qMax(usageWidth, temperatureWidth)) + padding;
    layout.preferredWidth = left + qMax(baseMetrics.horizontalAdvance('0') * 13, usageWidth + valueGap + temperatureWidth) + padding;
    const int available = qMax(0, width - left - padding);
    int y = padding;
    layout.title = QRect(left, y, available, titleMetrics.height());
    layout.marker = QRect(padding, y + titleMetrics.height() / 6, markerWidth, qMax(1, titleMetrics.height() * 2 / 3));
    y += titleMetrics.height() + gap;
    layout.name = QRect(left, y, available, detailMetrics.height());
    y += detailMetrics.height() + gap * 2;
    if (available >= usageWidth + valueGap + temperatureWidth) {
        const int rowHeight = qMax(valueMetrics.height(), detailMetrics.height());
        layout.usage = QRect(left, y, available - temperatureWidth - valueGap, rowHeight);
        layout.temperature = QRect(width - padding - temperatureWidth, y, temperatureWidth, rowHeight);
        y += rowHeight;
    } else {
        layout.usage = QRect(left, y, available, valueMetrics.height());
        y += valueMetrics.height() + gap;
        layout.temperature = QRect(left, y, available, detailMetrics.height());
        y += detailMetrics.height();
    }
    layout.height = y + padding;
    return layout;
}
} // namespace

ResourceTile::ResourceTile(MonitorModel *model, const QString &id, QWidget *parent)
    : QPushButton(parent), model_(model), id_(id)
{
    setCheckable(true);
    setAutoExclusive(true);
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
    const auto *sample = model_->resource(id_);
    const auto measured = measureTile(font(), this, width(), sample);
    return {measured.preferredWidth, heightForWidth(measured.preferredWidth)};
}
QSize ResourceTile::minimumSizeHint() const
{
    const auto measured = measureTile(font(), this, width(), model_->resource(id_));
    return {measured.minimumWidth, heightForWidth(measured.preferredWidth)};
}
int ResourceTile::heightForWidth(int width) const
{
    return measureTile(font(), this, width, model_->resource(id_)).height;
}
void ResourceTile::refresh()
{
    const auto *sample = model_->resource(id_);
    if (sample) {
        const QString values = QStringLiteral("利用率：%1；温度：%2")
            .arg(formatMetric(sample->utilization, "%"), formatMetric(sample->temperature, " °C"));
        setAccessibleName(resourceLabel(sample->kind) + ' ' + sample->name);
        setAccessibleDescription(values);
        // Tooltips auto-detect rich text. Escape device-provided names to display them literally.
        setToolTip(Qt::convertFromPlainText(resourceLabel(sample->kind) + '\n' + sample->name + '\n' + values));
    } else {
        setAccessibleName(id_);
        setAccessibleDescription({});
        setToolTip({});
    }
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
}
void ResourceTile::paintEvent(QPaintEvent *)
{
    const auto *sample = model_->resource(id_);
    if (!sample) return;
    const auto layout = measureTile(font(), this, width(), sample);
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const auto color = resourceColor(sample->kind);
    QColor selected = palette().color(QPalette::Highlight); selected.setAlpha(40);
    p.setPen(isChecked() ? QPen(palette().color(QPalette::Highlight)) : Qt::NoPen);
    p.setBrush(isChecked() ? selected : underMouse() ? palette().color(QPalette::Button) : palette().color(QPalette::AlternateBase));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);
    p.setPen(Qt::NoPen); p.setBrush(color); p.drawRoundedRect(layout.marker, 2, 2);
    p.setFont(layout.titleFont);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(layout.title, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
               p.fontMetrics().elidedText(resourceLabel(sample->kind), Qt::ElideRight, layout.title.width()));
    p.setFont(layout.detailFont); p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(layout.name, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
               p.fontMetrics().elidedText(sample->name.simplified(), Qt::ElideRight, layout.name.width()));
    p.setPen(color); p.setFont(layout.valueFont);
    p.drawText(layout.usage, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
               p.fontMetrics().elidedText(formatMetric(sample->utilization, "%"), Qt::ElideRight, layout.usage.width()));
    p.setFont(layout.detailFont); p.setPen(palette().color(QPalette::PlaceholderText));
    p.drawText(layout.temperature, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
               p.fontMetrics().elidedText(formatMetric(sample->temperature, " °C"), Qt::ElideRight, layout.temperature.width()));
    if (hasFocus()) { p.setPen(QPen(color, 1, Qt::DashLine)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(rect().adjusted(3, 3, -3, -3), 7, 7); }
}
} // namespace monitor::ui

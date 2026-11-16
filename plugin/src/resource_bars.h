// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QPointer>
class QQuickWindow;

namespace monitor::widget {
class ResourceBars : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList resources READ resources WRITE setResources NOTIFY resourcesChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool interactive READ interactive WRITE setInteractive NOTIFY interactiveChanged)
public:
    explicit ResourceBars(QQuickItem *parent = nullptr);
    ~ResourceBars() override;
    QVariantList resources() const;
    bool dark() const;
    bool paused() const;
    bool interactive() const;
    void setResources(const QVariantList &value);
    void setDark(bool value);
    void setPaused(bool value);
    void setInteractive(bool value);
    void paint(QPainter *painter) override;
signals:
    void resourcesChanged();
    void darkChanged();
    void pausedChanged();
    void interactiveChanged();
    void doubleClicked();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    QVariantList resources_;
    bool dark_ = false;
    bool paused_ = false;
    bool interactive_ = false;
    QPointer<QQuickWindow> observedWindow_;
};
} // namespace monitor::widget

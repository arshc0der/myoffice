#pragma once
#include <QPropertyAnimation>
#include <QWidget>

#include "core/applicationmanager.h"

// Launcher card: custom painted, animated hover lift, keyboard focusable.
class AppCard : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    explicit AppCard(ApplicationManager::AppId id, QWidget *parent = nullptr);
    qreal hover() const { return m_hover; }
    void setHover(qreal v) { m_hover = v; update(); }
    QSize sizeHint() const override { return { 270, 100 }; }
    QSize minimumSizeHint() const override { return { 220, 100 }; }

signals:
    void activated(ApplicationManager::AppId id);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { animateTo(1.0); }
    void leaveEvent(QEvent *) override { m_pressed = false; animateTo(hasFocus() ? 1.0 : 0.0); }
    void focusInEvent(QFocusEvent *) override { animateTo(1.0); }
    void focusOutEvent(QFocusEvent *) override { animateTo(underMouse() ? 1.0 : 0.0); }
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    void animateTo(qreal target);
    ApplicationManager::AppId m_id;
    qreal m_hover = 0.0;
    bool m_pressed = false;
    QPropertyAnimation m_anim;
};

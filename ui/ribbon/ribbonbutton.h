#pragma once
#include <QAction>
#include <QMenu>
#include <QPropertyAnimation>
#include <QWidget>

// Ribbon command button bound to a QAction. The icon name comes from the action's "icon" property.
class RibbonButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    enum class Size { Large, Small, SmallIcon };
    RibbonButton(QAction *action, Size size, QWidget *parent = nullptr);
    void setMenu(QMenu *menu) { m_menu = menu; updateGeometry(); update(); }
    void setSplit(bool split) { m_split = split; update(); }
    void setIndicatorColor(const QColor &c) { m_indicator = c; update(); }
    qreal hover() const { return m_hover; }
    void setHover(qreal v) { m_hover = v; update(); }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { animateTo(1.0); }
    void leaveEvent(QEvent *) override { m_pressed = false; animateTo(0.0); }
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void animateTo(qreal t);
    bool inArrow(const QPoint &p) const;
    QAction *m_action;
    Size m_size;
    QMenu *m_menu = nullptr;
    bool m_split = false, m_pressed = false, m_pressedArrow = false;
    QColor m_indicator;
    qreal m_hover = 0;
    QPropertyAnimation m_anim;
};

#pragma once
#include <QPropertyAnimation>
#include <QWidget>

// Custom-painted SVG icon button with animated hover, press feedback and keyboard focus.
class IconButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    IconButton(const QString &iconName, const QString &accessibleName, QWidget *parent = nullptr);
    void setIconName(const QString &name) { m_icon = name; update(); }
    void setOnChrome(bool on) { m_onChrome = on; update(); }
    void setIconSize(int px) { m_size = px; update(); }
    void setChecked(bool c) { m_checked = c; update(); }
    qreal hover() const { return m_hover; }
    void setHover(qreal v) { m_hover = v; update(); }
    QSize sizeHint() const override { return { 40, 30 }; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { animateTo(1.0); }
    void leaveEvent(QEvent *) override { m_pressed = false; animateTo(0.0); }
    void focusInEvent(QFocusEvent *) override { update(); }
    void focusOutEvent(QFocusEvent *) override { update(); }
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    void animateTo(qreal target);
    QString m_icon;
    int m_size = 16;
    bool m_onChrome = false, m_checked = false;
    qreal m_hover = 0.0;
    bool m_pressed = false;
    QPropertyAnimation m_anim;
};

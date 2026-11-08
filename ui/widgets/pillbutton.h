#pragma once
#include <QPropertyAnimation>
#include <QWidget>

class PillButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    PillButton(const QString &text, const QString &iconName, bool primary, QWidget *parent = nullptr);
    qreal hover() const { return m_hover; }
    void setHover(qreal v) { m_hover = v; update(); }
    QSize sizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override { animateTo(1.0); }
    void leaveEvent(QEvent *) override { m_pressed = false; animateTo(0.0); }
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void focusInEvent(QFocusEvent *) override { update(); }
    void focusOutEvent(QFocusEvent *) override { update(); }

private:
    void animateTo(qreal t);
    QString m_text, m_icon;
    bool m_primary, m_pressed = false;
    qreal m_hover = 0;
    QPropertyAnimation m_anim;
};

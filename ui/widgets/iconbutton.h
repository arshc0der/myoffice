#pragma once
#include <QPropertyAnimation>
#include <QWidget>

// Custom-painted, keyboard-accessible icon button with animated hover feedback.
class IconButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    enum class Glyph { Minimize, Maximize, Restore, Close, Moon, Sun, Back };

    IconButton(Glyph glyph, const QString &accessibleName, QWidget *parent = nullptr);
    void setGlyph(Glyph glyph);
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
    Glyph m_glyph;
    qreal m_hover = 0.0;
    bool m_pressed = false;
    QPropertyAnimation m_anim;
};

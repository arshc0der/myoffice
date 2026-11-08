#pragma once
#include <QPropertyAnimation>
#include <QWidget>

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal pos READ pos WRITE setPos)
public:
    explicit ToggleSwitch(const QString &accessibleName, QWidget *parent = nullptr);
    bool isChecked() const { return m_checked; }
    void setChecked(bool on, bool animate = true);
    qreal pos() const { return m_pos; }
    void setPos(qreal v) { m_pos = v; update(); }
    QSize sizeHint() const override { return { 42, 24 }; }

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    bool m_checked = false;
    qreal m_pos = 0;
    QPropertyAnimation m_anim;
};

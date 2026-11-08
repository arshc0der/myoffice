#pragma once
#include <QMenu>

class SwatchGrid : public QWidget
{
    Q_OBJECT
public:
    explicit SwatchGrid(QWidget *parent = nullptr);
    QSize sizeHint() const override { return { 10 * 24 + 12, 4 * 24 + 12 }; }

signals:
    void colorChosen(const QColor &c);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { m_hover = -1; update(); }

private:
    QColor colorAt(int idx) const;
    int indexAt(const QPoint &p) const;
    int m_hover = -1;
};

// Drop-down colour palette with "no colour" and "more colours" entries.
class ColorMenu : public QMenu
{
    Q_OBJECT
public:
    explicit ColorMenu(const QString &noColorText, QWidget *parent = nullptr);

signals:
    void colorPicked(const QColor &c);
    void noColorPicked();
};

#pragma once
#include <QVector>
#include <QWidget>

// Left navigation rail of the launcher (Home / Open on top, Settings / About at the bottom).
class NavRail : public QWidget
{
    Q_OBJECT
public:
    explicit NavRail(QWidget *parent = nullptr);
    QSize sizeHint() const override { return { 220, 400 }; }

signals:
    void homeRequested();
    void openRequested();
    void settingsRequested();
    void aboutRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { m_hover = -1; update(); }

private:
    struct Item { QString icon, text; bool bottom; };
    QRectF itemRect(int i) const;
    int indexAt(const QPointF &p) const;
    QVector<Item> m_items;
    int m_hover = -1;
};

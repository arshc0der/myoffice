#pragma once
#include <QVector>
#include <QWidget>

// Browser-style document tabs: close buttons, unsaved dot, drag to reorder, "+" for new.
class DocumentTabBar : public QWidget
{
    Q_OBJECT
public:
    explicit DocumentTabBar(QWidget *parent = nullptr);
    int addTab(const QString &title);
    void removeTab(int index);
    void setTabTitle(int index, const QString &title);
    void setTabModified(int index, bool modified);
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    int count() const { return m_tabs.size(); }

signals:
    void currentChanged(int index);
    void closeRequested(int index);
    void newRequested();
    void tabMoved(int from, int to);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

private:
    struct Tab { QString title; bool modified = false; };
    qreal tabWidth() const;
    QRectF tabRect(int i) const;
    QRectF closeRect(int i) const;
    QRectF plusRect() const;
    int indexAt(const QPointF &p) const;

    QVector<Tab> m_tabs;
    int m_current = -1, m_hover = -1, m_pressed = -1;
    bool m_hoverClose = false, m_hoverPlus = false;
};

#pragma once
#include <QWidget>

class PageView;

// Horizontal ruler above the page canvas. Shows centimetre ticks measured from the left margin,
// shades the margin zones, and follows zoom, scrolling and page setup of the attached PageView.
class Ruler : public QWidget
{
    Q_OBJECT
public:
    explicit Ruler(PageView *view, QWidget *parent = nullptr);
    void refresh() { update(); }   // call after the page setup (margins, size, orientation) changes
    QSize sizeHint() const override { return { 400, 22 }; }

protected:
    void paintEvent(QPaintEvent *) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    PageView *m_view;
};
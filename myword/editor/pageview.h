#pragma once
#include <QGraphicsView>

#include "myword/document/pagesetup.h"

class QGraphicsProxyWidget;
class QTextEdit;

// Paper-style canvas: a QTextEdit embedded in a zoomable QGraphicsView, drawn as a white sheet with
// a soft shadow on a grey desk. The layout is continuous; dashed lines mark approximate page breaks
// (exact pagination happens on PDF export).
class PageView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit PageView(QWidget *parent = nullptr);
    QTextEdit *edit() const { return m_edit; }
    void setPageSetup(const PageSetup &ps);
    const PageSetup &pageSetup() const { return m_ps; }
    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z);
    void fitWidth();
    int pageCount() const;
    int pageOfCursor() const;
    void ensureCursorVisible();
    void focusEditor();

signals:
    void zoomChanged(qreal zoom);

protected:
    void resizeEvent(QResizeEvent *e) override;
    void drawBackground(QPainter *p, const QRectF &rect) override;
    void drawForeground(QPainter *p, const QRectF &rect) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    void relayout();
    QTextEdit *m_edit;
    QGraphicsProxyWidget *m_proxy;
    PageSetup m_ps;
    qreal m_zoom = 1.0;
    QRectF m_sheet;
    bool m_inLayout = false;
};

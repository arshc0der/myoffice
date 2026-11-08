#include "pageview.h"

#include <QAbstractTextDocumentLayout>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPainter>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextEdit>
#include <QWheelEvent>
#include <cmath>

#include "core/thememanager.h"

PageView::PageView(QWidget *parent) : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setFrameShape(QFrame::NoFrame);
    setRenderHint(QPainter::TextAntialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setAccessibleName(tr("Document page"));

    m_edit = new QTextEdit;
    m_edit->setFrameStyle(QFrame::NoFrame);
    m_edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_edit->setAcceptRichText(true);
    QPalette pal = m_edit->palette(); // the page is always white, independent of the UI theme
    for (auto g : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        pal.setColor(g, QPalette::Base, Qt::white);
        pal.setColor(g, QPalette::Window, Qt::white);
        pal.setColor(g, QPalette::Text, QColor("#1B1E27"));
    }
    pal.setColor(QPalette::Highlight, ThemeManager::instance().palette().accent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    m_edit->setPalette(pal);
    m_edit->viewport()->installEventFilter(this);

    m_proxy = scene()->addWidget(m_edit);
    m_proxy->setCacheMode(QGraphicsItem::NoCache);
    m_proxy->setPos(0, 0);

    connect(m_edit->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, [this] { relayout(); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { viewport()->update(); });
    relayout();
}

void PageView::setPageSetup(const PageSetup &ps) { m_ps = ps; relayout(); viewport()->update(); }

void PageView::relayout()
{
    if (m_inLayout) return;
    m_inLayout = true;
    const QSizeF page = m_ps.pageSizePx();
    const QMarginsF m = m_ps.marginsPx();
    m_edit->setContentsMargins(
        qRound(m.left()),
        qRound(m.top()),
        qRound(m.right()),
        qRound(m.bottom())
        );
    const int w = qRound(page.width());
    m_edit->setFixedWidth(w);
    m_edit->document()->setTextWidth(page.width() - m.left() - m.right());
    const qreal docH = m_edit->document()->size().height();
    const int h = qMax(qRound(page.height()), qRound(docH + m.top() + m.bottom()) + 4);
    m_edit->setFixedHeight(h);
    m_proxy->resize(w, h);
    m_sheet = QRectF(0, 0, w, h);
    scene()->setSceneRect(m_sheet.adjusted(-48, -32, 48, 32));
    m_inLayout = false;
}

void PageView::setZoom(qreal z)
{
    z = qBound(0.25, z, 4.0);
    if (qFuzzyCompare(z, m_zoom)) return;
    m_zoom = z;
    resetTransform();
    scale(z, z);
    emit zoomChanged(z);
}

void PageView::fitWidth()
{
    setZoom((viewport()->width() - 64) / m_ps.pageSizePx().width());
}

int PageView::pageCount() const
{
    const qreal ch = m_ps.contentSizePx().height();
    return qMax(1, int(std::ceil(m_edit->document()->size().height() / ch - 0.001)));
}

int PageView::pageOfCursor() const
{
    const qreal y = m_edit->cursorRect().top();
    return qBound(1, int(y / m_ps.contentSizePx().height()) + 1, pageCount());
}

void PageView::ensureCursorVisible()
{
    const QRect cr = m_edit->cursorRect();
    const QPoint tl = m_edit->viewport()->mapTo(m_edit, cr.topLeft());
    ensureVisible(m_proxy->mapToScene(QRectF(tl, QSizeF(cr.size()))).boundingRect(), 24, 80);
}

void PageView::focusEditor()
{
    m_proxy->setFocus();
    m_edit->setFocus();
}

void PageView::resizeEvent(QResizeEvent *e) { QGraphicsView::resizeEvent(e); }

void PageView::drawBackground(QPainter *p, const QRectF &rect)
{
    const Palette &pal = ThemeManager::instance().palette();
    p->fillRect(rect, pal.canvas);
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    for (int i = 10; i >= 1; --i) { // layered rects approximate a blurred shadow without an offscreen pass
        p->setBrush(QColor(0, 0, 0, ThemeManager::instance().isDark() ? 16 : 7));
        p->drawRoundedRect(m_sheet.adjusted(-i, -i + 4, i, i + 4), 3 + i, 3 + i);
    }
    p->setBrush(Qt::white);
    p->drawRect(m_sheet);
}

void PageView::drawForeground(QPainter *p, const QRectF &rect)
{
    const qreal ch = m_ps.contentSizePx().height();
    const QMarginsF m = m_ps.marginsPx();
    p->setPen(QPen(QColor(150, 150, 160, 120), 1, Qt::DashLine));
    p->setFont(QFont(QStringLiteral("Arial"), 7));
    for (int k = 1; k < pageCount(); ++k) {
        const qreal y = m.top() + k * ch;
        if (y < rect.top() - 10 || y > rect.bottom() + 10) continue;
        p->drawLine(QPointF(m_sheet.left() + 8, y), QPointF(m_sheet.right() - 8, y));
        p->drawText(QPointF(m_sheet.right() - 64, y - 3), tr("Page %1").arg(k + 1));
    }
}

bool PageView::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_edit->viewport() && e->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(e);
        if (we->modifiers() & Qt::ControlModifier) setZoom(m_zoom * (we->angleDelta().y() > 0 ? 1.1 : 1 / 1.1));
        else verticalScrollBar()->setValue(verticalScrollBar()->value() - we->angleDelta().y());
        return true;
    }
    return QGraphicsView::eventFilter(o, e);
}

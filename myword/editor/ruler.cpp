#include "ruler.h"

#include <QEvent>
#include <QPainter>
#include <QScrollBar>
#include <QtMath>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "myword/editor/pageview.h"

Ruler::Ruler(PageView *view, QWidget *parent) : QWidget(parent), m_view(view)
{
    setFixedHeight(22);
    setAccessibleName(tr("Ruler"));
    connect(m_view, &PageView::zoomChanged, this, qOverload<>(&QWidget::update));
    connect(m_view->horizontalScrollBar(), &QScrollBar::valueChanged, this, qOverload<>(&QWidget::update));
    connect(m_view->horizontalScrollBar(), &QScrollBar::rangeChanged, this, qOverload<>(&QWidget::update));
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
    m_view->viewport()->installEventFilter(this); // repaint when the view is resized
}

bool Ruler::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_view->viewport() && e->type() == QEvent::Resize) update();
    return QWidget::eventFilter(o, e);
}

void Ruler::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    const PageSetup ps = m_view->pageSetup();
    const qreal z = m_view->zoom();
    const qreal mmPx = 96.0 / 25.4 * z;                       // screen pixels per millimetre
    const qreal originX = m_view->mapFromScene(QPointF(0, 0)).x(); // left edge of the sheet in view coordinates
    const QSizeF page = ps.pageSizePx();
    const QMarginsF m = ps.marginsPx();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), pal.surface);
    p.setPen(pal.border);
    p.drawLine(0, height() - 1, width(), height() - 1);

    // Page band: grey margins, white writable area between them.
    const QRectF band(originX, 4, page.width() * z, height() - 8);
    const QRectF writable(originX + m.left() * z, 4, (page.width() - m.left() - m.right()) * z, height() - 8);
    p.fillRect(band, Design::mix(Qt::white, pal.border, theme.isDark() ? 0.55 : 0.45));
    p.fillRect(writable, Qt::white);

    // Ticks are measured from the left margin: 0 at the margin, positive to the right, positive to the left too.
    const double leftMm = ps.marginLeftMm, rightMm = ps.marginRightMm;
    const double contentMm = (ps.landscape ? ps.heightMm : ps.widthMm) - leftMm - rightMm;
    const qreal x0 = originX + m.left() * z;
    p.setClipRect(band.adjusted(0, -1, 0, 1));
    p.setPen(QColor("#6B7280"));
    p.setFont(theme.uiFont(7));
    for (int mm = -qFloor(leftMm); mm <= qCeil(contentMm + rightMm); ++mm) {
        const qreal x = x0 + mm * mmPx;
        if (x < -4 || x > width() + 4) continue;
        const int a = qAbs(mm);
        if (a % 10 == 0) {
            p.drawLine(QPointF(x, 8), QPointF(x, height() - 8));
            if (a > 0 && (z >= 0.5 || (a / 10) % 2 == 0))
                p.drawText(QRectF(x + 2, 2, 24, height() - 4), Qt::AlignLeft | Qt::AlignVCenter, QString::number(a / 10));
        } else if (a % 5 == 0) {
            p.drawLine(QPointF(x, 10), QPointF(x, height() - 10));
        } else if (z >= 0.6) {
            p.drawLine(QPointF(x, 12), QPointF(x, height() - 12));
        }
    }
    p.setClipping(false);

    // Margin markers (triangles) at the left and right margin.
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(pal.accent);
    const qreal lx = writable.left(), rx = writable.right();
    p.drawPolygon(QPolygonF{ QPointF(lx - 4, height() - 4), QPointF(lx + 4, height() - 4), QPointF(lx, height() - 9) });
    p.drawPolygon(QPolygonF{ QPointF(rx - 4, height() - 4), QPointF(rx + 4, height() - 4), QPointF(rx, height() - 9) });
}
#include "documenttabbar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "core/designtokens.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"

DocumentTabBar::DocumentTabBar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(38);
    setMouseTracking(true);
    setAccessibleName(tr("Open documents"));
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

int DocumentTabBar::addTab(const QString &title)
{
    m_tabs.append({ title, false });
    update();
    return m_tabs.size() - 1;
}

void DocumentTabBar::removeTab(int i)
{
    if (i < 0 || i >= m_tabs.size()) return;
    const int old = m_current;
    m_tabs.removeAt(i);
    if (m_tabs.isEmpty()) m_current = -1;
    else if (i == old) { m_current = qMin(i, m_tabs.size() - 1); update(); emit currentChanged(m_current); return; }
    else if (i < old) --m_current;
    update();
}

void DocumentTabBar::setTabTitle(int i, const QString &t) { if (i >= 0 && i < m_tabs.size()) { m_tabs[i].title = t; update(); } }
void DocumentTabBar::setTabModified(int i, bool m) { if (i >= 0 && i < m_tabs.size()) { m_tabs[i].modified = m; update(); } }

void DocumentTabBar::setCurrentIndex(int i)
{
    if (i < 0 || i >= m_tabs.size() || i == m_current) return;
    m_current = i;
    update();
    emit currentChanged(i);
}

qreal DocumentTabBar::tabWidth() const
{
    const qreal avail = width() - 16 - 44;
    return qBound(110.0, m_tabs.isEmpty() ? 160.0 : avail / m_tabs.size(), 220.0);
}
QRectF DocumentTabBar::tabRect(int i) const { return QRectF(8 + i * (tabWidth() + 2), 6, tabWidth(), height() - 6); }
QRectF DocumentTabBar::closeRect(int i) const { const QRectF t = tabRect(i); return QRectF(t.right() - 28, t.center().y() - 10, 20, 20); }
QRectF DocumentTabBar::plusRect() const { return QRectF(8 + m_tabs.size() * (tabWidth() + 2) + 4, 9, 28, 26); }

int DocumentTabBar::indexAt(const QPointF &p) const
{
    for (int i = 0; i < m_tabs.size(); ++i) if (tabRect(i).contains(p)) return i;
    return -1;
}

void DocumentTabBar::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), pal.canvas);
    const qreal dpr = devicePixelRatioF();

    for (int i = 0; i < m_tabs.size(); ++i) {
        const QRectF r = tabRect(i);
        const bool cur = i == m_current;
        QPainterPath path;
        path.addRoundedRect(r.adjusted(0, 0, 0, 10), 9, 9);
        p.setClipRect(r.adjusted(-1, -1, 1, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(cur ? pal.surface : (i == m_hover ? Design::mix(pal.canvas, pal.surface, 0.55) : Qt::transparent));
        p.drawPath(path);
        if (cur) { p.setBrush(pal.accent); p.drawRoundedRect(QRectF(r.left() + 10, r.top(), r.width() - 20, 3), 1.5, 1.5); }
        p.setClipping(false);

        p.setPen(cur ? pal.text : pal.textSecondary);
        p.setFont(theme.uiFont(Design::Type::body, cur ? QFont::DemiBold : QFont::Normal));
        const QRectF tr(r.left() + 14, r.top(), r.width() - 48, r.height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(m_tabs[i].title, Qt::ElideRight, int(tr.width())));

        const QRectF c = closeRect(i);
        const bool hoverClose = (i == m_hover && m_hoverClose);
        if (m_tabs[i].modified && !hoverClose) {
            p.setPen(Qt::NoPen); p.setBrush(pal.accent); p.drawEllipse(c.center(), 4.5, 4.5);
        } else if (cur || i == m_hover) {
            if (hoverClose) { p.setPen(Qt::NoPen); p.setBrush(pal.surfaceHover); p.drawEllipse(c); }
            p.drawPixmap(QPointF(c.center().x() - 6, c.center().y() - 6), Icons::pixmap(QStringLiteral("close"), pal.text, 12, dpr));
        }
    }
    const QRectF pr = plusRect();
    if (m_hoverPlus) { p.setPen(Qt::NoPen); p.setBrush(pal.surfaceHover); p.drawRoundedRect(pr, 7, 7); }
    p.drawPixmap(QPointF(pr.center().x() - 8, pr.center().y() - 8), Icons::pixmap(QStringLiteral("plus"), pal.text, 16, dpr));
}

void DocumentTabBar::mousePressEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    const int i = indexAt(pos);
    if (e->button() == Qt::MiddleButton && i >= 0) { emit closeRequested(i); return; }
    if (e->button() != Qt::LeftButton) return;
    if (plusRect().contains(pos)) { emit newRequested(); return; }
    if (i < 0) return;
    if (closeRect(i).contains(pos)) { emit closeRequested(i); return; }
    setCurrentIndex(i);
    m_pressed = i;
}

void DocumentTabBar::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    m_hover = indexAt(pos);
    m_hoverClose = m_hover >= 0 && closeRect(m_hover).contains(pos);
    m_hoverPlus = plusRect().contains(pos);
    if ((e->buttons() & Qt::LeftButton) && m_pressed >= 0) {
        const int target = indexAt(QPointF(pos.x(), height() - 4));
        if (target >= 0 && target != m_pressed) {
            m_tabs.move(m_pressed, target);
            const int from = m_pressed;
            m_pressed = m_current = target;
            emit tabMoved(from, target);
        }
    }
    update();
}

void DocumentTabBar::mouseReleaseEvent(QMouseEvent *) { m_pressed = -1; }
void DocumentTabBar::leaveEvent(QEvent *) { m_hover = -1; m_hoverClose = m_hoverPlus = false; update(); }

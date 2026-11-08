#include "iconbutton.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "core/designtokens.h"
#include "core/thememanager.h"

IconButton::IconButton(Glyph glyph, const QString &accessibleName, QWidget *parent)
    : QWidget(parent), m_glyph(glyph), m_anim(this, "hover")
{
    setAccessibleName(accessibleName);
    setToolTip(accessibleName);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void IconButton::setGlyph(Glyph glyph) { m_glyph = glyph; update(); }

void IconButton::animateTo(qreal target)
{
    m_anim.stop();
    m_anim.setDuration(ThemeManager::instance().duration(Design::Duration::fast));
    m_anim.setStartValue(m_hover);
    m_anim.setEndValue(target);
    m_anim.start();
}

void IconButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) { m_pressed = true; update(); }
}

void IconButton::mouseReleaseEvent(QMouseEvent *e)
{
    const bool fire = m_pressed && e->button() == Qt::LeftButton && rect().contains(e->position().toPoint());
    m_pressed = false;
    update();
    if (fire) emit clicked();
}

void IconButton::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) emit clicked();
    else QWidget::keyPressEvent(e);
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &pal = ThemeManager::instance().palette();
    const bool isClose = m_glyph == Glyph::Close;

    const QRectF bg = QRectF(rect()).adjusted(2, 2, -2, -2);
    QColor fill = isClose ? pal.danger : pal.surfaceHover;
    fill.setAlphaF(m_hover * (m_pressed ? 1.0 : 0.9));
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(bg, Design::Radius::sm, Design::Radius::sm);
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(pal.accent, 2));
        p.drawRoundedRect(bg, Design::Radius::sm, Design::Radius::sm);
    }

    const QColor ink = (isClose && m_hover > 0.5) ? QColor(Qt::white) : pal.text;
    const QPointF c = QRectF(rect()).center();
    QPen pen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal s = 5.0;

    switch (m_glyph) {
    case Glyph::Minimize: p.drawLine(c + QPointF(-s, 0), c + QPointF(s, 0)); break;
    case Glyph::Maximize: p.drawRoundedRect(QRectF(c.x() - s, c.y() - s, 2 * s, 2 * s), 1.5, 1.5); break;
    case Glyph::Restore:
        p.drawRoundedRect(QRectF(c.x() - s, c.y() - s + 2, 2 * s - 2, 2 * s - 2), 1.5, 1.5);
        p.drawPolyline(QPolygonF{ c + QPointF(-s + 2, -s + 2), c + QPointF(-s + 2, -s), c + QPointF(s, -s),
                                  c + QPointF(s, s - 4), c + QPointF(s - 2, s - 4) });
        break;
    case Glyph::Close:
        p.drawLine(c + QPointF(-s, -s), c + QPointF(s, s));
        p.drawLine(c + QPointF(-s, s), c + QPointF(s, -s));
        break;
    case Glyph::Back:
        p.drawLine(c + QPointF(s, 0), c + QPointF(-s, 0));
        p.drawPolyline(QPolygonF{ c + QPointF(-s + 4, -4), c + QPointF(-s, 0), c + QPointF(-s + 4, 4) });
        break;
    case Glyph::Moon: {
        QPainterPath a, b;
        a.addEllipse(c, 6.5, 6.5);
        b.addEllipse(c + QPointF(3.5, -2.5), 5.5, 5.5);
        p.setPen(Qt::NoPen);
        p.setBrush(ink);
        p.drawPath(a.subtracted(b));
        break;
    }
    case Glyph::Sun:
        p.drawEllipse(c, 3.5, 3.5);
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(c);
            p.rotate(i * 45.0);
            p.drawLine(QPointF(0, -6), QPointF(0, -8));
            p.restore();
        }
        break;
    }
}

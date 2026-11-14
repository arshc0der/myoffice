#include "iconbutton.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "core/designtokens.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"

IconButton::IconButton(const QString &iconName, const QString &accessibleName, QWidget *parent)
    : QWidget(parent), m_icon(iconName), m_anim(this, "hover")
{
    setAccessibleName(accessibleName);
    setToolTip(accessibleName);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

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
    const bool isClose = m_onChrome && m_icon == QLatin1String("close");
    QColor ink = m_onChrome ? pal.chromeText : pal.text;
    QColor fill = isClose ? pal.danger : (m_onChrome ? ink : pal.surfaceHover);
    const qreal base = isClose ? 0.95 : (m_onChrome ? 0.2 : 1.0);
    fill.setAlphaF(m_hover * base * (m_pressed ? 1.3 : 1.0) > 1 ? 1.0 : m_hover * base * (m_pressed ? 1.3 : 1.0));
    const QRectF bg = QRectF(rect()).adjusted(2, 2, -2, -2);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(bg, Design::Radius::sm, Design::Radius::sm);
    if (m_checked) {
        QColor c = pal.accent;
        c.setAlphaF(0.2);
        p.setBrush(c);
        p.setPen(QPen(pal.accent, 1));
        p.drawRoundedRect(bg, Design::Radius::sm, Design::Radius::sm);
        ink = pal.accentDeep;
    }
    if (hasFocus()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(m_onChrome ? pal.chromeText : pal.accent, 2));
        p.drawRoundedRect(bg, Design::Radius::sm, Design::Radius::sm);
    }
    if (isClose && m_hover > 0.5) ink = Qt::white;
    p.drawPixmap(QPointF((width() - m_size) / 2.0, (height() - m_size) / 2.0),
                 Icons::pixmap(m_icon, ink, m_size, devicePixelRatioF()));
}

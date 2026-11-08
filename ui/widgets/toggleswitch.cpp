#include "toggleswitch.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "core/designtokens.h"
#include "core/thememanager.h"

ToggleSwitch::ToggleSwitch(const QString &accessibleName, QWidget *parent) : QWidget(parent), m_anim(this, "pos")
{
    setAccessibleName(accessibleName);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void ToggleSwitch::setChecked(bool on, bool animate)
{
    if (m_checked == on) return;
    m_checked = on;
    m_anim.stop();
    m_anim.setDuration(animate ? ThemeManager::instance().duration(Design::Duration::fast) : 0);
    m_anim.setStartValue(m_pos);
    m_anim.setEndValue(on ? 1.0 : 0.0);
    m_anim.start();
    emit toggled(on);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent *e)
{
    if (rect().contains(e->position().toPoint())) setChecked(!m_checked);
}

void ToggleSwitch::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return) setChecked(!m_checked);
    else QWidget::keyPressEvent(e);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    const Palette &pal = ThemeManager::instance().palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(1, 3, -1, -3);
    p.setPen(hasFocus() ? QPen(pal.accent, 2) : Qt::NoPen);
    p.setBrush(Design::mix(pal.border, pal.accent, m_pos));
    p.drawRoundedRect(track, track.height() / 2, track.height() / 2);
    const qreal d = track.height() - 4;
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(track.left() + 2 + m_pos * (track.width() - d - 4), track.top() + 2, d, d));
}

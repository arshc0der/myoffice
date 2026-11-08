#include "pillbutton.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "core/designtokens.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"

PillButton::PillButton(const QString &text, const QString &iconName, bool primary, QWidget *parent)
    : QWidget(parent), m_text(text), m_icon(iconName), m_primary(primary), m_anim(this, "hover")
{
    setAccessibleName(text);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

QSize PillButton::sizeHint() const
{
    const QFontMetrics fm(ThemeManager::instance().uiFont(Design::Type::body, QFont::DemiBold));
    return { fm.horizontalAdvance(m_text) + (m_icon.isEmpty() ? 36 : 62), 36 };
}

void PillButton::animateTo(qreal t)
{
    m_anim.stop();
    m_anim.setDuration(ThemeManager::instance().duration(Design::Duration::fast));
    m_anim.setStartValue(m_hover);
    m_anim.setEndValue(t);
    m_anim.start();
}

void PillButton::mousePressEvent(QMouseEvent *e) { if (e->button() == Qt::LeftButton) { m_pressed = true; update(); } }
void PillButton::mouseReleaseEvent(QMouseEvent *e)
{
    const bool fire = m_pressed && rect().contains(e->position().toPoint());
    m_pressed = false;
    update();
    if (fire) emit clicked();
}
void PillButton::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) emit clicked();
    else QWidget::keyPressEvent(e);
}

void PillButton::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const QColor fill = m_primary ? Design::mix(pal.accent, pal.accentDeep, m_hover * (m_pressed ? 1.0 : 0.6))
                                  : Design::mix(pal.surface, pal.surfaceHover, m_hover);
    const QColor ink = m_primary ? QColor(Qt::white) : pal.text;
    p.setBrush(fill);
    p.setPen(m_primary ? Qt::NoPen : QPen(hasFocus() ? pal.accent : pal.border, hasFocus() ? 2 : 1));
    p.drawRoundedRect(r, 18, 18);
    qreal x = 18;
    if (!m_icon.isEmpty()) {
        p.drawPixmap(QPointF(16, (height() - 18) / 2.0), Icons::pixmap(m_icon, ink, 18, devicePixelRatioF()));
        x = 42;
    }
    p.setPen(ink);
    p.setFont(theme.uiFont(Design::Type::body, QFont::DemiBold));
    p.drawText(QRectF(x, 0, width() - x - 12, height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

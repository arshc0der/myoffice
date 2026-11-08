#include "appcard.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include "appglyph.h"
#include "core/designtokens.h"
#include "core/thememanager.h"

AppCard::AppCard(ApplicationManager::AppId id, QWidget *parent)
    : QWidget(parent), m_id(id), m_anim(this, "hover")
{
    const auto &info = ApplicationManager::info(id);
    setAccessibleName(info.name);
    setAccessibleDescription(info.tagline + QStringLiteral(". ") + info.detail);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void AppCard::animateTo(qreal target)
{
    m_anim.stop();
    m_anim.setDuration(ThemeManager::instance().duration(Design::Duration::normal));
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    m_anim.setStartValue(m_hover);
    m_anim.setEndValue(target);
    m_anim.start();
}

void AppCard::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) { m_pressed = true; update(); }
}

void AppCard::mouseReleaseEvent(QMouseEvent *e)
{
    const bool fire = m_pressed && e->button() == Qt::LeftButton && rect().contains(e->position().toPoint());
    m_pressed = false;
    update();
    if (fire) emit activated(m_id);
}

void AppCard::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) emit activated(m_id);
    else QWidget::keyPressEvent(e);
}

void AppCard::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    const auto &info = ApplicationManager::info(m_id);
    const qreal h = m_hover;
    const qreal R = Design::Radius::lg;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    QRectF card = QRectF(rect()).adjusted(18, 18, -18, -18);
    card.translate(0, -5 * h); // hover lift
    if (m_pressed) { // press feedback: scale 0.985 about the centre
        p.translate(card.center());
        p.scale(0.985, 0.985);
        p.translate(-card.center());
    }

    // Soft shadow from stacked translucent rounded rects (cheap, no offscreen blur).
    p.setPen(Qt::NoPen);
    for (int i = 6; i >= 0; --i) {
        const qreal grow = i * 2.2;
        const int alpha = int((theme.isDark() ? 34 : 15) * (0.6 + 0.7 * h) / (1.0 + i * 0.35));
        p.setBrush(QColor(0, 0, 0, alpha));
        p.drawRoundedRect(card.adjusted(-grow, -grow + 8, grow, grow + 8), R + grow, R + grow);
    }

    p.setBrush(pal.surface);
    p.setPen(QPen(hasFocus() ? pal.accent : Design::mix(pal.border, info.accent, h), hasFocus() ? 2 : 1));
    p.drawRoundedRect(card, R, R);

    const qreal pad = 26;
    paintAppGlyph(&p, QRectF(card.left() + pad, card.top() + pad, Design::IconSize::xl, Design::IconSize::xl), m_id);

    p.setPen(pal.text);
    p.setFont(theme.uiFont(Design::Type::title + 2, QFont::DemiBold));
    p.drawText(QRectF(card.left() + pad, card.top() + 104, card.width() - 2 * pad, 28), Qt::AlignLeft | Qt::AlignVCenter, info.name);

    p.setPen(info.accent);
    p.setFont(theme.uiFont(Design::Type::caption, QFont::DemiBold));
    p.drawText(QRectF(card.left() + pad, card.top() + 134, card.width() - 2 * pad, 18), Qt::AlignLeft | Qt::AlignVCenter, info.tagline.toUpper());

    p.setPen(pal.textSecondary);
    p.setFont(theme.uiFont(Design::Type::body));
    p.drawText(QRectF(card.left() + pad, card.top() + 160, card.width() - 2 * pad, 70),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, info.detail);

    QColor open = info.accent;
    open.setAlphaF(0.55 + 0.45 * h);
    p.setPen(open);
    p.setFont(theme.uiFont(Design::Type::body, QFont::DemiBold));
    p.drawText(QRectF(card.left() + pad + 5 * h, card.bottom() - 46, card.width() - 2 * pad, 24),
               Qt::AlignLeft | Qt::AlignVCenter, tr("New  →"));
}

#include "appcard.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

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
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRectF card = QRectF(rect()).adjusted(2, 2, -2, -2);
    p.setBrush(Design::mix(pal.surface, pal.surfaceHover, h * (m_pressed ? 1.0 : 0.7)));
    p.setPen(QPen(hasFocus() ? pal.accent : Design::mix(pal.border, info.accent, h * 0.8), hasFocus() ? 2 : 1));
    p.drawRoundedRect(card, 8, 8);

    paintAppGlyph(&p, QRectF(card.left() + 18, card.center().y() - 24, 48, 48), m_id);

    p.setPen(pal.text);
    p.setFont(theme.uiFont(Design::Type::subtitle, QFont::DemiBold));
    p.drawText(QRectF(card.left() + 80, card.top() + 20, card.width() - 90, 24), Qt::AlignLeft | Qt::AlignVCenter, info.name);
    p.setPen(pal.textSecondary);
    p.setFont(theme.uiFont(Design::Type::body));
    p.drawText(QRectF(card.left() + 80, card.top() + 44, card.width() - 90, 20), Qt::AlignLeft | Qt::AlignVCenter, tr("Blank %1").arg(info.noun.toLower()));
    p.setFont(theme.uiFont(Design::Type::caption));
    QStringList exts;
    for (int i = 0; i < qMin(3, int(info.extensions.size())); ++i) exts << info.extensions[i].toUpper();
    p.drawText(QRectF(card.left() + 80, card.top() + 64, card.width() - 90, 18), Qt::AlignLeft | Qt::AlignVCenter, exts.join(QStringLiteral("  ·  ")));
}

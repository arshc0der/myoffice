#include "ribbonbutton.h"

#include <QMouseEvent>
#include <QPainter>

#include "core/designtokens.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"

RibbonButton::RibbonButton(QAction *action, Size size, QWidget *parent)
    : QWidget(parent), m_action(action), m_size(size), m_anim(this, "hover")
{
    setFocusPolicy(Qt::NoFocus); // keep keyboard focus in the document
    setCursor(Qt::PointingHandCursor);
    auto refresh = [this] {
        QString tip = m_action->text();
        tip.replace(QLatin1Char('\n'), QLatin1Char(' '));
        if (!m_action->shortcut().isEmpty()) tip += QStringLiteral("  (") + m_action->shortcut().toString(QKeySequence::NativeText) + QLatin1Char(')');
        setToolTip(tip);
        setAccessibleName(tip);
        updateGeometry();
        update();
    };
    connect(action, &QAction::changed, this, refresh);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
    refresh();
    setFixedSize(sizeHint());
}

QSize RibbonButton::sizeHint() const
{
    const QFontMetrics fm(ThemeManager::instance().uiFont(Design::Type::caption));
    int tw = 0;
    for (const QString &line : m_action->text().split(QLatin1Char('\n'))) tw = qMax(tw, fm.horizontalAdvance(line));
    switch (m_size) {
    case Size::Large: return { qMax(54, tw + 18), 78 };
    case Size::Small: return { 8 + 16 + 6 + tw + 8 + (m_menu ? 12 : 0), 26 };
    case Size::SmallIcon: return { 28 + (m_menu ? 12 : 0), 26 };
    }
    return {};
}

void RibbonButton::animateTo(qreal t)
{
    m_anim.stop();
    m_anim.setDuration(ThemeManager::instance().duration(Design::Duration::fast));
    m_anim.setStartValue(m_hover);
    m_anim.setEndValue(t);
    m_anim.start();
}

bool RibbonButton::inArrow(const QPoint &p) const
{
    return m_size == Size::Large ? p.y() >= height() - 18 : p.x() >= width() - 14;
}

void RibbonButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    m_pressed = true;
    m_pressedArrow = inArrow(e->position().toPoint());
    update();
}

void RibbonButton::mouseReleaseEvent(QMouseEvent *e)
{
    const bool fire = m_pressed && rect().contains(e->position().toPoint());
    m_pressed = false;
    update();
    if (!fire || !m_action->isEnabled()) return;
    if (m_menu && (!m_split || m_pressedArrow)) m_menu->popup(mapToGlobal(QPoint(0, height())));
    else m_action->trigger();
}

void RibbonButton::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool enabled = m_action->isEnabled();
    const bool checked = m_action->isCheckable() && m_action->isChecked();
    p.setOpacity(enabled ? 1.0 : 0.4);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    QColor fill = pal.surfaceHover;
    fill.setAlphaF(m_hover * (m_pressed ? 1.0 : 0.9));
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(r, Design::Radius::sm, Design::Radius::sm);
    if (checked) {
        QColor c = pal.accent;
        c.setAlphaF(0.2);
        p.setBrush(c);
        p.setPen(QPen(pal.accent, 1));
        p.drawRoundedRect(r, Design::Radius::sm, Design::Radius::sm);
    }
    const QColor ink = checked ? pal.accentDeep : pal.text;
    const QString icon = m_action->property("icon").toString();
    const qreal dpr = devicePixelRatioF();
    const QFont font = theme.uiFont(Design::Type::caption);
    p.setFont(font);
    p.setPen(ink);

    if (m_size == Size::Large) {
        p.drawPixmap(QPointF(width() / 2.0 - 14, 7), Icons::pixmap(icon, ink, 28, dpr));
        if (m_indicator.isValid()) { p.setPen(Qt::NoPen); p.setBrush(m_indicator); p.drawRoundedRect(QRectF(width() / 2.0 - 10, 36, 20, 3), 1.5, 1.5); }
        p.setPen(ink);
        p.drawText(QRectF(2, 42, width() - 4, m_menu ? 16 : 32), Qt::AlignHCenter | Qt::AlignTop, m_action->text());
        if (m_menu) p.drawPixmap(QPointF(width() / 2.0 - 5, height() - 15), Icons::pixmap(QStringLiteral("chevron-down"), ink, 10, dpr));
    } else {
        const qreal ix = m_size == Size::SmallIcon ? (m_menu ? 6 : (width() - 16) / 2.0) : 8;
        p.drawPixmap(QPointF(ix, (height() - 16) / 2.0), Icons::pixmap(icon, ink, 16, dpr));
        if (m_indicator.isValid()) { p.setPen(Qt::NoPen); p.setBrush(m_indicator); p.drawRoundedRect(QRectF(ix, height() - 6, 16, 3), 1.5, 1.5); }
        p.setPen(ink);
        if (m_size == Size::Small) p.drawText(QRectF(30, 0, width() - 30 - (m_menu ? 12 : 0), height()), Qt::AlignVCenter | Qt::AlignLeft, m_action->text());
        if (m_menu) p.drawPixmap(QPointF(width() - 13, (height() - 10) / 2.0), Icons::pixmap(QStringLiteral("chevron-down"), ink, 10, dpr));
    }
}

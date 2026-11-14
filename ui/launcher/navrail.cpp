#include "navrail.h"

#include <QMouseEvent>
#include <QPainter>

#include "core/designtokens.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"

NavRail::NavRail(QWidget *parent) : QWidget(parent)
{
    setFixedWidth(220);
    setMouseTracking(true);
    setAccessibleName(tr("Navigation"));
    m_items = { { QStringLiteral("home"), tr("Home"), false }, { QStringLiteral("folder"), tr("Open"), false },
                { QStringLiteral("settings"), tr("Settings"), true }, { QStringLiteral("help"), tr("About"), true } };
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

QRectF NavRail::itemRect(int i) const
{
    int topIdx = 0, botIdx = 0;
    for (int k = 0; k < i; ++k) (m_items[k].bottom ? botIdx : topIdx)++;
    const qreal h = 40;
    if (!m_items[i].bottom) return QRectF(8, 76 + topIdx * (h + 2), width() - 16, h);
    int bottomCount = 0;
    for (const Item &it : m_items) bottomCount += it.bottom;
    return QRectF(8, height() - 8 - (bottomCount - botIdx) * (h + 2) + 2, width() - 16, h);
}

int NavRail::indexAt(const QPointF &p) const
{
    for (int i = 0; i < m_items.size(); ++i) if (itemRect(i).contains(p)) return i;
    return -1;
}

void NavRail::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), pal.surface);
    p.setPen(pal.border);
    p.drawLine(width() - 1, 0, width() - 1, height());

    // Brand
    p.setPen(Qt::NoPen);
    p.setBrush(pal.accent);
    p.drawRoundedRect(QRectF(20, 24, 28, 28), 7, 7);
    p.setPen(Qt::white);
    p.setFont(theme.uiFont(13, QFont::Bold));
    p.drawText(QRectF(20, 24, 28, 28), Qt::AlignCenter, QStringLiteral("M"));
    p.setPen(pal.text);
    p.setFont(theme.uiFont(Design::Type::subtitle, QFont::DemiBold));
    p.drawText(QRectF(58, 24, width() - 70, 28), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("MyOffice"));

    for (int i = 0; i < m_items.size(); ++i) {
        const QRectF r = itemRect(i);
        const bool selected = i == 0;
        if (selected || i == m_hover) {
            p.setPen(Qt::NoPen);
            p.setBrush(selected ? Design::mix(pal.surface, pal.accent, 0.12) : pal.surfaceHover);
            p.drawRoundedRect(r, 6, 6);
        }
        if (selected) { p.setBrush(pal.accent); p.drawRoundedRect(QRectF(r.left(), r.top() + 9, 3, r.height() - 18), 1.5, 1.5); }
        const QColor ink = selected ? pal.accentDeep : pal.text;
        p.drawPixmap(QPointF(r.left() + 16, r.center().y() - 10), Icons::pixmap(m_items[i].icon, ink, 20, devicePixelRatioF()));
        p.setPen(ink);
        p.setFont(theme.uiFont(Design::Type::body, selected ? QFont::DemiBold : QFont::Normal));
        p.drawText(QRectF(r.left() + 48, r.top(), r.width() - 52, r.height()), Qt::AlignVCenter | Qt::AlignLeft, m_items[i].text);
    }
}

void NavRail::mouseMoveEvent(QMouseEvent *e)
{
    const int i = indexAt(e->position());
    if (i != m_hover) { m_hover = i; update(); }
    setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void NavRail::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    switch (indexAt(e->position())) {
    case 0: emit homeRequested(); break;
    case 1: emit openRequested(); break;
    case 2: emit settingsRequested(); break;
    case 3: emit aboutRequested(); break;
    default: break;
    }
}

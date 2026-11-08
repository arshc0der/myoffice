#include "colormenu.h"

#include <QColorDialog>
#include <QMouseEvent>
#include <QPainter>
#include <QWidgetAction>

#include "core/thememanager.h"

SwatchGrid::SwatchGrid(QWidget *parent) : QWidget(parent) { setMouseTracking(true); }

QColor SwatchGrid::colorAt(int idx) const
{
    static const char *base[10] = { "#000000", "#7F7F7F", "#C00000", "#FF6600", "#FFC000", "#00B050", "#00B0F0", "#0070C0", "#7030A0", "#FF66CC" };
    const int row = idx / 10, col = idx % 10;
    QColor c(base[col]);
    switch (row) {
    case 1: return col == 0 ? QColor("#404040") : c.lighter(170);
    case 2: return col == 0 ? QColor("#A6A6A6") : c.lighter(135);
    case 3: return col == 0 ? QColor("#FFFFFF") : c.darker(140);
    default: return c;
    }
}

int SwatchGrid::indexAt(const QPoint &p) const
{
    const int c = (p.x() - 6) / 24, r = (p.y() - 6) / 24;
    return (p.x() >= 6 && p.y() >= 6 && c < 10 && r < 4) ? r * 10 + c : -1;
}

void SwatchGrid::paintEvent(QPaintEvent *)
{
    const Palette &pal = ThemeManager::instance().palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    for (int i = 0; i < 40; ++i) {
        const QRectF r(6 + (i % 10) * 24 + 2, 6 + (i / 10) * 24 + 2, 20, 20);
        p.setBrush(colorAt(i));
        p.setPen(QPen(i == m_hover ? pal.accent : pal.border, i == m_hover ? 2 : 1));
        p.drawRoundedRect(r, 4, 4);
    }
}

void SwatchGrid::mouseMoveEvent(QMouseEvent *e) { m_hover = indexAt(e->position().toPoint()); update(); }
void SwatchGrid::mouseReleaseEvent(QMouseEvent *e)
{
    const int i = indexAt(e->position().toPoint());
    if (i >= 0) emit colorChosen(colorAt(i));
}

ColorMenu::ColorMenu(const QString &noColorText, QWidget *parent) : QMenu(parent)
{
    auto *wa = new QWidgetAction(this);
    auto *grid = new SwatchGrid(this);
    wa->setDefaultWidget(grid);
    addAction(wa);
    addSeparator();
    connect(grid, &SwatchGrid::colorChosen, this, [this](const QColor &c) { close(); emit colorPicked(c); });
    connect(addAction(noColorText), &QAction::triggered, this, &ColorMenu::noColorPicked);
    connect(addAction(tr("More colors…")), &QAction::triggered, this, [this] {
        const QColor c = QColorDialog::getColor(Qt::black, this, tr("Choose color"));
        if (c.isValid()) emit colorPicked(c);
    });
}

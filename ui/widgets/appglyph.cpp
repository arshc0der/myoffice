#include "appglyph.h"

void paintAppGlyph(QPainter *p, const QRectF &r, ApplicationManager::AppId id)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QColor accent = ApplicationManager::info(id).accent;
    p->setPen(Qt::NoPen);
    p->setBrush(accent);
    p->drawRoundedRect(r, r.width() * 0.24, r.width() * 0.24);

    const qreal w = r.width();
    const QPointF o = r.topLeft();
    auto at = [&](qreal x, qreal y) { return o + QPointF(x * w, y * w); };
    QPen white(Qt::white, w * 0.055, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    switch (id) {
    case ApplicationManager::AppId::Word:
        p->setPen(white);
        p->drawLine(at(0.28, 0.32), at(0.72, 0.32));
        p->drawLine(at(0.28, 0.48), at(0.72, 0.48));
        p->drawLine(at(0.28, 0.64), at(0.72, 0.64));
        p->drawLine(at(0.28, 0.80), at(0.5, 0.80));
        break;
    case ApplicationManager::AppId::Excel: {
        const QRectF grid(at(0.26, 0.26), at(0.74, 0.74));
        p->setPen(white);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(grid, w * 0.04, w * 0.04);
        p->drawLine(QPointF(grid.left() + grid.width() / 3, grid.top()), QPointF(grid.left() + grid.width() / 3, grid.bottom()));
        p->drawLine(QPointF(grid.left() + grid.width() * 2 / 3, grid.top()), QPointF(grid.left() + grid.width() * 2 / 3, grid.bottom()));
        p->drawLine(QPointF(grid.left(), grid.top() + grid.height() / 3), QPointF(grid.right(), grid.top() + grid.height() / 3));
        p->drawLine(QPointF(grid.left(), grid.top() + grid.height() * 2 / 3), QPointF(grid.right(), grid.top() + grid.height() * 2 / 3));
        break;
    }
    case ApplicationManager::AppId::PowerPoint: {
        const QRectF disc(at(0.27, 0.27), at(0.73, 0.73));
        p->setPen(white);
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(disc);
        p->setPen(Qt::NoPen);
        p->setBrush(Qt::white);
        p->drawPie(disc.adjusted(w * 0.07, w * 0.07, -w * 0.07, -w * 0.07), 90 * 16, -90 * 16 * 3 / 2);
        break;
    }
    }
    p->restore();
}

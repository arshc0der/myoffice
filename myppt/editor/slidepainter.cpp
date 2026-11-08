#include "slidepainter.h"

#include <QFont>
#include <QPainter>
#include <QPen>

namespace SlidePainter {
namespace {
void drawBody(QPainter *p, const SlideObject &o)
{
    const QRectF r = o.rect;
    if (o.line.isValid() && o.lineWidth > 0) p->setPen(QPen(o.line, o.lineWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    else p->setPen(Qt::NoPen);
    p->setBrush(o.fill.isValid() ? QBrush(o.fill) : QBrush(Qt::NoBrush));
    switch (o.type) {
    case SlideObject::Text:
    case SlideObject::Rect: p->drawRect(r); break;
    case SlideObject::RoundRect: { const qreal rad = qMin(r.width(), r.height()) * 0.16; p->drawRoundedRect(r, rad, rad); break; }
    case SlideObject::Ellipse: p->drawEllipse(r); break;
    case SlideObject::Line: p->drawLine(o.flipV ? r.bottomLeft() : r.topLeft(), o.flipV ? r.topRight() : r.bottomRight()); break;
    case SlideObject::Image:
        if (!o.image.isNull()) p->drawImage(r, o.image);
        break;
    }
    if (o.type == SlideObject::Line || o.type == SlideObject::Image || o.text.isEmpty()) return;
    QFont f;
    f.setPixelSize(qMax(1, qRound(o.fontSize * 4.0 / 3.0))); // points -> px at 96 dpi
    f.setBold(o.bold);
    f.setItalic(o.italic);
    f.setUnderline(o.underline);
    p->setFont(f);
    p->setPen(o.textColor.isValid() ? o.textColor : QColor(Qt::black));
    p->setBrush(Qt::NoBrush);
    int flags = int(Qt::TextWordWrap);
    flags |= o.align == 1 ? int(Qt::AlignHCenter) : o.align == 2 ? int(Qt::AlignRight) : int(Qt::AlignLeft);
    flags |= o.valign == 1 ? int(Qt::AlignVCenter) : o.valign == 2 ? int(Qt::AlignBottom) : int(Qt::AlignTop);
    p->drawText(r.adjusted(10, 6, -10, -6), flags, o.text);
}
} // namespace

void paintObject(QPainter *p, const SlideObject &o, qreal t)
{
    using Ppt::Effect;
    p->save();
    const bool animating = t < 1.0 && o.anim.effect != Effect::None;
    qreal opacity = 1.0, dx = 0, dy = 0, scale = 1.0, spin = 0;
    if (animating) {
        switch (o.anim.effect) {
        case Effect::Fade: opacity = t; break;
        case Effect::FlyLeft: dx = -o.rect.right() * (1 - t); break;
        case Effect::FlyRight: dx = (Ppt::kSlideW - o.rect.left()) * (1 - t); break;
        case Effect::FlyTop: dy = -o.rect.bottom() * (1 - t); break;
        case Effect::FlyBottom: dy = (Ppt::kSlideH - o.rect.top()) * (1 - t); break;
        case Effect::Zoom: scale = qMax(0.001, t); opacity = t; break;
        case Effect::Spin: scale = qMax(0.001, t); opacity = t; spin = 360.0 * (1 - t); break;
        case Effect::Wipe:
            p->setClipRect(QRectF(o.rect.left() - 20, o.rect.top() - 20, (o.rect.width() + 40) * t, o.rect.height() + 40), Qt::IntersectClip);
            break;
        default: break; // Appear: visible as soon as progress > 0
        }
    }
    p->setOpacity(p->opacity() * opacity);
    const QPointF c = o.rect.center();
    p->translate(c.x() + dx, c.y() + dy);
    p->rotate(o.rotation + spin);
    p->scale(scale, scale);
    p->translate(-c.x(), -c.y());
    drawBody(p, o);
    p->restore();
}

void paintSlide(QPainter *p, const Slide &s, const QRectF &target, const QVector<qreal> *progress)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setRenderHint(QPainter::TextAntialiasing);
    p->setRenderHint(QPainter::SmoothPixmapTransform);
    p->translate(target.topLeft());
    p->scale(target.width() / Ppt::kSlideW, target.height() / Ppt::kSlideH);
    const QRectF full(0, 0, Ppt::kSlideW, Ppt::kSlideH);
    p->setClipRect(full);
    p->fillRect(full, s.background.isValid() ? s.background : QColor(Qt::white));
    for (int i = 0; i < s.objects.size(); ++i) {
        const qreal t = (progress && i < progress->size()) ? progress->at(i) : 1.0;
        if (t > 0) paintObject(p, s.objects[i], t);
    }
    p->restore();
}

QPixmap renderSlide(const Slide &s, const QSize &px, qreal dpr, const QVector<qreal> *progress)
{
    QPixmap pm(QSize(qMax(1, qRound(px.width() * dpr)), qMax(1, qRound(px.height() * dpr))));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::white);
    QPainter p(&pm);
    paintSlide(&p, s, QRectF(0, 0, px.width(), px.height()), progress);
    return pm;
}
}

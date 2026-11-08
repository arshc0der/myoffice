#pragma once
#include <QPixmap>
#include <QVector>

#include "myppt/document/slidemodel.h"

class QPainter;

// Shared renderer used by the editor canvas, thumbnails, slide show, PDF and PNG export.
// `progress` (optional, one entry per object) drives object animations: 0 = hidden, 1 = fully shown.
namespace SlidePainter {
void paintObject(QPainter *p, const SlideObject &o, qreal progress = 1.0);
void paintSlide(QPainter *p, const Slide &s, const QRectF &target, const QVector<qreal> *progress = nullptr);
QPixmap renderSlide(const Slide &s, const QSize &pixelSize, qreal dpr = 1.0, const QVector<qreal> *progress = nullptr);
}

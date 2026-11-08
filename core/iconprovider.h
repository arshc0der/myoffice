#pragma once
#include <QColor>
#include <QPixmap>

namespace Icons {
// Renders resources/icons/<name>.svg tinted with `color` (replaces currentColor); cached per size/dpr.
QPixmap pixmap(const QString &name, const QColor &color, int logicalSize, qreal dpr);
}

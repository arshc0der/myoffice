#pragma once
#include <QPainter>
#include "core/applicationmanager.h"

// Paints the original MyWord / MyExcel / MyPowerPoint app icons (no third-party assets).
void paintAppGlyph(QPainter *p, const QRectF &r, ApplicationManager::AppId id);

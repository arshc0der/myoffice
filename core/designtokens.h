#pragma once
// Design system tokens shared by every MyOffice component.
#include <QColor>

namespace Design {
namespace Space    { inline constexpr int xs = 4, sm = 8, md = 12, lg = 16, xl = 24, xxl = 32; }
namespace Radius   { inline constexpr int sm = 6, md = 10, lg = 16; }
namespace IconSize { inline constexpr int sm = 16, md = 20, lg = 28, xl = 56; }
namespace Duration { inline constexpr int fast = 120, normal = 200, slow = 320; } // milliseconds
namespace Type     { inline constexpr qreal caption = 9.5, body = 10.5, subtitle = 12.5, title = 16, display = 26; }
namespace Window   { inline constexpr int shadowMargin = 12, cornerRadius = 12, titleBarHeight = 40; }

inline QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
} // namespace Design

#pragma once
#include <QColor>
#include <QImage>
#include <QList>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QStringList>

// Data model of a MyPowerPoint presentation. Slides are 16:9 and use a fixed 1280 x 720 coordinate
// space (1 unit = 1 px at 96 dpi = 9525 EMU), so geometry is resolution independent.
namespace Ppt {
constexpr int kSlideW = 1280;
constexpr int kSlideH = 720;

enum class Effect { None, Appear, Fade, FlyLeft, FlyRight, FlyTop, FlyBottom, Zoom, Wipe, Spin };
enum class Trigger { OnClick, WithPrevious, AfterPrevious };
enum class Transition { None, Fade, PushLeft, PushUp, Wipe, Cover, Zoom, Dissolve };

inline QStringList effectNames()
{
    return { QObject::tr("None"), QObject::tr("Appear"), QObject::tr("Fade"), QObject::tr("Fly in from left"),
             QObject::tr("Fly in from right"), QObject::tr("Fly in from top"), QObject::tr("Fly in from bottom"),
             QObject::tr("Zoom"), QObject::tr("Wipe"), QObject::tr("Spin") };
}
inline QStringList triggerNames() { return { QObject::tr("On click"), QObject::tr("With previous"), QObject::tr("After previous") }; }
inline QStringList transitionNames()
{
    return { QObject::tr("None"), QObject::tr("Fade"), QObject::tr("Push left"), QObject::tr("Push up"), QObject::tr("Wipe"),
             QObject::tr("Cover"), QObject::tr("Zoom"), QObject::tr("Dissolve") };
}
} // namespace Ppt

struct Animation
{
    Ppt::Effect effect = Ppt::Effect::None;
    Ppt::Trigger trigger = Ppt::Trigger::OnClick;
    int durationMs = 600;
    int delayMs = 0;
};

struct SlideObject
{
    enum Type { Text, Rect, RoundRect, Ellipse, Line, Image };

    int id = 0; // shape id inside a PPTX slide (only meaningful while reading/writing)
    Type type = Rect;
    QRectF rect { 100, 100, 320, 160 };
    qreal rotation = 0;
    bool flipV = false;       // lines: draw from bottom-left to top-right
    QColor fill;              // invalid = no fill
    QColor line;              // invalid = no outline
    qreal lineWidth = 2;
    QString text;
    qreal fontSize = 24;      // points
    bool bold = false, italic = false, underline = false;
    QColor textColor;         // invalid = black
    int align = 0;            // 0 left, 1 centre, 2 right
    int valign = 0;           // 0 top, 1 middle, 2 bottom
    QImage image;
    Animation anim;
};

struct Slide
{
    QList<SlideObject> objects;
    QColor background { Qt::white };
    Ppt::Transition transition = Ppt::Transition::None;
    int transitionMs = 700;
    int advanceAfterMs = 0;   // 0 = advance on click only
    QString notes;
};

struct Deck
{
    QList<Slide> slides;
};

inline SlideObject makeObject(SlideObject::Type t)
{
    SlideObject o;
    o.type = t;
    switch (t) {
    case SlideObject::Text:
        o.rect = { 140, 140, 480, 80 };
        o.fontSize = 28;
        o.textColor = QColor("#1F2937");
        o.text = QObject::tr("Text");
        break;
    case SlideObject::Line:
        o.rect = { 200, 300, 360, 0 };
        o.line = QColor("#1F2937");
        o.lineWidth = 4;
        break;
    default:
        o.rect = { 200, 200, 320, 180 };
        o.fill = QColor("#3B82F6");
        o.line = QColor("#1E40AF");
        o.lineWidth = 2;
        o.textColor = QColor(Qt::white);
        o.align = 1;
        o.valign = 1;
        break;
    }
    return o;
}

inline QString objectLabel(const SlideObject &o)
{
    switch (o.type) {
    case SlideObject::Text: return o.text.isEmpty() ? QObject::tr("Text box") : o.text.simplified().left(24);
    case SlideObject::Rect: return QObject::tr("Rectangle");
    case SlideObject::RoundRect: return QObject::tr("Rounded rectangle");
    case SlideObject::Ellipse: return QObject::tr("Ellipse");
    case SlideObject::Line: return QObject::tr("Line");
    case SlideObject::Image: return QObject::tr("Picture");
    }
    return {};
}

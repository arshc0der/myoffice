#include "slidecanvas.h"

#include <QInputDialog>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

#include "core/thememanager.h"
#include "slidepainter.h"

namespace {
constexpr int kHandle = 9; // handle size in widget pixels
constexpr qreal kMinSize = 10;

qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-9) return QLineF(p, a).length();
    const qreal t = qBound(0.0, ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2, 1.0);
    return QLineF(p, a + ab * t).length();
}
}

SlideCanvas::SlideCanvas(SlideProvider provider, QWidget *parent) : QWidget(parent), m_provider(std::move(provider))
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(320, 200);
    setAccessibleName(tr("Slide editor"));
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

QRectF SlideCanvas::pageRect() const
{
    const QRectF avail = QRectF(rect()).adjusted(28, 28, -28, -28);
    const qreal s = qMax(0.01, qMin(avail.width() / Ppt::kSlideW, avail.height() / Ppt::kSlideH));
    const qreal w = Ppt::kSlideW * s, h = Ppt::kSlideH * s;
    return QRectF(avail.center().x() - w / 2, avail.center().y() - h / 2, w, h);
}

qreal SlideCanvas::scale() const { return pageRect().width() / Ppt::kSlideW; }

QPointF SlideCanvas::toSlide(const QPointF &p) const
{
    const QRectF page = pageRect();
    return QPointF((p.x() - page.left()) / scale(), (p.y() - page.top()) / scale());
}

void SlideCanvas::setSelected(int index)
{
    if (m_sel == index) return;
    m_sel = index;
    emit selectionChanged();
    update();
}

void SlideCanvas::setReadOnly(bool on)
{
    m_readOnly = on;
    if (on) m_tool = -1;
    update();
}

void SlideCanvas::setInsertTool(int objectType)
{
    m_tool = m_readOnly ? -1 : objectType;
    setCursor(m_tool >= 0 ? Qt::CrossCursor : Qt::ArrowCursor);
    setFocus();
}

void SlideCanvas::begin(const QString &text)
{
    if (m_started) return;
    m_started = true;
    emit checkpointRequested(text);
}

QPointF SlideCanvas::handlePos(const QRectF &r, int h) const
{
    switch (h) {
    case 0: return r.topLeft();
    case 1: return QPointF(r.center().x(), r.top());
    case 2: return r.topRight();
    case 3: return QPointF(r.right(), r.center().y());
    case 4: return r.bottomRight();
    case 5: return QPointF(r.center().x(), r.bottom());
    case 6: return r.bottomLeft();
    default: return QPointF(r.left(), r.center().y());
    }
}

int SlideCanvas::hitHandle(const QPointF &wp) const
{
    Slide *s = m_provider();
    if (!s || m_sel < 0 || m_sel >= s->objects.size() || m_readOnly) return -1;
    const SlideObject &o = s->objects[m_sel];
    const QRectF page = pageRect();
    const qreal sc = scale();
    for (int h = 0; h < 8; ++h) {
        if (o.type == SlideObject::Line && h != 0 && h != 4) continue;
        const QPointF c = handlePos(o.rect, h);
        const QPointF w(page.left() + c.x() * sc, page.top() + c.y() * sc);
        if (QRectF(w.x() - kHandle, w.y() - kHandle, kHandle * 2, kHandle * 2).contains(wp)) return h;
    }
    return -1;
}

int SlideCanvas::hitObject(const QPointF &sp) const
{
    Slide *s = m_provider();
    if (!s) return -1;
    const qreal tol = 4.0 / scale();
    for (int i = int(s->objects.size()) - 1; i >= 0; --i) {
        const SlideObject &o = s->objects[i];
        if (o.type == SlideObject::Line) {
            const QPointF a = o.flipV ? o.rect.bottomLeft() : o.rect.topLeft();
            const QPointF b = o.flipV ? o.rect.topRight() : o.rect.bottomRight();
            if (distanceToSegment(sp, a, b) <= o.lineWidth / 2 + tol * 2) return i;
        } else if (o.rect.adjusted(-tol, -tol, tol, tol).contains(sp)) {
            return i;
        }
    }
    return -1;
}

void SlideCanvas::updateCursor(const QPointF &wp)
{
    if (m_tool >= 0) { setCursor(Qt::CrossCursor); return; }
    const int h = hitHandle(wp);
    if (h == 0 || h == 4) setCursor(Qt::SizeFDiagCursor);
    else if (h == 2 || h == 6) setCursor(Qt::SizeBDiagCursor);
    else if (h == 1 || h == 5) setCursor(Qt::SizeVerCursor);
    else if (h == 3 || h == 7) setCursor(Qt::SizeHorCursor);
    else if (!m_readOnly && hitObject(toSlide(wp)) >= 0) setCursor(Qt::SizeAllCursor);
    else setCursor(Qt::ArrowCursor);
}

void SlideCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    setFocus();
    Slide *s = m_provider();
    if (!s) return;
    const QPointF sp = toSlide(e->position());
    m_started = false;
    if (m_readOnly) {
        const int hit = hitObject(sp);
        if (hit != m_sel) { m_sel = hit; emit selectionChanged(); }
        update();
        return;
    }
    if (m_tool >= 0) {
        m_mode = Mode::Create;
        m_pressPos = sp;
        if (m_sel != -1) { m_sel = -1; emit selectionChanged(); }
        return;
    }
    const int h = hitHandle(e->position());
    if (h >= 0) {
        m_mode = Mode::Resize;
        m_handle = h;
        m_pressPos = sp;
        m_origRect = s->objects[m_sel].rect;
        return;
    }
    const int hit = hitObject(sp);
    if (hit != m_sel) { m_sel = hit; emit selectionChanged(); }
    if (hit >= 0) {
        m_mode = Mode::Move;
        m_pressPos = sp;
        m_origRect = s->objects[hit].rect;
    }
    update();
}

void SlideCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (m_mode == Mode::Idle) { updateCursor(e->position()); return; }
    const QPointF sp = toSlide(e->position());
    const QPointF d = sp - m_pressPos;

    if (m_mode == Mode::Create) {
        if (!m_started && QPointF(d).manhattanLength() < 6) return;
        if (!m_started) {
            begin(tr("Insert"));
            Slide *s = m_provider();
            if (!s) return;
            s->objects << makeObject(SlideObject::Type(m_tool));
            m_sel = int(s->objects.size()) - 1;
            emit selectionChanged();
        }
        Slide *s = m_provider();
        if (!s || m_sel < 0 || m_sel >= s->objects.size()) return;
        SlideObject &o = s->objects[m_sel];
        QRectF r(m_pressPos, sp);
        r = r.normalized();
        if (o.type == SlideObject::Line) {
            o.flipV = (sp.x() - m_pressPos.x()) * (sp.y() - m_pressPos.y()) < 0;
        }
        o.rect = r;
        emit modified();
        update();
        return;
    }

    Slide *s = m_provider();
    if (!s || m_sel < 0 || m_sel >= s->objects.size()) return;
    if (m_mode == Mode::Move) {
        if (!m_started && d.manhattanLength() < 3) return;
        begin(tr("Move"));
        s = m_provider();
        s->objects[m_sel].rect = m_origRect.translated(d);
    } else if (m_mode == Mode::Resize) {
        begin(tr("Resize"));
        s = m_provider();
        SlideObject &o = s->objects[m_sel];
        const qreal minSize = o.type == SlideObject::Line ? 0 : kMinSize;
        QRectF r = m_origRect;
        const int h = m_handle;
        if (h == 0 || h == 6 || h == 7) r.setLeft(qMin(m_origRect.left() + d.x(), m_origRect.right() - minSize));
        if (h == 2 || h == 3 || h == 4) r.setRight(qMax(m_origRect.right() + d.x(), m_origRect.left() + minSize));
        if (h == 0 || h == 1 || h == 2) r.setTop(qMin(m_origRect.top() + d.y(), m_origRect.bottom() - minSize));
        if (h == 4 || h == 5 || h == 6) r.setBottom(qMax(m_origRect.bottom() + d.y(), m_origRect.top() + minSize));
        o.rect = r;
    }
    emit modified();
    update();
}

void SlideCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    const Mode mode = m_mode;
    m_mode = Mode::Idle;
    if (mode == Mode::Create) {
        const int type = m_tool;
        if (!m_started) { // a plain click: drop a default-sized object centred on the click
            begin(tr("Insert"));
            Slide *s = m_provider();
            if (!s) { m_started = false; return; }
            SlideObject o = makeObject(SlideObject::Type(type));
            const QPointF c = m_pressPos;
            o.rect.moveCenter(c);
            o.rect.moveLeft(qBound(0.0, o.rect.left(), Ppt::kSlideW - o.rect.width()));
            o.rect.moveTop(qBound(0.0, o.rect.top(), Ppt::kSlideH - o.rect.height()));
            s->objects << o;
            m_sel = int(s->objects.size()) - 1;
            emit selectionChanged();
        } else {
            Slide *s = m_provider();
            if (s && m_sel >= 0 && m_sel < s->objects.size()) {
                SlideObject &o = s->objects[m_sel];
                if (o.type != SlideObject::Line) { o.rect.setWidth(qMax(o.rect.width(), 20.0)); o.rect.setHeight(qMax(o.rect.height(), 20.0)); }
                else o.rect.setWidth(qMax(o.rect.width(), 20.0));
            }
        }
        m_started = false;
        m_tool = -1;
        setCursor(Qt::ArrowCursor);
        emit modified();
        update();
        Slide *s = m_provider();
        if (s && m_sel >= 0 && m_sel < s->objects.size() && s->objects[m_sel].type == SlideObject::Text) editText(m_sel);
        return;
    }
    m_started = false;
    updateCursor(e->position());
}

void SlideCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || m_readOnly) return;
    const int hit = hitObject(toSlide(e->position()));
    if (hit >= 0) { setSelected(hit); editText(hit); }
}

void SlideCanvas::editText(int index)
{
    Slide *s = m_provider();
    if (!s || m_readOnly || index < 0 || index >= s->objects.size()) return;
    const SlideObject &o = s->objects[index];
    if (o.type == SlideObject::Line || o.type == SlideObject::Image) return;
    bool ok = false;
    const QString t = QInputDialog::getMultiLineText(this, tr("Edit text"), tr("Text:"), o.text, &ok);
    if (!ok) return;
    m_started = false;
    begin(tr("Edit text"));
    s = m_provider();
    if (s && index < s->objects.size()) s->objects[index].text = t;
    m_started = false;
    emit modified();
    update();
}

void SlideCanvas::keyPressEvent(QKeyEvent *e)
{
    Slide *s = m_provider();
    if (!s || m_readOnly) { QWidget::keyPressEvent(e); return; }
    if (e->key() == Qt::Key_Escape) {
        if (m_tool >= 0) setInsertTool(-1);
        else setSelected(-1);
        return;
    }
    if (m_sel < 0 || m_sel >= s->objects.size()) { QWidget::keyPressEvent(e); return; }
    const int step = (e->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    switch (e->key()) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        m_started = false;
        begin(tr("Delete"));
        s = m_provider();
        s->objects.removeAt(m_sel);
        m_started = false;
        m_sel = -1;
        emit selectionChanged();
        emit modified();
        break;
    case Qt::Key_Left: case Qt::Key_Right: case Qt::Key_Up: case Qt::Key_Down: {
        m_started = false;
        begin(tr("Nudge"));
        s = m_provider();
        const qreal dx = e->key() == Qt::Key_Left ? -step : e->key() == Qt::Key_Right ? step : 0;
        const qreal dy = e->key() == Qt::Key_Up ? -step : e->key() == Qt::Key_Down ? step : 0;
        s->objects[m_sel].rect.translate(dx, dy);
        m_started = false;
        emit modified();
        break;
    }
    case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_F2:
        editText(m_sel);
        break;
    default:
        QWidget::keyPressEvent(e);
        return;
    }
    update();
}

void SlideCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &pal = ThemeManager::instance().palette();
    p.fillRect(rect(), pal.canvas);
    Slide *s = m_provider();
    if (!s) return;
    const QRectF page = pageRect();
    p.setPen(Qt::NoPen);
    for (int i = 6; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, 7));
        p.drawRoundedRect(page.adjusted(-i, -i + 2, i, i + 2), 3 + i, 3 + i);
    }
    SlidePainter::paintSlide(&p, *s, page);

    if (m_sel < 0 || m_sel >= s->objects.size()) return;
    const SlideObject &o = s->objects[m_sel];
    const qreal sc = scale();
    const QRectF wr(page.left() + o.rect.left() * sc, page.top() + o.rect.top() * sc, o.rect.width() * sc, o.rect.height() * sc);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(pal.accent, 1.5));
    if (o.type != SlideObject::Line) p.drawRect(wr);
    if (m_readOnly) return;
    p.setBrush(Qt::white);
    for (int h = 0; h < 8; ++h) {
        if (o.type == SlideObject::Line && h != 0 && h != 4) continue;
        const QPointF c = handlePos(wr, h);
        p.drawRect(QRectF(c.x() - kHandle / 2.0, c.y() - kHandle / 2.0, kHandle, kHandle));
    }
}

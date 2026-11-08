#include "slideshow.h"

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <algorithm>

#include "core/thememanager.h"
#include "slidepainter.h"

// ====================================================================== SlideAnimator
SlideAnimator::SlideAnimator(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &SlideAnimator::tick);
}

void SlideAnimator::setSlide(const Slide *slide)
{
    m_timer.stop();
    m_steps.clear();
    m_runs.clear();
    m_progress.clear();
    m_next = 0;
    m_autoFirst = false;
    if (!slide) return;

    // Group animations into click steps and compute each entry's start offset inside its step.
    qint64 prevStart = 0;
    int prevDur = 0;
    for (int i = 0; i < slide->objects.size(); ++i) {
        const SlideObject &o = slide->objects.at(i);
        m_progress.append(o.anim.effect == Ppt::Effect::None ? 1.0 : 0.0);
        if (o.anim.effect == Ppt::Effect::None) continue;
        const bool newStep = o.anim.trigger == Ppt::Trigger::OnClick || m_steps.isEmpty();
        if (newStep) {
            if (m_steps.isEmpty() && o.anim.trigger != Ppt::Trigger::OnClick) m_autoFirst = true;
            m_steps.append(QList<Entry>());
            prevStart = 0;
            prevDur = 0;
        }
        qint64 start = o.anim.delayMs;
        if (!newStep) start = (o.anim.trigger == Ppt::Trigger::AfterPrevious ? prevStart + prevDur : prevStart) + o.anim.delayMs;
        m_steps.last().append({ i, start, qMax(1, o.anim.durationMs) });
        prevStart = start;
        prevDur = qMax(1, o.anim.durationMs);
    }
}

void SlideAnimator::startAuto()
{
    if (m_autoFirst && !m_steps.isEmpty() && m_next == 0) startStep(0);
}

void SlideAnimator::nextStep()
{
    if (m_next < m_steps.size()) startStep(m_next);
}

void SlideAnimator::startStep(int index)
{
    m_next = index + 1;
    if (m_runs.isEmpty()) m_clock.restart();
    const qint64 now = m_clock.elapsed();
    for (const Entry &e : m_steps.at(index)) m_runs.append({ e.obj, now + e.start, e.dur });
    if (!m_timer.isActive()) m_timer.start();
    tick();
}

void SlideAnimator::tick()
{
    const qint64 now = m_clock.elapsed();
    for (int i = m_runs.size() - 1; i >= 0; --i) {
        const Run &r = m_runs.at(i);
        if (now < r.start) continue;
        const qreal t = qBound<qreal>(0.0, qreal(now - r.start) / r.dur, 1.0);
        const qreal eased = 1.0 - (1.0 - t) * (1.0 - t); // ease-out
        m_progress[r.obj] = t >= 1.0 ? 1.0 : qMax<qreal>(0.001, eased);
        if (t >= 1.0) m_runs.removeAt(i);
    }
    emit updated();
    if (m_runs.isEmpty()) {
        m_timer.stop();
        emit finished();
    }
}

void SlideAnimator::finishAll()
{
    m_timer.stop();
    m_runs.clear();
    m_next = m_steps.size();
    for (const auto &step : m_steps) for (const Entry &e : step) m_progress[e.obj] = 1.0;
    emit updated();
    emit finished();
}

// ====================================================================== SlideShow
SlideShow::SlideShow(const Deck &deck, int startIndex) : QWidget(nullptr), m_deck(deck), m_index(qBound(0, startIndex, int(deck.slides.size()) - 1))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("MyPowerPoint — Slide show"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    connect(&m_anim, &SlideAnimator::updated, this, qOverload<>(&QWidget::update));
    connect(&m_anim, &SlideAnimator::finished, this, [this] { checkAutoAdvance(); });
    m_autoTimer.setSingleShot(true);
    connect(&m_autoTimer, &QTimer::timeout, this, [this] {
        if (m_transitioning || m_anim.running() || m_anim.hasMoreSteps()) { m_autoTimer.start(300); return; }
        advance();
    });
    m_cursorTimer.setSingleShot(true);
    m_cursorTimer.setInterval(2000);
    connect(&m_cursorTimer, &QTimer::timeout, this, [this] { setCursor(Qt::BlankCursor); });
    m_trans.setStartValue(0.0);
    m_trans.setEndValue(1.0);
    m_trans.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_trans, &QVariantAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    connect(&m_trans, &QVariantAnimation::finished, this, [this] { if (m_transitioning) finishTransition(); });
}

void SlideShow::run()
{
    showFullScreen();
    activateWindow();
    setFocus();
    m_cursorTimer.start();
    m_anim.setSlide(&m_deck.slides[m_index]);
    m_anim.startAuto();
    checkAutoAdvance();
    update();
}

QRectF SlideShow::targetRect() const
{
    const qreal s = qMin(qreal(width()) / Ppt::kSlideW, qreal(height()) / Ppt::kSlideH);
    const qreal w = Ppt::kSlideW * s, h = Ppt::kSlideH * s;
    return QRectF((width() - w) / 2, (height() - h) / 2, w, h);
}

QPixmap SlideShow::renderCurrent(bool final) const
{
    const QSize px = targetRect().size().toSize();
    if (final) return SlidePainter::renderSlide(m_deck.slides.at(m_index), px, devicePixelRatioF(), nullptr);
    return SlidePainter::renderSlide(m_deck.slides.at(m_index), px, devicePixelRatioF(), &m_anim.progress());
}

void SlideShow::goTo(int index, bool withTransition)
{
    index = qBound(0, index, int(m_deck.slides.size()) - 1);
    m_autoTimer.stop();
    if (m_transitioning) { m_trans.stop(); m_transitioning = false; }
    const Slide &next = m_deck.slides.at(index);
    const bool animate = withTransition && next.transition != Ppt::Transition::None && next.transitionMs > 0 && !ThemeManager::instance().reducedMotion();
    if (animate) m_from = renderCurrent(true); // outgoing slide fully revealed
    m_index = index;
    m_anim.setSlide(&m_deck.slides[m_index]);
    if (animate) {
        m_to = renderCurrent(false); // incoming slide in its initial (pre-animation) state
        beginTransition(next.transition, next.transitionMs);
        return;
    }
    m_anim.startAuto();
    checkAutoAdvance();
    update();
}

void SlideShow::beginTransition(Ppt::Transition kind, int ms)
{
    m_kind = kind;
    m_transitioning = true;
    if (kind == Ppt::Transition::Dissolve) {
        m_noise.resize(32 * 18);
        for (qreal &n : m_noise) n = QRandomGenerator::global()->generateDouble();
    }
    m_trans.setDuration(ms);
    m_trans.start();
}

void SlideShow::finishTransition()
{
    m_transitioning = false;
    m_from = m_to = QPixmap();
    m_anim.startAuto();
    checkAutoAdvance();
    update();
}

void SlideShow::checkAutoAdvance()
{
    const Slide &s = m_deck.slides.at(m_index);
    if (s.advanceAfterMs > 0 && !m_transitioning && !m_anim.running()) m_autoTimer.start(s.advanceAfterMs);
}

void SlideShow::advance()
{
    if (m_transitioning) { m_trans.stop(); finishTransition(); return; }
    if (m_anim.running()) { m_anim.finishAll(); return; }
    if (m_anim.hasMoreSteps()) { m_anim.nextStep(); return; }
    if (m_index + 1 < m_deck.slides.size()) goTo(m_index + 1, true);
    else close(); // end of show
}

void SlideShow::back()
{
    if (m_transitioning) { m_trans.stop(); finishTransition(); }
    if (m_index > 0) { goTo(m_index - 1, false); m_anim.finishAll(); } // show the previous slide fully revealed
}

void SlideShow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    const QRectF t = targetRect();
    if (m_transitioning) { paintTransition(p, t); return; }
    SlidePainter::paintSlide(&p, m_deck.slides.at(m_index), t, &m_anim.progress());
}

void SlideShow::paintTransition(QPainter &p, const QRectF &t)
{
    using T = Ppt::Transition;
    const qreal v = qBound(0.0, m_trans.currentValue().toReal(), 1.0);
    p.setClipRect(t);
    const QPointF o = t.topLeft();
    switch (m_kind) {
    case T::PushLeft:
        p.drawPixmap(o + QPointF(-t.width() * v, 0), m_from);
        p.drawPixmap(o + QPointF(t.width() * (1 - v), 0), m_to);
        break;
    case T::PushUp:
        p.drawPixmap(o + QPointF(0, -t.height() * v), m_from);
        p.drawPixmap(o + QPointF(0, t.height() * (1 - v)), m_to);
        break;
    case T::Cover:
        p.drawPixmap(o, m_from);
        p.drawPixmap(o + QPointF(t.width() * (1 - v), 0), m_to);
        break;
    case T::Wipe:
        p.drawPixmap(o, m_from);
        p.setClipRect(QRectF(t.left(), t.top(), t.width() * v, t.height()));
        p.drawPixmap(o, m_to);
        break;
    case T::Zoom: {
        p.drawPixmap(o, m_from);
        const qreal s = 0.3 + 0.7 * v;
        p.setOpacity(v);
        p.translate(t.center());
        p.scale(s, s);
        p.translate(-t.center());
        p.drawPixmap(o, m_to);
        break;
    }
    case T::Dissolve: {
        p.drawPixmap(o, m_from);
        const qreal bw = t.width() / 32, bh = t.height() / 18;
        for (int r = 0; r < 18; ++r)
            for (int c = 0; c < 32; ++c) {
                if (m_noise.value(r * 32 + c) > v) continue;
                const QRectF block(t.left() + c * bw, t.top() + r * bh, bw + 1, bh + 1);
                p.setClipRect(block.intersected(t));
                p.drawPixmap(o, m_to);
            }
        break;
    }
    case T::Fade:
    default:
        p.drawPixmap(o, m_from);
        p.setOpacity(v);
        p.drawPixmap(o, m_to);
        break;
    }
}

void SlideShow::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_PageDown: case Qt::Key_Space: case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_N:
        advance(); break;
    case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp: case Qt::Key_Backspace: case Qt::Key_P:
        back(); break;
    case Qt::Key_Home: goTo(0, false); m_anim.finishAll(); break;
    case Qt::Key_End: goTo(int(m_deck.slides.size()) - 1, false); m_anim.finishAll(); break;
    case Qt::Key_Escape: case Qt::Key_Q: close(); break;
    default: QWidget::keyPressEvent(e);
    }
}

void SlideShow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) advance();
    else if (e->button() == Qt::RightButton) back();
}

void SlideShow::mouseMoveEvent(QMouseEvent *)
{
    setCursor(Qt::ArrowCursor);
    m_cursorTimer.start();
}

void SlideShow::closeEvent(QCloseEvent *e)
{
    m_autoTimer.stop();
    m_trans.stop();
    emit finished(m_index);
    e->accept();
}

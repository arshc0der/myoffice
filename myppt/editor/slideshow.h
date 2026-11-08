#pragma once
#include <QElapsedTimer>
#include <QList>
#include <QPixmap>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

#include "myppt/document/slidemodel.h"

// Plays the object animations of one slide. Animations are grouped into click steps:
// an "On click" animation starts a new step; "With previous" / "After previous" animations join the step.
// If the first animation is not "On click", the first step starts automatically when the slide appears.
class SlideAnimator : public QObject
{
    Q_OBJECT
public:
    explicit SlideAnimator(QObject *parent = nullptr);
    void setSlide(const Slide *slide);      // resets to the initial state (animated objects hidden)
    void startAuto();                       // starts the automatic first step, if any
    bool hasMoreSteps() const { return m_next < m_steps.size(); }
    bool running() const { return !m_runs.isEmpty(); }
    void nextStep();
    void finishAll();                       // jump to the fully revealed state
    const QVector<qreal> &progress() const { return m_progress; }

signals:
    void updated();
    void finished(); // the running animations completed

private:
    struct Entry { int obj; qint64 start; int dur; };
    struct Run { int obj; qint64 start; int dur; };
    void startStep(int index);
    void tick();

    QList<QList<Entry>> m_steps;
    QList<Run> m_runs;
    QVector<qreal> m_progress;
    QElapsedTimer m_clock;
    QTimer m_timer;
    int m_next = 0;
    bool m_autoFirst = false;
};

// Full-screen presenter. Click / Space / arrow keys advance, Esc exits. Closes itself (WA_DeleteOnClose).
class SlideShow : public QWidget
{
    Q_OBJECT
public:
    SlideShow(const Deck &deck, int startIndex);
    void run();

signals:
    void finished(int lastIndex);

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private:
    QRectF targetRect() const;
    QPixmap renderCurrent(bool final) const;
    void goTo(int index, bool withTransition);
    void advance();
    void back();
    void beginTransition(Ppt::Transition kind, int ms);
    void finishTransition();
    void checkAutoAdvance();
    void paintTransition(QPainter &p, const QRectF &t);

    Deck m_deck;
    int m_index = 0;
    bool m_ended = false, m_transitioning = false;
    SlideAnimator m_anim;
    QTimer m_autoTimer, m_cursorTimer;
    QVariantAnimation m_trans;
    Ppt::Transition m_kind = Ppt::Transition::None;
    QPixmap m_from, m_to;
    QVector<qreal> m_noise;
};

#include "fadestack.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#include "core/designtokens.h"
#include "core/thememanager.h"

void FadeStack::fadeTo(QWidget *page)
{
    if (currentWidget() == page) return;
    setCurrentWidget(page);
    const int ms = ThemeManager::instance().duration(Design::Duration::normal);
    if (ms == 0) return;

    auto *fx = new QGraphicsOpacityEffect(page);
    page->setGraphicsEffect(fx);
    auto *anim = new QPropertyAnimation(fx, "opacity", page);
    anim->setDuration(ms);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    // Remove the effect afterwards: effects force offscreen rendering, which we only want while fading.
    connect(anim, &QPropertyAnimation::finished, page, [page] { page->setGraphicsEffect(nullptr); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

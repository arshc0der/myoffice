#pragma once
#include <QStackedWidget>

// QStackedWidget that cross-fades the incoming page (instant when reduced motion is on).
class FadeStack : public QStackedWidget
{
    Q_OBJECT
public:
    using QStackedWidget::QStackedWidget;
    void fadeTo(QWidget *page);
};

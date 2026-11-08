#pragma once
#include <QPalette>
#include <QWidget>

// Sets the text colour of a QLabel/QWidget without using style sheets.
inline void setTextColor(QWidget *w, const QColor &c)
{
    QPalette p = w->palette();
    p.setColor(QPalette::WindowText, c);
    p.setColor(QPalette::ButtonText, c);
    w->setPalette(p);
}

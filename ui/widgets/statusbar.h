#pragma once
#include <QWidget>

class QHBoxLayout;
class QLabel;

// Application-coloured status bar: text labels on the left, controls (e.g. zoom) on the right.
class OfficeStatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit OfficeStatusBar(QWidget *parent = nullptr);
    QLabel *addLeftText();
    void addRight(QWidget *w);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void recolor();
    QHBoxLayout *m_left, *m_right;
    QList<QLabel *> m_labels;
};

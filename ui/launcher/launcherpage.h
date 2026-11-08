#pragma once
#include <QWidget>

class QLabel;

class LauncherPage : public QWidget
{
    Q_OBJECT
public:
    explicit LauncherPage(QWidget *parent = nullptr);

private:
    void refreshTheme();
    QLabel *m_heading, *m_sub, *m_footer;
};

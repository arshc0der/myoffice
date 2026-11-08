#pragma once
#include <QWidget>

class IconButton;
class QLabel;

// Custom title bar: branding, back navigation, theme toggle and window controls.
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);
    void setContext(const QString &appName); // empty = launcher
    void setMaximizedState(bool maximized);

signals:
    void backRequested();
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void refreshTheme();

    IconButton *m_back, *m_theme, *m_min, *m_max, *m_close;
    QLabel *m_logo, *m_brand, *m_context;
};

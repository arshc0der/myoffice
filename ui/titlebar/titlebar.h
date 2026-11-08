#pragma once
#include <QWidget>

class IconButton;
class QLabel;

// Custom title bar: branding, back navigation, theme toggle and window controls.
// Takes the active application's colour while an app is open.
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);
    void setContext(const QString &appName); // empty = launcher
    void setMaximizedState(bool maximized);
    void setRadius(int r) { m_radius = r; update(); }

signals:
    void backRequested();
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();
    void aboutRequested();
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    void refreshTheme();
    IconButton *m_back, *m_help, *m_settings, *m_theme, *m_min, *m_max, *m_close;
    QLabel *m_logo, *m_brand, *m_context;
    int m_radius = 12;
};

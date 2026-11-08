#pragma once
#include <QWidget>

#include "core/applicationmanager.h"

class TitleBar;
class FadeStack;
class LauncherPage;
class WorkspacePage;
class Surface;

// Frameless, translucent top-level window. The outer margin hosts the drop shadow and the
// resize hit-zones; the inner Surface paints the rounded window body.
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    MainWindow();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { unsetCursor(); }
    void changeEvent(QEvent *e) override;

private:
    Qt::Edges edgesAt(const QPoint &p) const;
    void onNavigationChanged();
    void toggleMaximize();
    void applyWindowState();

    Surface *m_surface;
    TitleBar *m_titleBar;
    FadeStack *m_stack;
    LauncherPage *m_launcher;
    WorkspacePage *m_workspace;
};

#pragma once
#include <QWidget>

#include "core/applicationmanager.h"

class ExcelWorkspace;
class FadeStack;
class LauncherPage;
class Surface;
class TitleBar;
class WordWorkspace;
class PptWorkspace;

// Frameless, translucent top-level window hosting the launcher and the application workspaces.
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    MainWindow();
    void openFiles(const QStringList &paths); // routes each file to the right app (view mode per user setting)
    void checkRecovery();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { unsetCursor(); }
    void changeEvent(QEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    Qt::Edges edgesAt(const QPoint &p) const;
    void onNavigationChanged();
    void toggleMaximize();
    void applyWindowState();
    void openFileDialog();
    WordWorkspace *ensureWord();
    ExcelWorkspace *ensureExcel();
    PptWorkspace *ensurePpt();

    Surface *m_surface;
    TitleBar *m_titleBar;
    FadeStack *m_stack;
    LauncherPage *m_launcher;
    WordWorkspace *m_word = nullptr;
    ExcelWorkspace *m_excel = nullptr;
    PptWorkspace *m_ppt = nullptr;
};

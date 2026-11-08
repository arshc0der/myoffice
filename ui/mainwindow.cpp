#include "mainwindow.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWindow>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "launcher/launcherpage.h"
#include "launcher/workspacepage.h"
#include "titlebar/titlebar.h"
#include "widgets/fadestack.h"

// Inner rounded body of the window.
class Surface : public QWidget
{
public:
    explicit Surface(QWidget *parent) : QWidget(parent)
    {
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
    }
    void setRadius(int r) { m_radius = r; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Palette &pal = ThemeManager::instance().palette();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(pal.window);
        p.setPen(QPen(pal.border, 1));
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.drawRoundedRect(r, m_radius, m_radius);
    }

private:
    int m_radius = Design::Window::cornerRadius;
};

using Design::Window::shadowMargin;

MainWindow::MainWindow()
{
    setWindowTitle(QStringLiteral("MyOffice"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setMinimumSize(900, 600);
    resize(1120, 720);

    m_surface = new Surface(this);
    m_titleBar = new TitleBar(m_surface);
    m_stack = new FadeStack(m_surface);
    m_launcher = new LauncherPage(m_stack);
    m_workspace = new WorkspacePage(m_stack);
    m_stack->addWidget(m_launcher);
    m_stack->addWidget(m_workspace);

    auto *inner = new QVBoxLayout(m_surface);
    inner->setContentsMargins(1, 1, 1, 1);
    inner->setSpacing(0);
    inner->addWidget(m_titleBar);
    inner->addWidget(m_stack, 1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(shadowMargin, shadowMargin, shadowMargin, shadowMargin);
    outer->addWidget(m_surface);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &MainWindow::toggleMaximize);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::backRequested, &ApplicationManager::instance(), &ApplicationManager::goHome);
    connect(&ApplicationManager::instance(), &ApplicationManager::navigationChanged, this, &MainWindow::onNavigationChanged);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));

    auto *home = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(home, &QShortcut::activated, &ApplicationManager::instance(), &ApplicationManager::goHome);
    auto *theme = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")), this);
    connect(theme, &QShortcut::activated, &ThemeManager::instance(), &ThemeManager::toggleLightDark);
}

void MainWindow::onNavigationChanged()
{
    auto &mgr = ApplicationManager::instance();
    if (mgr.atHome()) {
        m_titleBar->setContext({});
        m_stack->fadeTo(m_launcher);
    } else {
        m_workspace->showApp(mgr.currentApp());
        m_titleBar->setContext(ApplicationManager::info(mgr.currentApp()).name);
        m_stack->fadeTo(m_workspace);
    }
}

void MainWindow::toggleMaximize() { isMaximized() ? showNormal() : showMaximized(); }

void MainWindow::applyWindowState()
{
    const bool flat = isMaximized() || isFullScreen();
    layout()->setContentsMargins(flat ? 0 : shadowMargin, flat ? 0 : shadowMargin,
                                 flat ? 0 : shadowMargin, flat ? 0 : shadowMargin);
    m_surface->setRadius(flat ? 0 : Design::Window::cornerRadius);
    m_titleBar->setMaximizedState(isMaximized());
    update();
}

void MainWindow::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange) applyWindowState();
}

void MainWindow::paintEvent(QPaintEvent *)
{
    if (isMaximized() || isFullScreen()) return;
    // Layered translucent rounded rects approximate a blurred drop shadow without an offscreen pass.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const bool dark = ThemeManager::instance().isDark();
    const QRectF body = QRectF(rect()).adjusted(shadowMargin, shadowMargin, -shadowMargin, -shadowMargin);
    for (int i = shadowMargin - 1; i >= 1; --i) {
        const int alpha = int((dark ? 60 : 30) * (1.0 - double(i) / shadowMargin) * 0.35);
        p.setBrush(QColor(0, 0, 0, alpha));
        p.drawRoundedRect(body.adjusted(-i, -i + 2, i, i + 2), Design::Window::cornerRadius + i, Design::Window::cornerRadius + i);
    }
}

Qt::Edges MainWindow::edgesAt(const QPoint &p) const
{
    if (isMaximized() || isFullScreen()) return {};
    Qt::Edges e;
    if (p.x() < shadowMargin) e |= Qt::LeftEdge;
    if (p.x() >= width() - shadowMargin) e |= Qt::RightEdge;
    if (p.y() < shadowMargin) e |= Qt::TopEdge;
    if (p.y() >= height() - shadowMargin) e |= Qt::BottomEdge;
    return e;
}

void MainWindow::mouseMoveEvent(QMouseEvent *e)
{
    const Qt::Edges edges = edgesAt(e->position().toPoint());
    if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) setCursor(Qt::SizeFDiagCursor);
    else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) setCursor(Qt::SizeBDiagCursor);
    else if (edges & (Qt::LeftEdge | Qt::RightEdge)) setCursor(Qt::SizeHorCursor);
    else if (edges & (Qt::TopEdge | Qt::BottomEdge)) setCursor(Qt::SizeVerCursor);
    else unsetCursor();
}

void MainWindow::mousePressEvent(QMouseEvent *e)
{
    const Qt::Edges edges = edgesAt(e->position().toPoint());
    if (e->button() == Qt::LeftButton && edges && windowHandle()) windowHandle()->startSystemResize(edges);
}

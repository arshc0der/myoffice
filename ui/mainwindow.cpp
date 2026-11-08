#include "mainwindow.h"

#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QShortcut>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#include "core/autosavemanager.h"
#include "core/designtokens.h"
#include "core/filemanager.h"
#include "core/thememanager.h"
#include "launcher/launcherpage.h"
#include "myexcel/excelworkspace.h"
#include "dialogs/appdialogs.h"
#include "myppt/editor/pptworkspace.h"
#include "myword/document/worddocument.h"
#include "myword/editor/wordworkspace.h"
#include "titlebar/titlebar.h"
#include "widgets/fadestack.h"
#include "widgets/toast.h"

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
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), m_radius, m_radius);
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
    setAcceptDrops(true);
    setMinimumSize(1100, 700);
    resize(1280, 800);

    m_surface = new Surface(this);
    m_titleBar = new TitleBar(m_surface);
    m_stack = new FadeStack(m_surface);
    m_launcher = new LauncherPage(m_stack);
    m_stack->addWidget(m_launcher);

    auto *inner = new QVBoxLayout(m_surface);
    inner->setContentsMargins(1, 1, 1, 1);
    inner->setSpacing(0);
    inner->addWidget(m_titleBar);
    inner->addWidget(m_stack, 1);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(shadowMargin, shadowMargin, shadowMargin, shadowMargin);
    outer->addWidget(m_surface);

    auto &apps = ApplicationManager::instance();
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &MainWindow::toggleMaximize);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
    connect(m_titleBar, &TitleBar::backRequested, &apps, &ApplicationManager::goHome);
    connect(m_titleBar, &TitleBar::aboutRequested, this, [this] { AppDialogs::showAbout(this); });
    connect(m_titleBar, &TitleBar::settingsRequested, this, [this] { AppDialogs::showSettings(this); });
    connect(&apps, &ApplicationManager::navigationChanged, this, &MainWindow::onNavigationChanged);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
    connect(m_launcher, &LauncherPage::newRequested, this, [this](ApplicationManager::AppId id) { ApplicationManager::instance().openApp(id); });
    connect(m_launcher, &LauncherPage::openFileRequested, this, &MainWindow::openFileDialog);
    connect(m_launcher, &LauncherPage::openPathRequested, this, [this](const QString &p) { openFiles({ p }); });

    auto *about = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(about, &QShortcut::activated, this, [this] { AppDialogs::showAbout(this); });
    auto *theme = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")), this);
    connect(theme, &QShortcut::activated, &ThemeManager::instance(), &ThemeManager::toggleLightDark);
}

WordWorkspace *MainWindow::ensureWord()
{
    if (!m_word) {
        m_word = new WordWorkspace(m_stack);
        m_stack->addWidget(m_word);
        connect(m_word, &Workspace::allDocumentsClosed, &ApplicationManager::instance(), &ApplicationManager::goHome);
        connect(m_word, &Workspace::openFilesRequested, this, &MainWindow::openFiles);
    }
    return m_word;
}

ExcelWorkspace *MainWindow::ensureExcel()
{
    if (!m_excel) {
        m_excel = new ExcelWorkspace(m_stack);
        m_stack->addWidget(m_excel);
        connect(m_excel, &Workspace::allDocumentsClosed, &ApplicationManager::instance(), &ApplicationManager::goHome);
        connect(m_excel, &Workspace::openFilesRequested, this, &MainWindow::openFiles);
    }
    return m_excel;
}

PptWorkspace *MainWindow::ensurePpt()
{
    if (!m_ppt) {
        m_ppt = new PptWorkspace(m_stack);
        m_stack->addWidget(m_ppt);
        connect(m_ppt, &Workspace::allDocumentsClosed, &ApplicationManager::instance(), &ApplicationManager::goHome);
        connect(m_ppt, &Workspace::openFilesRequested, this, &MainWindow::openFiles);
    }
    return m_ppt;
}

void MainWindow::onNavigationChanged()
{
    auto &mgr = ApplicationManager::instance();
    if (mgr.atHome()) {
        ThemeManager::instance().setActiveApp(std::nullopt);
        m_titleBar->setContext({});
        m_stack->fadeTo(m_launcher);
        return;
    }
    const auto id = mgr.currentApp();
    ThemeManager::instance().setActiveApp(id);
    m_titleBar->setContext(ApplicationManager::info(id).name);
    QWidget *page = m_launcher;
    if (id == ApplicationManager::AppId::Word) {
        ensureWord();
        if (m_word->documentCount() == 0) m_word->newDocument();
        page = m_word;
    } else if (id == ApplicationManager::AppId::Excel) {
        ensureExcel();
        if (m_excel->documentCount() == 0) m_excel->newDocument();
        page = m_excel;
    } else {
        ensurePpt();
        if (m_ppt->documentCount() == 0) m_ppt->newDocument();
        page = m_ppt;
    }
    m_stack->fadeTo(page);
}

void MainWindow::openFileDialog()
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open"), dir, FileManager::allSupportedFilter());
    if (files.isEmpty()) return;
    QSettings().setValue(QStringLiteral("files/lastDir"), QFileInfo(files.first()).absolutePath());
    openFiles(files);
}

void MainWindow::openFiles(const QStringList &paths)
{
    const bool view = QSettings().value(QStringLiteral("files/openInViewMode"), true).toBool();
    std::optional<ApplicationManager::AppId> target;
    for (const QString &p : paths) {
        const auto route = FileManager::route(p);
        if (!route.ok) {
            Toast::show(this, QFileInfo(p).fileName() + QStringLiteral(": ") + route.error, Toast::Error, 5000);
            FileManager::instance().removeRecent(p);
            continue;
        }
        if (route.app == ApplicationManager::AppId::Word) { ensureWord()->openFile(p, view); target = route.app; }
        else if (route.app == ApplicationManager::AppId::Excel) { ensureExcel()->openFile(p, view); target = route.app; }
        else { ensurePpt()->openFile(p, view); target = route.app; }
    }
    if (target) ApplicationManager::instance().openApp(*target);
}

void MainWindow::checkRecovery()
{
    QStringList ids;
    for (const QString &id : AutosaveManager::instance().recoverableIds()) if (id.startsWith(QLatin1String("word-"))) ids << id;
    if (ids.isEmpty()) return;
    const auto r = QMessageBox::question(this, tr("MyOffice"), tr("MyOffice found %n unsaved document(s) from a previous session. Recover them?", "", ids.size()));
    for (const QString &id : ids) {
        if (r == QMessageBox::Yes) {
            auto *d = new WordDocument;
            ensureWord()->addDocument(d);
            d->loadRecovery(AutosaveManager::instance().readRecovery(id), id);
        } else {
            AutosaveManager::instance().discardRecovery(id);
        }
    }
    if (r == QMessageBox::Yes) ApplicationManager::instance().openApp(ApplicationManager::AppId::Word);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    for (Workspace *w : { static_cast<Workspace *>(m_word), static_cast<Workspace *>(m_excel), static_cast<Workspace *>(m_ppt) })
        if (w && !w->requestCloseAll()) { e->ignore(); return; }
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void MainWindow::dropEvent(QDropEvent *e)
{
    QStringList paths;
    for (const QUrl &u : e->mimeData()->urls()) if (u.isLocalFile()) paths << u.toLocalFile();
    if (!paths.isEmpty()) openFiles(paths);
}

void MainWindow::toggleMaximize() { isMaximized() ? showNormal() : showMaximized(); }

void MainWindow::applyWindowState()
{
    const bool flat = isMaximized() || isFullScreen();
    const int m = flat ? 0 : shadowMargin;
    layout()->setContentsMargins(m, m, m, m);
    const int radius = flat ? 0 : Design::Window::cornerRadius;
    m_surface->setRadius(radius);
    m_titleBar->setRadius(radius);
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
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const bool dark = ThemeManager::instance().isDark();
    const QRectF body = QRectF(rect()).adjusted(shadowMargin, shadowMargin, -shadowMargin, -shadowMargin);
    for (int i = shadowMargin - 1; i >= 1; --i) {
        p.setBrush(QColor(0, 0, 0, int((dark ? 60 : 30) * (1.0 - double(i) / shadowMargin) * 0.35)));
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

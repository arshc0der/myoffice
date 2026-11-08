#include "workspace.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/filemanager.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"
#include "ui/ribbon/ribbon.h"
#include "ui/widgets/documenttabbar.h"
#include "ui/widgets/fadestack.h"
#include "ui/widgets/pillbutton.h"
#include "ui/widgets/statusbar.h"
#include "ui/widgets/themeutil.h"
#include "ui/widgets/toast.h"

// Protected-view style banner shown while a document is read-only.
class ViewBanner : public QWidget
{
public:
    ViewBanner(QWidget *parent) : QWidget(parent)
    {
        setFixedHeight(44);
        auto *row = new QHBoxLayout(this);
        row->setContentsMargins(52, 0, 14, 0);
        m_label = new QLabel(tr("VIEW MODE  —  This file is open read-only. Enable editing to make changes."), this);
        m_label->setFont(ThemeManager::instance().uiFont(Design::Type::body));
        m_button = new PillButton(tr("Enable editing"), QString(), true, this);
        row->addWidget(m_label, 1);
        row->addWidget(m_button);
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { setTextColor(m_label, ThemeManager::instance().palette().text); update(); });
        setTextColor(m_label, ThemeManager::instance().palette().text);
    }
    PillButton *button() const { return m_button; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Palette &pal = ThemeManager::instance().palette();
        QPainter p(this);
        p.fillRect(rect(), Design::mix(pal.surface, pal.accent, 0.12));
        p.setPen(pal.border);
        p.drawLine(0, height() - 1, width(), height() - 1);
        p.drawPixmap(QPointF(18, (height() - 22) / 2.0), Icons::pixmap(QStringLiteral("shield"), pal.accentDeep, 22, devicePixelRatioF()));
    }

private:
    QLabel *m_label;
    PillButton *m_button;
};

Workspace::Workspace(ApplicationManager::AppId id, QWidget *parent) : QWidget(parent), m_id(id)
{
    m_tabs = new DocumentTabBar(this);
    m_ribbon = new Ribbon(this);
    m_banner = new ViewBanner(this);
    m_banner->hide();
    m_stack = new FadeStack(this);
    m_status = new OfficeStatusBar(this);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addWidget(m_tabs);
    col->addWidget(m_ribbon);
    col->addWidget(m_banner);
    col->addWidget(m_stack, 1);
    col->addWidget(m_status);

    m_actNew = makeAction(tr("New"), QStringLiteral("new-file"), QKeySequence::New);
    m_actOpen = makeAction(tr("Open"), QStringLiteral("folder"), QKeySequence::Open);
    m_actSave = makeAction(tr("Save"), QStringLiteral("save"), QKeySequence::Save);
    m_actSaveAs = makeAction(tr("Save As"), QStringLiteral("save-as"), QKeySequence::SaveAs);
    m_actClose = makeAction(tr("Close"), QStringLiteral("close"), QKeySequence::Close);
    connect(m_actNew, &QAction::triggered, this, &Workspace::newDocument);
    connect(m_actOpen, &QAction::triggered, this, &Workspace::openFileDialog);
    connect(m_actSave, &QAction::triggered, this, [this] { if (auto *d = currentDocument()) d->save(); });
    connect(m_actSaveAs, &QAction::triggered, this, [this] { if (auto *d = currentDocument()) d->saveAs(); });
    connect(m_actClose, &QAction::triggered, this, [this] { closeDocumentAt(m_tabs->currentIndex()); });

    connect(m_tabs, &DocumentTabBar::currentChanged, this, &Workspace::onTabChanged);
    connect(m_tabs, &DocumentTabBar::closeRequested, this, [this](int i) { closeDocumentAt(i); });
    connect(m_tabs, &DocumentTabBar::newRequested, this, &Workspace::newDocument);
    connect(m_tabs, &DocumentTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *w = m_stack->widget(from);
        m_stack->removeWidget(w);
        m_stack->insertWidget(to, w);
        m_stack->setCurrentWidget(w);
    });
    connect(m_banner->button(), &PillButton::clicked, this, [this] { if (auto *d = currentDocument()) d->setViewMode(false); });
}

QAction *Workspace::makeAction(const QString &text, const QString &icon, const QKeySequence &sc, bool checkable, bool edits)
{
    auto *a = new QAction(text, this);
    a->setProperty("iconName", icon);
    a->setCheckable(checkable);
    if (!sc.isEmpty()) { a->setShortcut(sc); a->setShortcutContext(Qt::WindowShortcut); }
    addAction(a);
    if (edits) m_editActions << a;
    return a;
}

int Workspace::documentCount() const { return m_stack->count(); }
DocumentView *Workspace::docAt(int i) const { return qobject_cast<DocumentView *>(m_stack->widget(i)); }
DocumentView *Workspace::currentDocument() const { return docAt(m_tabs->currentIndex()); }
QString Workspace::openDialogFilter() const { return FileManager::allSupportedFilter(); }

DocumentView *Workspace::addDocument(DocumentView *doc)
{
    m_stack->addWidget(doc);
    const int idx = m_tabs->addTab(doc->displayName());
    auto indexOfDoc = [this, doc] { for (int i = 0; i < m_stack->count(); ++i) if (m_stack->widget(i) == doc) return i; return -1; };
    connect(doc, &DocumentView::titleChanged, this, [this, doc, indexOfDoc] { m_tabs->setTabTitle(indexOfDoc(), doc->displayName()); });
    connect(doc, &DocumentView::modifiedChanged, this, [this, doc, indexOfDoc] { m_tabs->setTabModified(indexOfDoc(), doc->isModified()); });
    connect(doc, &DocumentView::viewModeChanged, this, [this, doc] { if (doc == currentDocument()) { updateBanner(); updateEditability(); } });
    connect(doc, &DocumentView::stateChanged, this, [this, doc] { if (doc == currentDocument()) syncState(); });
    connect(doc, &DocumentView::statusChanged, this, [this, doc] { if (doc == currentDocument()) syncStatus(); });
    connect(doc, &DocumentView::message, this, [this](const QString &t, int k) { Toast::show(this, t, k); });
    m_tabs->setCurrentIndex(idx);
    return doc;
}

void Workspace::newDocument()
{
    DocumentView *d = createBlankDocument();
    d->setViewMode(false);
    addDocument(d);
}

void Workspace::openFile(const QString &path, bool viewMode)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < m_stack->count(); ++i)
        if (docAt(i) && docAt(i)->filePath() == abs) { m_tabs->setCurrentIndex(i); return; }
    DocumentView *d = createBlankDocument();
    addDocument(d);
    connect(d, &DocumentView::loadFinished, this, [this, d, abs](bool ok, const QString &err) {
        if (ok) { FileManager::instance().addRecent(abs); return; }
        Toast::show(this, tr("Could not open %1: %2").arg(QFileInfo(abs).fileName(), err), Toast::Error, 5000);
        FileManager::instance().removeRecent(abs);
        for (int i = 0; i < m_stack->count(); ++i) if (docAt(i) == d) { d->setProperty("discard", true); closeDocumentAt(i); break; }
    });
    d->load(abs, viewMode);
}

void Workspace::openFileDialog()
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open"), dir, openDialogFilter());
    if (files.isEmpty()) return;
    QSettings().setValue(QStringLiteral("files/lastDir"), QFileInfo(files.first()).absolutePath());
    emit openFilesRequested(files);
}

void Workspace::onTabChanged(int i)
{
    DocumentView *d = docAt(i);
    if (!d) return;
    m_stack->fadeTo(d);
    updateBanner();
    updateEditability();
    d->activated();
    syncState();
    syncStatus();
}

void Workspace::updateBanner()
{
    DocumentView *d = currentDocument();
    m_banner->setVisible(d && d->isViewMode());
}

void Workspace::updateEditability()
{
    DocumentView *d = currentDocument();
    const bool editable = d && !d->isViewMode();
    for (QAction *a : m_editActions) a->setEnabled(editable);
    if (editable) syncState();
}

bool Workspace::closeDocumentAt(int i)
{
    DocumentView *d = docAt(i);
    if (!d) return false;
    if (d->isModified() && !d->property("discard").toBool()) {
        m_tabs->setCurrentIndex(i);
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("MyOffice"));
        box.setText(tr("Do you want to save changes to \"%1\"?").arg(d->displayName()));
        box.setInformativeText(tr("Your changes will be lost if you don't save them."));
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Save);
        const int r = box.exec();
        if (r == QMessageBox::Cancel) return false;
        if (r == QMessageBox::Save && !d->save()) return false;
    }
    d->aboutToClose();
    m_stack->removeWidget(d);
    m_tabs->removeTab(i);
    d->deleteLater();
    if (m_tabs->count() == 0 && !m_closingAll) emit allDocumentsClosed();
    return true;
}

bool Workspace::requestCloseAll()
{
    m_closingAll = true;
    bool ok = true;
    for (int i = m_stack->count() - 1; i >= 0 && ok; --i) ok = closeDocumentAt(i);
    m_closingAll = false;
    return ok;
}

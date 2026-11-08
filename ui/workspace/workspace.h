#pragma once
#include <QWidget>

#include "core/applicationmanager.h"

class DocumentTabBar;
class FadeStack;
class OfficeStatusBar;
class Ribbon;
class ViewBanner;
class QAction;

// One open document inside a workspace tab.
class DocumentView : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;
    virtual QString displayName() const = 0;
    virtual QString filePath() const = 0;
    virtual bool isModified() const = 0;
    virtual bool save() = 0;                 // false = cancelled or failed
    virtual bool saveAs() = 0;
    virtual bool isViewMode() const = 0;
    virtual void setViewMode(bool on) = 0;
    virtual void load(const QString &path, bool viewMode) = 0; // async; emits loadFinished
    virtual void aboutToClose() {}
    virtual void activated() {}

signals:
    void titleChanged();
    void modifiedChanged();
    void viewModeChanged();
    void statusChanged();
    void stateChanged();
    void loadFinished(bool ok, const QString &error);
    void message(const QString &text, int kind); // Toast::Kind
};

// Shared shell for each application: document tabs, ribbon, view-mode banner, content, status bar.
class Workspace : public QWidget
{
    Q_OBJECT
public:
    Workspace(ApplicationManager::AppId id, QWidget *parent = nullptr);
    ApplicationManager::AppId appId() const { return m_id; }
    int documentCount() const;
    DocumentView *currentDocument() const;
    DocumentView *addDocument(DocumentView *doc);
    void newDocument();
    void openFile(const QString &path, bool viewMode);
    bool requestCloseAll();

signals:
    void allDocumentsClosed();
    void openFilesRequested(const QStringList &paths);

protected:
    virtual DocumentView *createBlankDocument() = 0;
    virtual void syncState() {}      // refresh ribbon state from current document
    virtual void syncStatus() {}     // refresh status bar from current document
    virtual QString openDialogFilter() const;

    QAction *makeAction(const QString &text, const QString &icon, const QKeySequence &shortcut = {},
                        bool checkable = false, bool editsDocument = false);
    DocumentView *docAt(int i) const;
    bool closeDocumentAt(int i);
    void openFileDialog();
    void updateEditability();

    Ribbon *m_ribbon;
    OfficeStatusBar *m_status;
    QAction *m_actNew, *m_actOpen, *m_actSave, *m_actSaveAs, *m_actClose;
    QList<QAction *> m_editActions;

private:
    void onTabChanged(int i);
    void updateBanner();

    ApplicationManager::AppId m_id;
    DocumentTabBar *m_tabs;
    FadeStack *m_stack;
    ViewBanner *m_banner;
    bool m_closingAll = false;
};

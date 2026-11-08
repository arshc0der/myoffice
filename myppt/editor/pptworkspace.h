#pragma once
#include "ui/workspace/workspace.h"

class PptDocument;
class QComboBox;
class QLabel;
class RibbonButton;
class RibbonGroup;
class QMenu;

// MyPowerPoint ribbon (File, Home, Insert, Design, Transitions, Animations, Slide Show, View) and status bar.
class PptWorkspace : public Workspace
{
    Q_OBJECT
public:
    explicit PptWorkspace(QWidget *parent = nullptr);
    PptDocument *pptDoc() const;

protected:
    DocumentView *createBlankDocument() override;
    void syncState() override;
    void syncStatus() override;

private:
    void buildFile();
    void buildHome();
    void buildInsert();
    void buildDesign();
    void buildTransitions();
    void buildAnimations();
    void buildShow();
    void buildView();
    void buildStatusBar();
    QAction *editAction(const QString &text, const QString &icon, const QKeySequence &sc = {}, bool checkable = false);
    QAction *objectAction(const QString &text, const QString &icon, const QKeySequence &sc = {}, bool checkable = false);
    RibbonButton *iconButton(RibbonGroup *g, QAction *a, QMenu *menu = nullptr);

    QList<QAction *> m_objActions; // enabled only while an object is selected (and the document is editable)
    QAction *m_bold, *m_italic, *m_underline, *m_alignL, *m_alignC, *m_alignR, *m_undo, *m_redo, *m_cut, *m_copy;
    QAction *m_animPaneAct, *m_notesAct, *m_readMode;
    QComboBox *m_size, *m_transCombo, *m_transDur, *m_advance, *m_effect, *m_trigger, *m_animDur, *m_animDelay;
    RibbonButton *m_textBtn, *m_fillBtn, *m_lineBtn;
    QColor m_textColor = QColor("#E8663A"), m_fillColor = QColor("#3B82F6"), m_lineColor = QColor("#1E40AF");
    QLabel *m_slideLabel, *m_modeLabel;
};

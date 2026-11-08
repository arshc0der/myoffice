#pragma once
#include "ui/workspace/workspace.h"

class ExcelDocument;
class QComboBox;
class QLabel;
class QSlider;
class RibbonButton;

// MyExcel ribbon (File, Home, Insert, Formulas, Data, View) and status bar.
class ExcelWorkspace : public Workspace
{
    Q_OBJECT
public:
    explicit ExcelWorkspace(QWidget *parent = nullptr);
    ExcelDocument *excelDoc() const;

protected:
    DocumentView *createBlankDocument() override;
    void syncState() override;
    void syncStatus() override;

private:
    void buildFile();
    void buildHome();
    void buildInsert();
    void buildFormulas();
    void buildData();
    void buildView();
    void buildStatusBar();
    QAction *editAction(const QString &text, const QString &icon, const QKeySequence &sc = {}, bool checkable = false);

    QAction *m_bold, *m_italic, *m_underline, *m_alignL, *m_alignC, *m_alignR, *m_undo, *m_redo, *m_cut, *m_copy, *m_grid, *m_readMode;
    QComboBox *m_numFmt;
    RibbonButton *m_colorBtn, *m_fillBtn;
    QColor m_textColor = QColor("#C0392B"), m_fillColor = QColor("#FFF2A8");
    QLabel *m_selLabel, *m_statLabel, *m_modeLabel, *m_zoomLabel;
    QSlider *m_zoomSlider;
};

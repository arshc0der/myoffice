#pragma once
#include "ui/workspace/workspace.h"

class ColorMenu;
class IconButton;
class QComboBox;
class QFontComboBox;
class QLabel;
class QSlider;
class RibbonButton;
class WordDocument;

// MyWord ribbon (File, Home, Insert, Layout, Review, View), status bar and document factory.
class WordWorkspace : public Workspace
{
    Q_OBJECT
public:
    explicit WordWorkspace(QWidget *parent = nullptr);
    WordDocument *wordDoc() const;

protected:
    DocumentView *createBlankDocument() override;
    void syncState() override;
    void syncStatus() override;

private:
    void buildFile();
    void buildHome();
    void buildInsert();
    void buildLayout();
    void buildReview();
    void buildView();
    void buildStatusBar();
    QAction *editAction(const QString &text, const QString &icon, const QKeySequence &sc = {}, bool checkable = false);
    void showWordCount();
    void insertTableDialog();

    QAction *m_bold, *m_italic, *m_underline, *m_strike, *m_bullets, *m_numbers;
    QAction *m_alignL, *m_alignC, *m_alignR, *m_alignJ, *m_undo, *m_redo, *m_cut, *m_copy, *m_pageNumbers, *m_viewMode, *m_rulerAct;
    QFontComboBox *m_fontBox;
    QComboBox *m_sizeBox, *m_styleBox;
    RibbonButton *m_colorBtn, *m_highlightBtn;
    QColor m_textColor = QColor("#C00000"), m_highlightColor = QColor("#FFFF00");
    QLabel *m_pageLabel, *m_wordsLabel, *m_modeLabel, *m_zoomLabel;
    QSlider *m_zoomSlider;
};

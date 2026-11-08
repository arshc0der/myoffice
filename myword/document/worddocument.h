#pragma once
#include <QTextBlock>
#include <QTextListFormat>
#include <QTimer>
#include <functional>

#include "myword/document/pagesetup.h"
#include "ui/workspace/workspace.h"

class FindBar;
class PageView;
class QTextEdit;

// One MyWord document: page canvas + find bar, with all editing commands the ribbon drives.
class WordDocument : public DocumentView
{
    Q_OBJECT
public:
    struct FormatState {
        bool bold = false, italic = false, underline = false, strike = false;
        QString family;
        qreal size = 11;
        Qt::Alignment align = Qt::AlignLeft;
        int listStyle = 0, heading = 0;
        bool inTable = false, hasSelection = false, canUndo = false, canRedo = false;
    };
    struct Stats { int words = 0, chars = 0, charsNoSpaces = 0, paragraphs = 0, pages = 1, page = 1; };

    explicit WordDocument(QWidget *parent = nullptr);
    ~WordDocument() override;

    // DocumentView
    QString displayName() const override;
    QString filePath() const override { return m_path; }
    bool isModified() const override;
    bool save() override;
    bool saveAs() override;
    bool isViewMode() const override { return m_viewMode; }
    void setViewMode(bool on) override;
    void load(const QString &path, bool viewMode) override;
    void aboutToClose() override;
    void activated() override;

    void loadRecovery(const QByteArray &html, const QString &id);
    bool exportTo(const QString &path);
    void exportDialog(const QString &suffix);

    QTextEdit *edit() const { return m_edit; }
    PageView *view() const { return m_view; }
    FindBar *findBar() const { return m_find; }
    FormatState formatState() const;
    Stats stats() const;
    PageSetup pageSetup() const;
    void setPageSetup(const PageSetup &ps);

    // Character formatting
    void setFontFamily(const QString &family);
    void setFontSize(qreal pt);
    void setBold(bool on);
    void setItalic(bool on);
    void setUnderline(bool on);
    void setStrike(bool on);
    void setTextColor(const QColor &c);
    void setHighlight(const QColor &c, bool clear = false);
    void clearFormatting();
    // Paragraph formatting
    void setAlignment(Qt::Alignment a);
    void toggleList(QTextListFormat::Style style);
    void changeIndent(int delta);
    void setLineSpacing(qreal percent);
    void setParagraphSpacing(qreal beforePt, qreal afterPt);
    void applyStyle(int index); // 0 Normal, 1 Title, 2-4 Heading 1-3, 5 Quote
    // Insert
    void insertTable(int rows, int cols);
    void insertImage();
    void insertLink();
    void removeLink();
    void insertPageBreak();
    void tableOp(int op); // 0 row above,1 row below,2 col left,3 col right,4 del row,5 del col,6 merge
    void setPageNumbers(bool on);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    bool writeFile(const QString &path, QString *error);
    bool saveToPath(const QString &path);
    bool parseBytes(const QString &path, const QByteArray &data, QString *error);
    void applyViewMode();
    void blocksInSelection(const std::function<void(const QTextBlock &)> &fn) const;

    QString m_path, m_untitled, m_id;
    bool m_viewMode = false;
    PageView *m_view;
    QTextEdit *m_edit;
    FindBar *m_find;
    QTimer m_statsTimer;
};

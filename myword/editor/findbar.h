#pragma once
#include <QTextDocument>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTextEdit;
class PillButton;

// Inline find & replace bar with live match highlighting.
class FindBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindBar(QTextEdit *edit, QWidget *parent = nullptr);
    void open(bool withReplace);
    void setReadOnly(bool ro);
    void findNext(bool backward = false);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void highlightAll();
    void replaceOne();
    void replaceAll();
    QTextDocument::FindFlags flags(bool backward = false) const;

    QTextEdit *m_edit;
    QLineEdit *m_find, *m_replace;
    QCheckBox *m_case, *m_words;
    QLabel *m_count;
    QWidget *m_replaceRow;
    PillButton *m_btnReplace, *m_btnReplaceAll;
    bool m_readOnly = false;
};

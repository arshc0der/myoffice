#include "findbar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QTextEdit>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "ui/widgets/iconbutton.h"
#include "ui/widgets/pillbutton.h"
#include "ui/widgets/themeutil.h"

FindBar::FindBar(QTextEdit *edit, QWidget *parent) : QWidget(parent), m_edit(edit)
{
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(12, 8, 12, 8);
    col->setSpacing(6);

    auto *row1 = new QHBoxLayout;
    m_find = new QLineEdit(this);
    m_find->setPlaceholderText(tr("Find in document"));
    m_find->setClearButtonEnabled(true);
    m_find->setMinimumWidth(260);
    m_case = new QCheckBox(tr("Match case"), this);
    m_words = new QCheckBox(tr("Whole words"), this);
    m_count = new QLabel(this);
    auto *prev = new IconButton(QStringLiteral("chevron-up"), tr("Previous match"), this);
    auto *next = new IconButton(QStringLiteral("chevron-down"), tr("Next match"), this);
    auto *close = new IconButton(QStringLiteral("close"), tr("Close find bar"), this);
    row1->addWidget(m_find);
    row1->addWidget(prev);
    row1->addWidget(next);
    row1->addWidget(m_case);
    row1->addWidget(m_words);
    row1->addWidget(m_count);
    row1->addStretch(1);
    row1->addWidget(close);

    m_replaceRow = new QWidget(this);
    auto *row2 = new QHBoxLayout(m_replaceRow);
    row2->setContentsMargins(0, 0, 0, 0);
    m_replace = new QLineEdit(m_replaceRow);
    m_replace->setPlaceholderText(tr("Replace with"));
    m_replace->setMinimumWidth(260);
    m_btnReplace = new PillButton(tr("Replace"), QString(), false, m_replaceRow);
    m_btnReplaceAll = new PillButton(tr("Replace all"), QString(), true, m_replaceRow);
    row2->addWidget(m_replace);
    row2->addWidget(m_btnReplace);
    row2->addWidget(m_btnReplaceAll);
    row2->addStretch(1);
    col->addLayout(row1);
    col->addWidget(m_replaceRow);

    connect(m_find, &QLineEdit::textChanged, this, [this] { highlightAll(); });
    connect(m_find, &QLineEdit::returnPressed, this, [this] { findNext(false); });
    connect(m_case, &QCheckBox::toggled, this, [this] { highlightAll(); });
    connect(m_words, &QCheckBox::toggled, this, [this] { highlightAll(); });
    connect(next, &IconButton::clicked, this, [this] { findNext(false); });
    connect(prev, &IconButton::clicked, this, [this] { findNext(true); });
    connect(close, &IconButton::clicked, this, [this] { hide(); m_edit->setExtraSelections({}); m_edit->setFocus(); });
    connect(m_btnReplace, &PillButton::clicked, this, &FindBar::replaceOne);
    connect(m_btnReplaceAll, &PillButton::clicked, this, &FindBar::replaceAll);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { setTextColor(m_count, ThemeManager::instance().palette().textSecondary); update(); });
    setTextColor(m_count, ThemeManager::instance().palette().textSecondary);
}

void FindBar::setReadOnly(bool ro)
{
    m_readOnly = ro;
    m_replaceRow->setEnabled(!ro);
}

void FindBar::open(bool withReplace)
{
    show();
    m_replaceRow->setVisible(withReplace);
    const QString sel = m_edit->textCursor().selectedText();
    if (!sel.isEmpty() && !sel.contains(QChar::ParagraphSeparator)) m_find->setText(sel);
    m_find->setFocus();
    m_find->selectAll();
    highlightAll();
}

QTextDocument::FindFlags FindBar::flags(bool backward) const
{
    QTextDocument::FindFlags f;
    if (backward) f |= QTextDocument::FindBackward;
    if (m_case->isChecked()) f |= QTextDocument::FindCaseSensitively;
    if (m_words->isChecked()) f |= QTextDocument::FindWholeWords;
    return f;
}

void FindBar::findNext(bool backward)
{
    const QString t = m_find->text();
    if (t.isEmpty()) return;
    if (!m_edit->find(t, flags(backward))) {
        QTextCursor c = m_edit->textCursor();
        c.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_edit->setTextCursor(c);
        m_edit->find(t, flags(backward)); // wrap around
    }
}

void FindBar::highlightAll()
{
    QList<QTextEdit::ExtraSelection> sels;
    const QString t = m_find->text();
    int n = 0;
    if (!t.isEmpty()) {
        QTextCursor c(m_edit->document());
        while (n < 5000) {
            c = m_edit->document()->find(t, c, flags());
            if (c.isNull()) break;
            QTextEdit::ExtraSelection s;
            s.cursor = c;
            s.format.setBackground(QColor(255, 226, 90));
            s.format.setForeground(Qt::black);
            sels.append(s);
            ++n;
        }
    }
    m_edit->setExtraSelections(sels);
    m_count->setText(t.isEmpty() ? QString() : (n == 0 ? tr("No matches") : tr("%1 matches").arg(n)));
}

void FindBar::replaceOne()
{
    if (m_readOnly) return;
    QTextCursor c = m_edit->textCursor();
    if (c.hasSelection() && QString::compare(c.selectedText(), m_find->text(), m_case->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0)
        c.insertText(m_replace->text());
    findNext(false);
    highlightAll();
}

void FindBar::replaceAll()
{
    if (m_readOnly || m_find->text().isEmpty()) return;
    QTextCursor c(m_edit->document());
    c.beginEditBlock(); // single undo step
    int n = 0;
    while (true) {
        c = m_edit->document()->find(m_find->text(), c, flags());
        if (c.isNull()) break;
        c.insertText(m_replace->text());
        ++n;
    }
    c.endEditBlock();
    highlightAll();
    m_count->setText(tr("%1 replaced").arg(n));
}

void FindBar::paintEvent(QPaintEvent *)
{
    const Palette &pal = ThemeManager::instance().palette();
    QPainter p(this);
    p.fillRect(rect(), pal.surface);
    p.setPen(pal.border);
    p.drawLine(0, height() - 1, width(), height() - 1);
}

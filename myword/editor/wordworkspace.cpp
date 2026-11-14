#include "wordworkspace.h"

#include <QActionGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSlider>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <QSpinBox>
#include <QTextEdit>

#include "myword/document/worddocument.h"
#include "myword/editor/findbar.h"
#include "myword/editor/pageview.h"
#include "ui/ribbon/colormenu.h"
#include "ui/ribbon/ribbon.h"
#include "ui/widgets/iconbutton.h"
#include "ui/widgets/statusbar.h"
#include "ui/widgets/themeutil.h"
#include "core/thememanager.h"

WordWorkspace::WordWorkspace(QWidget *parent) : Workspace(ApplicationManager::AppId::Word, parent)
{
    buildFile();
    buildHome();
    buildInsert();
    buildLayout();
    buildReview();
    buildView();
    buildStatusBar();
    m_ribbon->setCurrentIndex(1); // Home
}

WordDocument *WordWorkspace::wordDoc() const { return qobject_cast<WordDocument *>(currentDocument()); }
DocumentView *WordWorkspace::createBlankDocument() { return new WordDocument; }

QAction *WordWorkspace::editAction(const QString &text, const QString &icon, const QKeySequence &sc, bool checkable)
{
    return makeAction(text, icon, sc, checkable, true);
}

// ------------------------------------------------------------------ File
void WordWorkspace::buildFile()
{
    RibbonPage *page = m_ribbon->addPage(tr("File"), true);
    RibbonGroup *g = page->addGroup(tr("Document"));
    g->addLarge(m_actNew);
    g->addLarge(m_actOpen);
    g->addLarge(m_actSave);
    g->addLarge(m_actSaveAs);

    RibbonGroup *ex = page->addGroup(tr("Export"));
    const struct { const char *label, *suffix; } formats[] = { { "PDF", "pdf" }, { "Word (.docx)", "docx" }, { "Web page", "html" },
                                                               { "Markdown", "md" }, { "Plain text", "txt" }, { "OpenDocument", "odt" } };
    ex->beginColumn();
    for (int i = 0; i < 6; ++i) {
        if (i == 3) ex->beginColumn();
        QAction *a = makeAction(tr(formats[i].label), QStringLiteral("export"));
        const QString suffix = QLatin1String(formats[i].suffix);
        connect(a, &QAction::triggered, this, [this, suffix] { if (auto *d = wordDoc()) d->exportDialog(suffix); });
        ex->addSmall(a);
    }
    page->addGroup(tr("Close"))->addLarge(m_actClose);
}

// ------------------------------------------------------------------ Home
void WordWorkspace::buildHome()
{
    RibbonPage *page = m_ribbon->addPage(tr("Home"));

    m_undo = makeAction(tr("Undo"), QStringLiteral("undo"), {}, false, true);
    m_redo = makeAction(tr("Redo"), QStringLiteral("redo"), {}, false, true);
    m_cut = editAction(tr("Cut"), QStringLiteral("cut"));
    m_copy = makeAction(tr("Copy"), QStringLiteral("copy"));
    QAction *paste = editAction(tr("Paste"), QStringLiteral("paste"));
    connect(m_undo, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->edit()->undo(); });
    connect(m_redo, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->edit()->redo(); });
    connect(m_cut, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->edit()->cut(); });
    connect(m_copy, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->edit()->copy(); });
    connect(paste, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->edit()->paste(); });

    RibbonGroup *clip = page->addGroup(tr("Clipboard"));
    clip->addLarge(paste);
    clip->beginColumn();
    clip->addSmall(m_cut);
    clip->addSmall(m_copy);
    RibbonGroup *undo = page->addGroup(tr("Undo"));
    undo->beginColumn();
    undo->addSmall(m_undo);
    undo->addSmall(m_redo);

    // Font
    RibbonGroup *font = page->addGroup(tr("Font"));
    m_fontBox = new QFontComboBox;
    m_fontBox->setFixedSize(156, 26);
    m_fontBox->setEditable(false);
    m_sizeBox = new QComboBox;
    m_sizeBox->setEditable(true);
    m_sizeBox->setFixedSize(64, 26);
    for (const char *s : { "8", "9", "10", "10.5", "11", "12", "14", "16", "18", "20", "24", "28", "32", "36", "48", "72" }) m_sizeBox->addItem(QLatin1String(s));
    connect(m_fontBox, &QFontComboBox::textActivated, this, [this](const QString &f) { if (auto *d = wordDoc()) { d->setFontFamily(f); d->activated(); } });
    connect(m_sizeBox, &QComboBox::textActivated, this, [this](const QString &s) { if (auto *d = wordDoc()) { d->setFontSize(s.toDouble()); d->activated(); } });
    m_bold = editAction(tr("Bold"), QStringLiteral("bold"), QKeySequence::Bold, true);
    m_italic = editAction(tr("Italic"), QStringLiteral("italic"), QKeySequence::Italic, true);
    m_underline = editAction(tr("Underline"), QStringLiteral("underline"), QKeySequence::Underline, true);
    m_strike = editAction(tr("Strikethrough"), QStringLiteral("strikethrough"), {}, true);
    connect(m_bold, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setBold(on); });
    connect(m_italic, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setItalic(on); });
    connect(m_underline, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setUnderline(on); });
    connect(m_strike, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setStrike(on); });

    QAction *textColor = editAction(tr("Font color"), QStringLiteral("text-color"));
    QAction *highlight = editAction(tr("Highlight"), QStringLiteral("highlight"));
    QAction *clear = editAction(tr("Clear formatting"), QStringLiteral("clear-format"));
    auto *colorMenu = new ColorMenu(tr("Automatic"), this);
    auto *hiMenu = new ColorMenu(tr("No highlight"), this);
    connect(textColor, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->setTextColor(m_textColor); });
    connect(colorMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { m_textColor = c; m_colorBtn->setIndicatorColor(c); if (auto *d = wordDoc()) d->setTextColor(c); });
    connect(colorMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = wordDoc()) d->setTextColor(QColor("#1B1E27")); });
    connect(highlight, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->setHighlight(m_highlightColor); });
    connect(hiMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { m_highlightColor = c; m_highlightBtn->setIndicatorColor(c); if (auto *d = wordDoc()) d->setHighlight(c); });
    connect(hiMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = wordDoc()) d->setHighlight(QColor(), true); });
    connect(clear, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->clearFormatting(); });

    auto small = [&](QAction *a, QMenu *m = nullptr) { auto *b = new RibbonButton(a, RibbonButton::Size::SmallIcon, font); if (m) { b->setMenu(m); b->setSplit(true); b->setFixedSize(b->sizeHint()); } return b; };
    m_colorBtn = small(textColor, colorMenu);
    m_highlightBtn = small(highlight, hiMenu);
    m_colorBtn->setIndicatorColor(m_textColor);
    m_highlightBtn->setIndicatorColor(m_highlightColor);
    font->beginColumn();
    font->addRow({ m_fontBox, m_sizeBox });
    font->addRow({ small(m_bold), small(m_italic), small(m_underline), small(m_strike), m_colorBtn, m_highlightBtn, small(clear) });

    // Paragraph
    RibbonGroup *para = page->addGroup(tr("Paragraph"));
    m_bullets = editAction(tr("Bullets"), QStringLiteral("list-bullet"), {}, true);
    m_numbers = editAction(tr("Numbering"), QStringLiteral("list-number"), {}, true);
    QAction *outdent = editAction(tr("Decrease indent"), QStringLiteral("outdent"));
    QAction *indent = editAction(tr("Increase indent"), QStringLiteral("indent"));
    QAction *spacing = editAction(tr("Line spacing"), QStringLiteral("line-spacing"));
    auto *spMenu = new QMenu(this);
    for (double v : { 1.0, 1.15, 1.5, 2.0, 2.5, 3.0 })
        connect(spMenu->addAction(QString::number(v, 'f', v == 1.0 || v == 2.0 || v == 3.0 ? 1 : 2)), &QAction::triggered, this, [this, v] { if (auto *d = wordDoc()) d->setLineSpacing(v * 100); });
    connect(m_bullets, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->toggleList(QTextListFormat::ListDisc); });
    connect(m_numbers, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->toggleList(QTextListFormat::ListDecimal); });
    connect(outdent, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->changeIndent(-1); });
    connect(indent, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->changeIndent(1); });
    m_alignL = editAction(tr("Align left"), QStringLiteral("align-left"), QKeySequence(QStringLiteral("Ctrl+L")), true);
    m_alignC = editAction(tr("Center"), QStringLiteral("align-center"), QKeySequence(QStringLiteral("Ctrl+E")), true);
    m_alignR = editAction(tr("Align right"), QStringLiteral("align-right"), QKeySequence(QStringLiteral("Ctrl+R")), true);
    m_alignJ = editAction(tr("Justify"), QStringLiteral("align-justify"), QKeySequence(QStringLiteral("Ctrl+J")), true);
    auto *grp = new QActionGroup(this);
    for (QAction *a : { m_alignL, m_alignC, m_alignR, m_alignJ }) grp->addAction(a);
    connect(m_alignL, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->setAlignment(Qt::AlignLeft); });
    connect(m_alignC, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->setAlignment(Qt::AlignHCenter); });
    connect(m_alignR, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->setAlignment(Qt::AlignRight); });
    connect(m_alignJ, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->setAlignment(Qt::AlignJustify); });
    auto *sp = small(spacing, spMenu);
    para->beginColumn();
    para->addRow({ small(m_bullets), small(m_numbers), small(outdent), small(indent), sp });
    para->addRow({ small(m_alignL), small(m_alignC), small(m_alignR), small(m_alignJ) });

    // Styles
    RibbonGroup *styles = page->addGroup(tr("Styles"));
    m_styleBox = new QComboBox;
    m_styleBox->setFixedSize(150, 26);
    m_styleBox->addItems({ tr("Normal"), tr("Title"), tr("Heading 1"), tr("Heading 2"), tr("Heading 3"), tr("Quote") });
    connect(m_styleBox, &QComboBox::activated, this, [this](int i) { if (auto *d = wordDoc()) { d->applyStyle(i); d->activated(); } });
    styles->beginColumn();
    styles->addRow({ m_styleBox });

    // Find
    RibbonGroup *edit = page->addGroup(tr("Editing"));
    QAction *find = makeAction(tr("Find"), QStringLiteral("find"), QKeySequence::Find);
    QAction *repl = editAction(tr("Replace"), QStringLiteral("replace"), QKeySequence(QStringLiteral("Ctrl+H")));
    connect(find, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->findBar()->open(false); });
    connect(repl, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->findBar()->open(true); });
    edit->beginColumn();
    edit->addSmall(find);
    edit->addSmall(repl);
}

// ------------------------------------------------------------------ Insert
void WordWorkspace::insertTableDialog()
{
    auto *d = wordDoc();
    if (!d) return;
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Insert table"));
    auto *form = new QFormLayout(&dlg);
    auto *rows = new QSpinBox(&dlg), *cols = new QSpinBox(&dlg);
    rows->setRange(1, 200); rows->setValue(3);
    cols->setRange(1, 30); cols->setValue(3);
    form->addRow(tr("Rows:"), rows);
    form->addRow(tr("Columns:"), cols);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() == QDialog::Accepted) d->insertTable(rows->value(), cols->value());
}

void WordWorkspace::buildInsert()
{
    RibbonPage *page = m_ribbon->addPage(tr("Insert"));
    QAction *pb = editAction(tr("Page\nBreak"), QStringLiteral("page-break"), QKeySequence(QStringLiteral("Ctrl+Return")));
    m_pageNumbers = editAction(tr("Page numbers (PDF)"), QStringLiteral("page-number"), {}, true);
    connect(pb, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->insertPageBreak(); });
    connect(m_pageNumbers, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setPageNumbers(on); });
    RibbonGroup *pages = page->addGroup(tr("Pages"));
    pages->addLarge(pb);
    pages->beginColumn();
    pages->addSmall(m_pageNumbers);

    QAction *table = editAction(tr("Table"), QStringLiteral("table"));
    connect(table, &QAction::triggered, this, &WordWorkspace::insertTableDialog);
    page->addGroup(tr("Tables"))->addLarge(table);

    RibbonGroup *tools = page->addGroup(tr("Table tools"));
    const struct { const char *t, *i; } ops[] = { { "Row above", "row-above" }, { "Row below", "row-below" }, { "Column left", "col-left" }, { "Column right", "col-right" },
                                                  { "Delete row", "row-delete" }, { "Delete column", "col-delete" }, { "Merge cells", "merge-cells" } };
    for (int i = 0; i < 7; ++i) {
        if (i == 0 || i == 4) tools->beginColumn();
        QAction *a = editAction(tr(ops[i].t), QLatin1String(ops[i].i));
        connect(a, &QAction::triggered, this, [this, i] { if (auto *d = wordDoc()) d->tableOp(i); });
        tools->addSmall(a);
    }

    QAction *pic = editAction(tr("Picture"), QStringLiteral("image"));
    connect(pic, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->insertImage(); });
    page->addGroup(tr("Illustrations"))->addLarge(pic);

    QAction *link = editAction(tr("Link"), QStringLiteral("link"), QKeySequence(QStringLiteral("Ctrl+K")));
    QAction *unlink = editAction(tr("Remove link"), QStringLiteral("unlink"));
    connect(link, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->insertLink(); });
    connect(unlink, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->removeLink(); });
    RibbonGroup *links = page->addGroup(tr("Links"));
    links->addLarge(link);
    links->beginColumn();
    links->addSmall(unlink);
}

// ------------------------------------------------------------------ Layout
void WordWorkspace::buildLayout()
{
    RibbonPage *page = m_ribbon->addPage(tr("Layout"));
    RibbonGroup *setup = page->addGroup(tr("Page setup"));

    auto *marginMenu = new QMenu(this);
    const struct { const char *name; double mm; } margins[] = { { "Normal  (2.54 cm)", 25.4 }, { "Narrow  (1.27 cm)", 12.7 }, { "Moderate  (1.9 cm)", 19.1 }, { "Wide  (5.08 cm)", 50.8 } };
    for (const auto &m : margins)
        connect(marginMenu->addAction(tr(m.name)), &QAction::triggered, this, [this, mm = m.mm] {
            if (auto *d = wordDoc()) { PageSetup ps = d->pageSetup(); ps.marginLeftMm = ps.marginRightMm = ps.marginTopMm = ps.marginBottomMm = mm; d->setPageSetup(ps); d->edit()->document()->setModified(true); }
        });
    auto *orientMenu = new QMenu(this);
    connect(orientMenu->addAction(tr("Portrait")), &QAction::triggered, this, [this] { if (auto *d = wordDoc()) { PageSetup ps = d->pageSetup(); ps.landscape = false; d->setPageSetup(ps); d->edit()->document()->setModified(true); } });
    connect(orientMenu->addAction(tr("Landscape")), &QAction::triggered, this, [this] { if (auto *d = wordDoc()) { PageSetup ps = d->pageSetup(); ps.landscape = true; d->setPageSetup(ps); d->edit()->document()->setModified(true); } });
    auto *sizeMenu = new QMenu(this);
    const struct { const char *name; double w, h; } sizes[] = { { "A4  (21 × 29.7 cm)", 210, 297 }, { "A5  (14.8 × 21 cm)", 148, 210 }, { "Letter  (21.6 × 27.9 cm)", 215.9, 279.4 }, { "Legal  (21.6 × 35.6 cm)", 215.9, 355.6 } };
    for (const auto &s : sizes)
        connect(sizeMenu->addAction(tr(s.name)), &QAction::triggered, this, [this, w = s.w, h = s.h] {
            if (auto *d = wordDoc()) { PageSetup ps = d->pageSetup(); ps.widthMm = w; ps.heightMm = h; d->setPageSetup(ps); d->edit()->document()->setModified(true); }
        });
    setup->addLarge(makeAction(tr("Margins"), QStringLiteral("margins")), marginMenu);
    setup->addLarge(makeAction(tr("Orientation"), QStringLiteral("orientation-portrait")), orientMenu);
    setup->addLarge(makeAction(tr("Size"), QStringLiteral("page-size")), sizeMenu);

    RibbonGroup *para = page->addGroup(tr("Paragraph spacing"));
    auto *beforeMenu = new QMenu(this), *afterMenu = new QMenu(this);
    for (int pt : { 0, 6, 12, 18, 24 }) {
        connect(beforeMenu->addAction(tr("%1 pt").arg(pt)), &QAction::triggered, this, [this, pt] { if (auto *d = wordDoc()) d->setParagraphSpacing(pt, -1); });
        connect(afterMenu->addAction(tr("%1 pt").arg(pt)), &QAction::triggered, this, [this, pt] { if (auto *d = wordDoc()) d->setParagraphSpacing(-1, pt); });
    }
    QAction *before = editAction(tr("Space before"), QStringLiteral("spacing-before"));
    QAction *after = editAction(tr("Space after"), QStringLiteral("spacing-after"));
    para->beginColumn();
    para->addSmall(before, true, beforeMenu);
    para->addSmall(after, true, afterMenu);
}

// ------------------------------------------------------------------ Review
namespace {
int countSyllables(const QString &w)
{
    int n = 0;
    bool prevVowel = false;
    for (QChar ch : w) {
        const bool v = QStringLiteral("aeiouy").contains(ch);
        if (v && !prevVowel) ++n;
        prevVowel = v;
    }
    if (w.endsWith(QLatin1Char('e')) && n > 1 && !w.endsWith(QLatin1String("le"))) --n; // silent e
    return qMax(1, n);
}
}

// Document statistics with reading time, readability (Flesch reading ease) and the most used words.
void WordWorkspace::showWordCount()
{
    auto *d = wordDoc();
    if (!d) return;
    const auto s = d->stats();
    const QString text = d->edit()->document()->toPlainText();

    int sentences = 0;
    bool inEnd = false;
    for (QChar ch : text) {
        if (ch == QLatin1Char('.') || ch == QLatin1Char('!') || ch == QLatin1Char('?')) { if (!inEnd) { ++sentences; inEnd = true; } }
        else if (!ch.isSpace()) inEnd = false;
    }
    if (sentences == 0 && s.words > 0) sentences = 1;

    static const QSet<QString> stop = { "the", "and", "for", "that", "this", "with", "are", "was", "not", "you", "from", "have", "has", "had", "but", "its", "his", "her", "they", "them" };
    QHash<QString, int> freq;
    int syllables = 0, letters = 0, words = 0;
    static const QRegularExpression wordRe(QStringLiteral("[\\p{L}']+"));
    auto it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        const QString w = it.next().captured().toLower();
        ++words;
        letters += w.size();
        syllables += countSyllables(w);
        if (w.size() > 3 && !stop.contains(w)) ++freq[w];
    }
    const double flesch = (words > 0 && sentences > 0) ? 206.835 - 1.015 * (double(words) / sentences) - 84.6 * (double(syllables) / words) : 0.0;
    QString level = tr("n/a");
    if (words > 0) level = flesch >= 80 ? tr("Easy") : flesch >= 60 ? tr("Standard") : flesch >= 40 ? tr("Fairly difficult") : tr("Difficult");
    const int minutes = qMax(words > 0 ? 1 : 0, int(std::ceil(words / 200.0)));

    QList<QPair<int, QString>> top;
    for (auto f = freq.constBegin(); f != freq.constEnd(); ++f) top.append({ f.value(), f.key() });
    std::sort(top.begin(), top.end(), [](const auto &a, const auto &b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    QString topHtml;
    for (int i = 0; i < qMin(5, int(top.size())); ++i) topHtml += QStringLiteral("%1 (%2)%3").arg(top[i].second.toHtmlEscaped()).arg(top[i].first).arg(i < 4 && i + 1 < top.size() ? QStringLiteral(", ") : QString());
    if (topHtml.isEmpty()) topHtml = QStringLiteral("—");

    QMessageBox box(this);
    box.setWindowTitle(tr("Document statistics"));
    box.setIcon(QMessageBox::Information);
    box.setTextFormat(Qt::RichText);
    box.setText(tr("<table cellpadding='4'>"
                   "<tr><td>Pages</td><td><b>%1</b></td><td>&nbsp;&nbsp;Words</td><td><b>%2</b></td></tr>"
                   "<tr><td>Characters (with spaces)</td><td><b>%3</b></td><td>&nbsp;&nbsp;Characters (no spaces)</td><td><b>%4</b></td></tr>"
                   "<tr><td>Paragraphs</td><td><b>%5</b></td><td>&nbsp;&nbsp;Sentences</td><td><b>%6</b></td></tr>"
                   "<tr><td>Reading time</td><td><b>%7 min</b></td><td>&nbsp;&nbsp;Avg. word length</td><td><b>%8</b></td></tr>"
                   "<tr><td>Readability (Flesch)</td><td><b>%9</b></td><td>&nbsp;&nbsp;Level</td><td><b>%10</b></td></tr>"
                   "</table><p>Most used words: %11</p>")
                .arg(s.pages).arg(s.words).arg(s.chars).arg(s.charsNoSpaces).arg(s.paragraphs).arg(sentences)
                .arg(minutes).arg(words > 0 ? QString::number(double(letters) / words, 'f', 1) : QStringLiteral("0"))
                .arg(words > 0 ? QString::number(flesch, 'f', 0) : QStringLiteral("0")).arg(level).arg(topHtml));
    box.exec();
}

void WordWorkspace::buildReview()
{
    RibbonPage *page = m_ribbon->addPage(tr("Review"));
    QAction *wc = makeAction(tr("Document\nStatistics"), QStringLiteral("hash"));
    connect(wc, &QAction::triggered, this, &WordWorkspace::showWordCount);
    page->addGroup(tr("Proofing"))->addLarge(wc);

    QAction *find = makeAction(tr("Find"), QStringLiteral("find"));
    QAction *repl = editAction(tr("Replace"), QStringLiteral("replace"));
    connect(find, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->findBar()->open(false); });
    connect(repl, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->findBar()->open(true); });
    RibbonGroup *f = page->addGroup(tr("Search"));
    f->beginColumn();
    f->addSmall(find);
    f->addSmall(repl);

    m_viewMode = makeAction(tr("Restrict\nEditing"), QStringLiteral("shield"), {}, true);
    connect(m_viewMode, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setViewMode(on); });
    page->addGroup(tr("Protect"))->addLarge(m_viewMode);
}

// ------------------------------------------------------------------ View
void WordWorkspace::buildView()
{
    RibbonPage *page = m_ribbon->addPage(tr("View"));
    QAction *zin = makeAction(tr("Zoom in"), QStringLiteral("zoom-in"), QKeySequence::ZoomIn);
    QAction *zout = makeAction(tr("Zoom out"), QStringLiteral("zoom-out"), QKeySequence::ZoomOut);
    QAction *z100 = makeAction(tr("100%"), QStringLiteral("zoom-in"));
    QAction *fit = makeAction(tr("Fit width"), QStringLiteral("fit-width"));
    connect(zin, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->view()->setZoom(d->view()->zoom() * 1.1); });
    connect(zout, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->view()->setZoom(d->view()->zoom() / 1.1); });
    connect(z100, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->view()->setZoom(1.0); });
    connect(fit, &QAction::triggered, this, [this] { if (auto *d = wordDoc()) d->view()->fitWidth(); });
    RibbonGroup *z = page->addGroup(tr("Zoom"));
    z->addLarge(zin);
    z->addLarge(zout);
    z->beginColumn();
    z->addSmall(z100);
    z->addSmall(fit);
    QAction *mode = makeAction(tr("Read\nMode"), QStringLiteral("eye"), {}, true);
    connect(mode, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setViewMode(on); });
    page->addGroup(tr("Views"))->addLarge(mode);
    m_rulerAct = makeAction(tr("Ruler"), QStringLiteral("ruler"), {}, true);
    m_rulerAct->setChecked(true);
    connect(m_rulerAct, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setRulerVisible(on); });
    page->addGroup(tr("Show"))->addLarge(m_rulerAct);
    connect(m_viewMode, &QAction::toggled, mode, &QAction::setChecked);
    connect(mode, &QAction::toggled, m_viewMode, &QAction::setChecked);
}

// ------------------------------------------------------------------ Status bar + sync
void WordWorkspace::buildStatusBar()
{
    m_pageLabel = m_status->addLeftText();
    m_wordsLabel = m_status->addLeftText();
    m_modeLabel = m_status->addLeftText();
    auto *out = new IconButton(QStringLiteral("zoom-out"), tr("Zoom out"));
    auto *in = new IconButton(QStringLiteral("zoom-in"), tr("Zoom in"));
    for (IconButton *b : { out, in }) { b->setOnChrome(true); b->setFixedSize(26, 24); }
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(25, 300);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(140);
    m_zoomSlider->setAccessibleName(tr("Zoom"));
    m_zoomLabel = new QLabel(QStringLiteral("100%"));
    m_zoomLabel->setFixedWidth(44);
    m_zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_status->addRight(out);
    m_status->addRight(m_zoomSlider);
    m_status->addRight(in);
    m_status->addRight(m_zoomLabel);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { setTextColor(m_zoomLabel, ThemeManager::instance().palette().chromeText); });
    setTextColor(m_zoomLabel, ThemeManager::instance().palette().chromeText);
    connect(out, &IconButton::clicked, this, [this] { if (auto *d = wordDoc()) d->view()->setZoom(d->view()->zoom() / 1.1); });
    connect(in, &IconButton::clicked, this, [this] { if (auto *d = wordDoc()) d->view()->setZoom(d->view()->zoom() * 1.1); });
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) { if (auto *d = wordDoc()) d->view()->setZoom(v / 100.0); });
}

void WordWorkspace::syncStatus()
{
    auto *d = wordDoc();
    if (!d) return;
    const auto s = d->stats();
    m_pageLabel->setText(tr("Page %1 of %2").arg(s.page).arg(s.pages));
    m_wordsLabel->setText(tr("%1 words").arg(s.words));
    m_modeLabel->setText(d->isViewMode() ? tr("View mode") : tr("Editing"));
    const int z = qRound(d->view()->zoom() * 100);
    QSignalBlocker b(m_zoomSlider);
    m_zoomSlider->setValue(z);
    m_zoomLabel->setText(QString::number(z) + QLatin1Char('%'));
}

void WordWorkspace::syncState()
{
    auto *d = wordDoc();
    if (!d) return;
    const auto s = d->formatState();
    auto set = [](QAction *a, bool on) { QSignalBlocker b(a); a->setChecked(on); };
    set(m_bold, s.bold); set(m_italic, s.italic); set(m_underline, s.underline); set(m_strike, s.strike);
    set(m_bullets, s.listStyle == QTextListFormat::ListDisc);
    set(m_numbers, s.listStyle == QTextListFormat::ListDecimal);
    set(m_alignL, s.align & Qt::AlignLeft && !(s.align & (Qt::AlignHCenter | Qt::AlignRight | Qt::AlignJustify)));
    set(m_alignC, s.align & Qt::AlignHCenter);
    set(m_alignR, s.align & Qt::AlignRight);
    set(m_alignJ, s.align & Qt::AlignJustify);
    set(m_pageNumbers, d->pageSetup().pageNumbers);
    set(m_viewMode, d->isViewMode());
    set(m_rulerAct, d->rulerVisible());
    {
        QSignalBlocker b1(m_fontBox), b2(m_sizeBox), b3(m_styleBox);
        m_fontBox->setCurrentFont(QFont(s.family));
        m_sizeBox->setCurrentText(QString::number(s.size, 'g', 4));
        m_styleBox->setCurrentIndex(s.heading >= 1 && s.heading <= 3 ? 1 + s.heading : 0);
    }
    const bool editable = !d->isViewMode();
    m_undo->setEnabled(editable && s.canUndo);
    m_redo->setEnabled(editable && s.canRedo);
    m_cut->setEnabled(editable && s.hasSelection);
    m_copy->setEnabled(s.hasSelection);
    for (QWidget *w : { static_cast<QWidget *>(m_fontBox), static_cast<QWidget *>(m_sizeBox), static_cast<QWidget *>(m_styleBox) }) w->setEnabled(editable);
}

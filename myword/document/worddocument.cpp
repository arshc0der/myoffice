#include "worddocument.h"

#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QImage>
#include <QInputDialog>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSpinBox>
#include <QStringConverter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentWriter>
#include <QTextEdit>
#include <QTextList>
#include <QTextTable>
#include <QUuid>
#include <QVBoxLayout>

#include "core/autosavemanager.h"
#include "core/filemanager.h"
#include "core/fileio.h"
#include "myword/editor/findbar.h"
#include "myword/editor/pageview.h"
#include "myword/importexport/docxio.h"
#include "myword/importexport/htmlio.h"
#include "myword/importexport/pdfexport.h"
#include "ui/widgets/toast.h"

namespace {
QFont defaultDocFont()
{
    const QStringList have = QFontDatabase::families();
    for (const char *f : { "Calibri", "Segoe UI", "Arial", "Helvetica", "Noto Sans", "DejaVu Sans" })
        if (have.contains(QLatin1String(f))) return QFont(QLatin1String(f), 11);
    return QFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont).family(), 11);
}

QString decodeText(const QByteArray &d)
{
    if (auto enc = QStringConverter::encodingForData(d)) { QStringDecoder dec(*enc); return dec(d); } // BOM present
    QStringDecoder utf8(QStringConverter::Utf8);
    const QString s = utf8(d);
    if (!utf8.hasError()) return s;
    return QString::fromLocal8Bit(d);
}

bool isTextExt(const QString &ext) { return ext != "docx" && ext != "html" && ext != "htm" && ext != "md" && ext != "markdown"; }
}

WordDocument::WordDocument(QWidget *parent) : DocumentView(parent)
{
    static int counter = 0;
    m_untitled = tr("Document %1").arg(++counter);
    m_id = QStringLiteral("word-") + QUuid::createUuid().toString(QUuid::Id128);

    m_view = new PageView(this);
    m_edit = m_view->edit();
    m_find = new FindBar(m_edit, this);
    m_find->hide();
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addWidget(m_find);
    col->addWidget(m_view, 1);

    m_edit->document()->setDefaultFont(defaultDocFont());
    m_edit->setFont(defaultDocFont());
    m_edit->viewport()->installEventFilter(this);

    m_statsTimer.setSingleShot(true);
    m_statsTimer.setInterval(300);
    connect(&m_statsTimer, &QTimer::timeout, this, &DocumentView::statusChanged);
    connect(m_edit->document(), &QTextDocument::contentsChanged, this, [this] { m_statsTimer.start(); });
    connect(m_edit->document(), &QTextDocument::modificationChanged, this, [this] { emit modifiedChanged(); });
    connect(m_edit, &QTextEdit::cursorPositionChanged, this, [this] { m_view->ensureCursorVisible(); emit stateChanged(); emit statusChanged(); });
    connect(m_edit, &QTextEdit::currentCharFormatChanged, this, [this] { emit stateChanged(); });
    connect(m_edit, &QTextEdit::copyAvailable, this, [this] { emit stateChanged(); });
    connect(m_edit, &QTextEdit::undoAvailable, this, [this] { emit stateChanged(); });
    connect(m_edit, &QTextEdit::redoAvailable, this, [this] { emit stateChanged(); });
    connect(m_edit, &QTextEdit::selectionChanged, this, [this] { emit statusChanged(); });
    connect(m_view, &PageView::zoomChanged, this, [this] { emit statusChanged(); });

    AutosaveManager::instance().registerDocument(
        m_id, [this] { return HtmlIO::write(m_edit->document()); }, [this] { return isModified(); });
}

WordDocument::~WordDocument() { AutosaveManager::instance().unregisterDocument(m_id, false); }

QString WordDocument::displayName() const { return m_path.isEmpty() ? m_untitled : QFileInfo(m_path).fileName(); }
bool WordDocument::isModified() const { return m_edit->document()->isModified(); }
void WordDocument::aboutToClose() { AutosaveManager::instance().unregisterDocument(m_id, true); }

void WordDocument::activated()
{
    m_view->focusEditor();
    emit stateChanged();
    emit statusChanged();
}

void WordDocument::setViewMode(bool on)
{
    m_viewMode = on;
    applyViewMode();
    emit viewModeChanged();
}

void WordDocument::applyViewMode()
{
    m_edit->setReadOnly(m_viewMode);
    m_edit->setTextInteractionFlags(m_viewMode ? (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard | Qt::LinksAccessibleByMouse)
                                               : Qt::TextEditorInteraction);
    m_find->setReadOnly(m_viewMode);
}

// ------------------------------------------------------------------ open / save
void WordDocument::load(const QString &path, bool viewMode)
{
    m_path = path;
    m_viewMode = viewMode;
    applyViewMode();
    emit titleChanged();
    FileIO::readAsync(path, this, [this, path](const QByteArray &data, const QString &err) {
        QString perr = err;
        if (!err.isEmpty() || !parseBytes(path, data, &perr)) { emit loadFinished(false, perr); return; }
        m_edit->document()->setModified(false);
        m_edit->document()->clearUndoRedoStacks();
        emit titleChanged();
        emit viewModeChanged();
        emit statusChanged();
        emit loadFinished(true, QString());
    });
}

bool WordDocument::parseBytes(const QString &path, const QByteArray &data, QString *error)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    QTextDocument *doc = m_edit->document();
    if (ext == "docx") {
        PageSetup ps = pageSetup();
        if (!DocxIO::read(data, doc, &ps, error)) return false;
        setPageSetup(ps);
    } else if (ext == "html" || ext == "htm") {
        HtmlIO::read(data, doc);
    } else if (ext == "md" || ext == "markdown") {
        doc->setMarkdown(decodeText(data));
    } else {
        doc->setPlainText(decodeText(data));
    }
    doc->setDefaultFont(defaultDocFont());
    return true;
}

void WordDocument::loadRecovery(const QByteArray &html, const QString &id)
{
    AutosaveManager::instance().unregisterDocument(m_id, false);
    m_id = id;
    HtmlIO::read(html, m_edit->document());
    m_edit->document()->setModified(true);
    AutosaveManager::instance().registerDocument(
        m_id, [this] { return HtmlIO::write(m_edit->document()); }, [this] { return isModified(); });
    emit message(tr("Recovered an unsaved document."), Toast::Info);
}

bool WordDocument::writeFile(const QString &path, QString *error)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    QTextDocument *doc = m_edit->document();
    QByteArray bytes;
    if (ext == "pdf") return PdfExport::write(doc, pageSetup(), path, error);
    if (ext == "odt") {
        QTextDocumentWriter w(path, "ODF");
        if (!w.write(doc)) { if (error) *error = tr("Could not write the ODT file."); return false; }
        return true;
    }
    if (ext == "docx") {
        bytes = DocxIO::write(doc, pageSetup());
        if (bytes.isEmpty()) { if (error) *error = tr("Could not create the DOCX file."); return false; }
    } else if (ext == "html" || ext == "htm") bytes = HtmlIO::write(doc);
    else if (ext == "md" || ext == "markdown") bytes = doc->toMarkdown().toUtf8();
    else bytes = doc->toPlainText().toUtf8();

    QSaveFile f(path); // atomic replace: a crash never leaves a half-written file
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool WordDocument::saveToPath(const QString &path)
{
    QString err;
    if (!writeFile(path, &err)) { emit message(tr("Save failed: %1").arg(err), Toast::Error); return false; }
    m_path = path;
    m_edit->document()->setModified(false);
    FileManager::instance().addRecent(path);
    emit titleChanged();
    emit message(tr("Saved %1").arg(QFileInfo(path).fileName()), Toast::Success);
    return true;
}

bool WordDocument::save()
{
    if (m_path.isEmpty()) return saveAs();
    if (m_viewMode) setViewMode(false);
    return saveToPath(m_path);
}

bool WordDocument::saveAs()
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QString base = m_path.isEmpty() ? m_untitled : QFileInfo(m_path).completeBaseName();
    QString selected;
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), dir + QLatin1Char('/') + base,
        tr("Word Document (*.docx);;Web Page (*.html);;Markdown (*.md);;Plain Text (*.txt)"), &selected);
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty()) {
        const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("\\*\\.(\\w+)")).match(selected);
        path += QLatin1Char('.') + (m.hasMatch() ? m.captured(1) : QStringLiteral("docx"));
    }
    QSettings().setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    return saveToPath(path);
}

bool WordDocument::exportTo(const QString &path)
{
    QString err;
    if (!writeFile(path, &err)) { emit message(tr("Export failed: %1").arg(err), Toast::Error); return false; }
    emit message(tr("Exported %1").arg(QFileInfo(path).fileName()), Toast::Success);
    return true;
}

void WordDocument::exportDialog(const QString &suffix)
{
    static const QHash<QString, QString> names = { { "pdf", "PDF Document" }, { "docx", "Word Document" }, { "html", "Web Page" },
                                                   { "md", "Markdown" }, { "txt", "Plain Text" }, { "odt", "OpenDocument Text" } };
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QString base = m_path.isEmpty() ? m_untitled : QFileInfo(m_path).completeBaseName();
    const QString path = QFileDialog::getSaveFileName(this, tr("Export as %1").arg(names.value(suffix)),
        dir + QLatin1Char('/') + base + QLatin1Char('.') + suffix, tr("%1 (*.%2)").arg(names.value(suffix), suffix));
    if (!path.isEmpty()) exportTo(path);
}

// ------------------------------------------------------------------ state
PageSetup WordDocument::pageSetup() const { return m_view->pageSetup(); }
void WordDocument::setPageSetup(const PageSetup &ps) { m_view->setPageSetup(ps); emit statusChanged(); }
void WordDocument::setPageNumbers(bool on) { PageSetup ps = pageSetup(); ps.pageNumbers = on; setPageSetup(ps); }

WordDocument::FormatState WordDocument::formatState() const
{
    FormatState s;
    const QTextCharFormat cf = m_edit->currentCharFormat();
    const QTextCursor c = m_edit->textCursor();
    s.bold = cf.fontWeight() >= static_cast<int>(QFont::Bold);
    s.italic = cf.fontItalic();
    s.underline = cf.fontUnderline();
    s.strike = cf.fontStrikeOut();
    s.family = cf.hasProperty(QTextFormat::FontFamilies) ? cf.fontFamilies().toStringList().value(0) : cf.stringProperty(QTextFormat::FontFamily);
    if (s.family.isEmpty()) s.family = m_edit->document()->defaultFont().family();
    s.size = cf.hasProperty(QTextFormat::FontPointSize) ? cf.fontPointSize() : m_edit->document()->defaultFont().pointSizeF();
    s.align = m_edit->alignment();
    if (QTextList *l = c.currentList()) s.listStyle = l->format().style();
    s.heading = c.blockFormat().headingLevel();
    s.inTable = c.currentTable() != nullptr;
    s.hasSelection = c.hasSelection();
    s.canUndo = m_edit->document()->isUndoAvailable();
    s.canRedo = m_edit->document()->isRedoAvailable();
    return s;
}

WordDocument::Stats WordDocument::stats() const
{
    Stats s;
    const QString text = m_edit->document()->toPlainText();
    s.chars = text.size();
    s.paragraphs = m_edit->document()->blockCount();
    bool inWord = false;
    for (QChar ch : text) {
        const bool space = ch.isSpace();
        if (!space) { ++s.charsNoSpaces; if (!inWord) { ++s.words; inWord = true; } }
        else inWord = false;
    }
    s.pages = m_view->pageCount();
    s.page = m_view->pageOfCursor();
    return s;
}

// ------------------------------------------------------------------ editing commands
void WordDocument::setFontFamily(const QString &f) { QTextCharFormat cf; cf.setFontFamilies({ f }); m_edit->mergeCurrentCharFormat(cf); }
void WordDocument::setFontSize(qreal pt) { if (pt < 1) return; QTextCharFormat cf; cf.setFontPointSize(pt); m_edit->mergeCurrentCharFormat(cf); }
void WordDocument::setBold(bool on) { QTextCharFormat cf; cf.setFontWeight(on ? QFont::Bold : QFont::Normal); m_edit->mergeCurrentCharFormat(cf); }
void WordDocument::setItalic(bool on) { QTextCharFormat cf; cf.setFontItalic(on); m_edit->mergeCurrentCharFormat(cf); }
void WordDocument::setUnderline(bool on) { QTextCharFormat cf; cf.setFontUnderline(on); m_edit->mergeCurrentCharFormat(cf); }
void WordDocument::setStrike(bool on) { QTextCharFormat cf; cf.setFontStrikeOut(on); m_edit->mergeCurrentCharFormat(cf); }
void WordDocument::setTextColor(const QColor &c) { QTextCharFormat cf; cf.setForeground(c); m_edit->mergeCurrentCharFormat(cf); }
void WordDocument::setHighlight(const QColor &c, bool clear)
{
    QTextCharFormat cf;
    cf.setBackground(clear ? QBrush(Qt::NoBrush) : QBrush(c));
    m_edit->mergeCurrentCharFormat(cf);
}

void WordDocument::clearFormatting()
{
    QTextCursor c = m_edit->textCursor();
    c.beginEditBlock();
    c.setCharFormat(QTextCharFormat());
    QTextBlockFormat bf;
    bf.setAlignment(Qt::AlignLeft);
    c.mergeBlockFormat(bf);
    c.endEditBlock();
    m_edit->setCurrentCharFormat(QTextCharFormat());
}

void WordDocument::setAlignment(Qt::Alignment a) { m_edit->setAlignment(a); emit stateChanged(); }

void WordDocument::blocksInSelection(const std::function<void(const QTextBlock &)> &fn) const
{
    const QTextCursor c = m_edit->textCursor();
    const int start = c.selectionStart(), end = c.hasSelection() ? c.selectionEnd() - 1 : c.selectionEnd();
    for (QTextBlock b = m_edit->document()->findBlock(start); b.isValid() && b.position() <= end; b = b.next()) fn(b);
}

void WordDocument::toggleList(QTextListFormat::Style style)
{
    QTextCursor c = m_edit->textCursor();
    c.beginEditBlock();
    QTextList *existing = c.currentList();
    if (existing && existing->format().style() == style) {
        blocksInSelection([&](const QTextBlock &b) {
            if (QTextList *l = b.textList()) l->remove(b);
            QTextCursor bc(b);
            QTextBlockFormat bf = bc.blockFormat();
            bf.setIndent(0);
            bc.setBlockFormat(bf);
        });
    } else {
        QTextListFormat lf;
        lf.setStyle(style);
        lf.setIndent(existing ? existing->format().indent() : 1);
        QTextList *list = nullptr;
        blocksInSelection([&](const QTextBlock &b) {
            if (!list) { QTextCursor bc(b); list = bc.createList(lf); }
            else list->add(b);
        });
    }
    c.endEditBlock();
    emit stateChanged();
}

void WordDocument::changeIndent(int delta)
{
    QTextCursor c = m_edit->textCursor();
    c.beginEditBlock();
    QTextList *list = nullptr;
    blocksInSelection([&](const QTextBlock &b) {
        if (QTextList *l = b.textList()) {
            QTextListFormat lf = l->format();
            lf.setIndent(qBound(1, lf.indent() + delta, 9));
            if (!list) { QTextCursor bc(b); list = bc.createList(lf); } else list->add(b);
        } else {
            QTextCursor bc(b);
            QTextBlockFormat bf = bc.blockFormat();
            bf.setIndent(qBound(0, bf.indent() + delta, 8));
            bc.setBlockFormat(bf);
        }
    });
    c.endEditBlock();
}

void WordDocument::setLineSpacing(qreal percent)
{
    QTextBlockFormat bf;
    bf.setLineHeight(percent, QTextBlockFormat::ProportionalHeight);
    QTextCursor c = m_edit->textCursor();
    c.mergeBlockFormat(bf);
}

void WordDocument::setParagraphSpacing(qreal before, qreal after)
{
    QTextBlockFormat bf;
    if (before >= 0) bf.setTopMargin(before * 96.0 / 72.0);
    if (after >= 0) bf.setBottomMargin(after * 96.0 / 72.0);
    QTextCursor c = m_edit->textCursor();
    c.mergeBlockFormat(bf);
}

void WordDocument::applyStyle(int idx)
{
    static const struct { qreal pt; bool bold, italic; int heading; const char *color; } styles[] = {
        { 11, false, false, 0, nullptr }, { 28, true, false, 0, nullptr }, { 20, true, false, 1, nullptr },
        { 16, true, false, 2, nullptr }, { 13, true, false, 3, nullptr }, { 11, false, true, 0, "#5B6475" } };
    if (idx < 0 || idx > 5) return;
    const auto &s = styles[idx];
    QTextCharFormat cf;
    cf.setFontPointSize(s.pt);
    cf.setFontWeight(s.bold ? QFont::Bold : QFont::Normal);
    cf.setFontItalic(s.italic);
    cf.setForeground(s.color ? QBrush(QColor(QLatin1String(s.color))) : QBrush(QColor("#1B1E27")));
    QTextBlockFormat bf;
    bf.setHeadingLevel(s.heading);
    bf.setTopMargin(s.heading ? 12 : 0);
    bf.setBottomMargin(s.heading ? 6 : 0);
    QTextCursor c = m_edit->textCursor();
    c.beginEditBlock();
    c.mergeBlockFormat(bf);
    QTextCursor sel = c;
    sel.setPosition(c.selectionStart());
    sel.movePosition(QTextCursor::StartOfBlock);
    sel.setPosition(c.selectionEnd(), QTextCursor::KeepAnchor);
    sel.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    sel.mergeCharFormat(cf);
    c.mergeBlockCharFormat(cf);
    c.endEditBlock();
    m_edit->mergeCurrentCharFormat(cf);
    emit stateChanged();
}

void WordDocument::insertTable(int rows, int cols)
{
    QTextTableFormat tf;
    tf.setBorder(1);
    tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    tf.setBorderBrush(QColor("#808080"));
    tf.setCellPadding(5);
    tf.setCellSpacing(0);
    tf.setBorderCollapse(true);
    tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    m_edit->textCursor().insertTable(rows, cols, tf);
}

void WordDocument::tableOp(int op)
{
    QTextCursor c = m_edit->textCursor();
    QTextTable *t = c.currentTable();
    if (!t) { emit message(tr("Place the cursor inside a table first."), Toast::Info); return; }
    const QTextTableCell cell = t->cellAt(c);
    switch (op) {
    case 0: t->insertRows(cell.row(), 1); break;
    case 1: t->insertRows(cell.row() + 1, 1); break;
    case 2: t->insertColumns(cell.column(), 1); break;
    case 3: t->insertColumns(cell.column() + 1, 1); break;
    case 4: if (t->rows() > 1) t->removeRows(cell.row(), 1); break;
    case 5: if (t->columns() > 1) t->removeColumns(cell.column(), 1); break;
    case 6: t->mergeCells(c); break;
    }
}

void WordDocument::insertImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Insert picture"), QString(), tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty()) return;
    const QImage img(path);
    if (img.isNull()) { emit message(tr("That image could not be read."), Toast::Error); return; }
    const QString name = QStringLiteral("myword-image-") + QUuid::createUuid().toString(QUuid::Id128);
    m_edit->document()->addResource(QTextDocument::ImageResource, QUrl(name), img);
    const qreal maxW = pageSetup().contentSizePx().width();
    const qreal w = qMin<qreal>(img.width(), maxW);
    QTextImageFormat f;
    f.setName(name);
    f.setWidth(w);
    f.setHeight(w * img.height() / img.width());
    m_edit->textCursor().insertImage(f);
}

void WordDocument::insertLink()
{
    bool ok = false;
    const QString url = QInputDialog::getText(this, tr("Insert link"), tr("Address (URL):"), QLineEdit::Normal, QStringLiteral("https://"), &ok);
    if (!ok || url.trimmed().isEmpty()) return;
    QTextCharFormat f;
    f.setAnchor(true);
    f.setAnchorHref(url.trimmed());
    f.setForeground(QColor("#0563C1"));
    f.setFontUnderline(true);
    QTextCursor c = m_edit->textCursor();
    if (c.hasSelection()) c.mergeCharFormat(f); else c.insertText(url.trimmed(), f);
}

void WordDocument::removeLink()
{
    QTextCharFormat f;
    f.setAnchor(false);
    f.setAnchorHref(QString());
    f.setFontUnderline(false);
    f.setForeground(QColor("#1B1E27"));
    QTextCursor c = m_edit->textCursor();
    if (!c.hasSelection()) c.select(QTextCursor::WordUnderCursor);
    c.mergeCharFormat(f);
}

void WordDocument::insertPageBreak()
{
    QTextBlockFormat bf;
    bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
    m_edit->textCursor().insertBlock(bf);
}

bool WordDocument::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_edit->viewport() && e->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton && (m_viewMode || (me->modifiers() & Qt::ControlModifier))) {
            const QString a = m_edit->anchorAt(me->pos());
            if (!a.isEmpty()) QDesktopServices::openUrl(QUrl(a));
        }
    }
    return DocumentView::eventFilter(o, e);
}

#include "exceldocument.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QRegularExpression>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSplitter>
#include <cmath>
#include <QSettings>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>

#include "core/filemanager.h"
#include "core/fileio.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"
#include "myexcel/importexport/csvio.h"
#include "myexcel/importexport/xlsxio.h"
#include "ui/widgets/iconbutton.h"
#include "ui/widgets/pillbutton.h"
#include "ui/widgets/toast.h"

namespace {
// Clipboard origin of the last copy made inside MyExcel, so pasted formulas can shift relative references.
struct ClipOrigin { QString text; int row = 0, col = 0; bool valid = false; } g_clip;

// Draws a heavy accent border around the active cell.
class CellDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
    {
        QStyleOptionViewItem o = opt;
        o.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(p, o, idx);
        if (opt.state & QStyle::State_HasFocus) {
            p->save();
            p->setPen(QPen(ThemeManager::instance().palette().accent, 2));
            p->setBrush(Qt::NoBrush);
            p->drawRect(opt.rect.adjusted(1, 1, -1, -1));
            p->restore();
        }
    }
};

std::vector<std::vector<QString>> parseTsv(const QString &t) { return CsvIO::parse(t, QLatin1Char('\t')); }
}

ExcelDocument::ExcelDocument(QWidget *parent) : DocumentView(parent)
{
    static int counter = 0;
    m_untitled = tr("Workbook %1").arg(++counter);

    m_nameBox = new QLineEdit(this);
    m_nameBox->setFixedWidth(96);
    m_nameBox->setAlignment(Qt::AlignCenter);
    m_nameBox->setAccessibleName(tr("Name box"));
    m_formulaBar = new QLineEdit(this);
    m_formulaBar->setAccessibleName(tr("Formula bar"));
    m_formulaBar->setPlaceholderText(tr("Enter a value or a formula, e.g. =SUM(A1:A10)"));
    auto *fx = new QLabel(this);
    fx->setPixmap(Icons::pixmap(QStringLiteral("function"), ThemeManager::instance().palette().textSecondary, 18, devicePixelRatioF()));
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(10, 6, 10, 6);
    bar->setSpacing(8);
    bar->addWidget(m_nameBox);
    bar->addWidget(fx);
    bar->addWidget(m_formulaBar, 1);

    m_stack = new QStackedWidget(this);
    m_tabs = new QTabBar(this);
    m_tabs->setShape(QTabBar::RoundedSouth);
    m_tabs->setExpanding(false);
    m_tabs->setDrawBase(false);
    m_tabs->setAccessibleName(tr("Sheet tabs"));
    auto *plus = new IconButton(QStringLiteral("plus"), tr("New sheet"), this);
    auto *tabRow = new QHBoxLayout;
    tabRow->setContentsMargins(8, 2, 8, 2);
    tabRow->addWidget(m_tabs);
    tabRow->addWidget(plus);
    tabRow->addStretch(1);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addLayout(bar);
    m_split = new QSplitter(Qt::Horizontal, this);
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(1);
    m_split->addWidget(m_stack);
    m_chart = new ChartPanel(m_split);
    m_chart->hide();
    m_split->addWidget(m_chart);
    m_split->setStretchFactor(0, 3);
    m_split->setStretchFactor(1, 2);
    col->addWidget(m_split, 1);
    m_chartTimer.setSingleShot(true);
    m_chartTimer.setInterval(180);
    connect(&m_chartTimer, &QTimer::timeout, this, [this] { refreshChart(); });
    connect(m_chart, &ChartPanel::closeRequested, this, &ExcelDocument::hideChart);
    connect(m_chart, &ChartPanel::exportRequested, this, &ExcelDocument::exportChartImage);
    col->addLayout(tabRow);

    addPage(tr("Sheet1"));
    m_tabs->setCurrentIndex(0);
    m_undo.setClean();

    connect(&m_undo, &QUndoStack::cleanChanged, this, [this] { emit modifiedChanged(); });
    connect(&m_undo, &QUndoStack::indexChanged, this, [this] { emit stateChanged(); emit statusChanged(); });
    connect(plus, &IconButton::clicked, this, &ExcelDocument::addSheet);
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int i) { if (i >= 0 && i < int(m_pages.size())) { m_stack->setCurrentIndex(i); onCurrentChanged(); emit stateChanged(); } });
    connect(m_tabs, &QTabBar::tabBarDoubleClicked, this, [this](int) { renameSheet(); });
    connect(m_formulaBar, &QLineEdit::returnPressed, this, &ExcelDocument::commitFormula);
    connect(m_nameBox, &QLineEdit::returnPressed, this, [this] { jumpTo(m_nameBox->text()); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this, fx] {
        fx->setPixmap(Icons::pixmap(QStringLiteral("function"), ThemeManager::instance().palette().textSecondary, 18, devicePixelRatioF()));
        styleViews();
    });
    styleViews();
}

ExcelDocument::~ExcelDocument() = default;

QString ExcelDocument::displayName() const { return m_path.isEmpty() ? m_untitled : QFileInfo(m_path).fileName(); }
ExcelDocument::Page *ExcelDocument::page() const { const int i = m_tabs->currentIndex(); return (i >= 0 && i < int(m_pages.size())) ? m_pages[i].get() : nullptr; }

ExcelDocument::Page *ExcelDocument::addPage(const QString &name)
{
    auto p = std::make_unique<Page>();
    p->name = name;
    p->sheet = std::make_unique<Sheet>();
    p->model = new SheetModel(p->sheet.get(), &m_undo, this);
    p->model->setReadOnly(m_viewMode);
    p->view = new QTableView(m_stack);
    p->view->setModel(p->model);
    p->view->setItemDelegate(new CellDelegate(p->view));
    p->view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    p->view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    p->view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    p->view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    p->view->horizontalHeader()->setDefaultSectionSize(qRound(96 * m_zoom / 100.0));
    p->view->verticalHeader()->setDefaultSectionSize(qRound(24 * m_zoom / 100.0));
    p->view->horizontalHeader()->setHighlightSections(true);
    p->view->verticalHeader()->setHighlightSections(true);
    p->view->setShowGrid(m_gridlines);
    p->view->setCornerButtonEnabled(true);
    p->view->installEventFilter(this);
    QFont f = p->view->font();
    f.setPointSizeF(10.5 * m_zoom / 100.0);
    p->view->setFont(f);
    p->model->setBaseFont(f);
    m_stack->addWidget(p->view);
    m_tabs->addTab(name);
    connect(p->view->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { onCurrentChanged(); });
    connect(p->view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { onCurrentChanged(); emit stateChanged(); });
    connect(p->model, &QAbstractItemModel::dataChanged, this, [this] { onCurrentChanged(); if (m_chart && m_chart->isVisible()) m_chartTimer.start(); });
    Page *raw = p.get();
    m_pages.push_back(std::move(p));
    styleViews();
    return raw;
}

void ExcelDocument::styleViews()
{
    const Palette &pal = ThemeManager::instance().palette();
    auto rgba = [](const QColor &c, int a) { return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a); };
    const QString css = QStringLiteral(
        "QTableView{background:%1;gridline-color:%2;selection-background-color:%3;selection-color:%4;border:none;}"
        "QHeaderView::section{background:%5;color:%6;border:none;border-right:1px solid %2;border-bottom:1px solid %2;padding:2px 6px;font-weight:600;}"
        "QTableCornerButton::section{background:%5;border:none;border-right:1px solid %2;border-bottom:1px solid %2;}")
        .arg(pal.surface.name(), pal.border.name(), rgba(pal.accent, 60), pal.text.name(), pal.surfaceHover.name(), pal.textSecondary.name());
    for (auto &p : m_pages) p->view->setStyleSheet(css);
}

// ------------------------------------------------------------------ state & selection
ExcelDocument::State ExcelDocument::state() const
{
    State s;
    s.canUndo = m_undo.canUndo();
    s.canRedo = m_undo.canRedo();
    if (Page *p = page()) {
        const QModelIndex cur = p->view->currentIndex();
        if (cur.isValid()) s.style = p->sheet->cell(cur.row(), cur.column()).style;
        s.hasSelection = p->view->selectionModel()->hasSelection();
        s.sheetName = p->name;
    }
    return s;
}

QRect ExcelDocument::clamp(QRect r) const
{
    Page *p = page();
    if (!p) return r;
    const QSize ext = p->sheet->usedExtent();
    if (r.height() > 2000) r.setBottom(qMin(r.bottom(), qMax(ext.height() - 1, r.top() + 49))); // whole-column selections
    if (r.width() > 100) r.setRight(qMin(r.right(), qMax(ext.width() - 1, r.left() + 25)));
    return r;
}

QList<QRect> ExcelDocument::selectionRects() const
{
    QList<QRect> out;
    Page *p = page();
    if (!p) return out;
    for (const QItemSelectionRange &r : p->view->selectionModel()->selection())
        out << clamp(QRect(QPoint(r.left(), r.top()), QPoint(r.right(), r.bottom())));
    if (out.isEmpty()) { const QModelIndex c = p->view->currentIndex(); if (c.isValid()) out << QRect(c.column(), c.row(), 1, 1); }
    return out;
}

void ExcelDocument::onCurrentChanged()
{
    Page *p = page();
    if (!p) return;
    const QModelIndex cur = p->view->currentIndex();
    if (!cur.isValid()) return;
    if (!m_formulaBar->hasFocus()) m_formulaBar->setText(p->sheet->cell(cur.row(), cur.column()).input);
    QString name = Formula::refName(cur.row(), cur.column());
    const auto sel = p->view->selectionModel()->selection();
    if (!sel.isEmpty()) {
        const auto r = sel.first();
        if (r.width() > 1 || r.height() > 1) name = Formula::refName(r.top(), r.left()) + QLatin1Char(':') + Formula::refName(r.bottom(), r.right());
    }
    if (!m_nameBox->hasFocus()) m_nameBox->setText(name);
    emit statusChanged();
    emit stateChanged();
}

void ExcelDocument::goToCell(int r, int c)
{
    Page *p = page();
    if (!p) return;
    const QModelIndex idx = p->model->index(r, c);
    p->view->setCurrentIndex(idx);
    p->view->scrollTo(idx);
}

void ExcelDocument::jumpTo(const QString &text)
{
    Page *p = page();
    if (!p) return;
    const QStringList parts = text.split(QLatin1Char(':'));
    int r1, c1, r2, c2;
    if (parts.isEmpty() || !Formula::parseRef(parts[0].trimmed(), &r1, &c1)) { emit message(tr("\"%1\" is not a valid cell reference.").arg(text), Toast::Error); return; }
    r2 = r1; c2 = c1;
    if (parts.size() > 1 && !Formula::parseRef(parts[1].trimmed(), &r2, &c2)) { r2 = r1; c2 = c1; }
    goToCell(r1, c1);
    p->view->selectionModel()->select(QItemSelection(p->model->index(qMin(r1, r2), qMin(c1, c2)), p->model->index(qMax(r1, r2), qMax(c1, c2))), QItemSelectionModel::ClearAndSelect);
    p->view->setFocus();
}

void ExcelDocument::commitFormula()
{
    Page *p = page();
    if (!p) return;
    const QModelIndex cur = p->view->currentIndex();
    if (!cur.isValid()) return;
    p->model->setData(cur, m_formulaBar->text(), Qt::EditRole);
    p->view->setFocus();
    goToCell(qMin(cur.row() + 1, Sheet::kMaxRows - 1), cur.column());
}

void ExcelDocument::focusFormulaBar(const QString &prefill)
{
    if (m_viewMode) return;
    m_formulaBar->setFocus();
    if (!prefill.isEmpty()) m_formulaBar->setText(prefill);
    m_formulaBar->setCursorPosition(m_formulaBar->text().size());
}

bool ExcelDocument::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(e);
        if (ke->key() == Qt::Key_Delete && !m_viewMode) { clearSelection(); return true; }
    }
    return DocumentView::eventFilter(o, e);
}

// ------------------------------------------------------------------ view / zoom
void ExcelDocument::setViewMode(bool on)
{
    m_viewMode = on;
    applyViewMode();
    emit viewModeChanged();
}

void ExcelDocument::applyViewMode()
{
    for (auto &p : m_pages) p->model->setReadOnly(m_viewMode);
    m_formulaBar->setReadOnly(m_viewMode);
    m_nameBox->setReadOnly(false);
}

void ExcelDocument::setZoom(int percent)
{
    m_zoom = qBound(40, percent, 300);
    for (auto &p : m_pages) {
        p->view->horizontalHeader()->setDefaultSectionSize(qRound(96 * m_zoom / 100.0));
        p->view->verticalHeader()->setDefaultSectionSize(qRound(24 * m_zoom / 100.0));
        QFont f = p->view->font();
        f.setPointSizeF(10.5 * m_zoom / 100.0);
        p->view->setFont(f);
        p->model->setBaseFont(f);
    }
    emit statusChanged();
}

void ExcelDocument::setGridlines(bool on)
{
    m_gridlines = on;
    for (auto &p : m_pages) p->view->setShowGrid(on);
}

void ExcelDocument::activated()
{
    if (Page *p = page()) p->view->setFocus();
    emit stateChanged();
    emit statusChanged();
}

// ------------------------------------------------------------------ clipboard & editing
void ExcelDocument::copy()
{
    if (auto *le = qobject_cast<QLineEdit *>(QApplication::focusWidget())) { le->copy(); return; }
    Page *p = page();
    const QList<QRect> rects = selectionRects();
    if (!p || rects.isEmpty()) return;
    const QRect r = rects.first();
    QString text;
    for (int row = r.top(); row <= r.bottom(); ++row) {
        for (int col = r.left(); col <= r.right(); ++col) {
            QString v = p->sheet->cell(row, col).input;
            if (v.contains(QLatin1Char('\t')) || v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('"'))) { v.replace(QLatin1Char('"'), QStringLiteral("\"\"")); v = QLatin1Char('"') + v + QLatin1Char('"'); }
            if (col > r.left()) text += QLatin1Char('\t');
            text += v;
        }
        text += QLatin1Char('\n');
    }
    QApplication::clipboard()->setText(text);
    g_clip = { text, r.top(), r.left(), true };
}

void ExcelDocument::cut() { if (qobject_cast<QLineEdit *>(QApplication::focusWidget())) { static_cast<QLineEdit *>(QApplication::focusWidget())->cut(); return; } copy(); clearSelection(); }

void ExcelDocument::paste()
{
    if (auto *le = qobject_cast<QLineEdit *>(QApplication::focusWidget())) { le->paste(); return; }
    Page *p = page();
    if (!p || m_viewMode) return;
    const QString text = QApplication::clipboard()->text();
    if (text.isEmpty()) return;
    auto grid = parseTsv(text);
    if (!grid.empty() && grid.back().size() == 1 && grid.back()[0].isEmpty() && text.endsWith(QLatin1Char('\n'))) grid.pop_back();
    if (grid.empty()) return;
    const QList<QRect> rects = selectionRects();
    const QRect sel = rects.isEmpty() ? QRect(0, 0, 1, 1) : rects.first();
    const bool ours = g_clip.valid && g_clip.text == text;
    QVector<SheetModel::Edit> edits;
    auto put = [&](int r, int c, const QString &raw) {
        if (r >= Sheet::kMaxRows || c >= Sheet::kMaxCols) return;
        QString v = raw;
        if (ours && v.startsWith(QLatin1Char('='))) v = QLatin1Char('=') + Formula::translateReferences(v.mid(1), sel.top() - g_clip.row, sel.left() - g_clip.col);
        const Cell before = p->sheet->cell(r, c);
        Cell after = before;
        after.input = v;
        autoFormat(after);
        if (after.input != before.input || !(after.style == before.style)) edits.push_back({ r, c, before, after });
    };
    if (grid.size() == 1 && grid[0].size() == 1 && (sel.width() > 1 || sel.height() > 1)) {
        for (int r = sel.top(); r <= sel.bottom(); ++r) for (int c = sel.left(); c <= sel.right(); ++c) put(r, c, grid[0][0]);
    } else {
        for (size_t i = 0; i < grid.size(); ++i) for (size_t j = 0; j < grid[i].size(); ++j) put(sel.top() + int(i), sel.left() + int(j), grid[i][j]);
    }
    p->model->pushEdits(edits, tr("Paste"));
}

void ExcelDocument::clearSelection()
{
    Page *p = page();
    if (!p || m_viewMode) return;
    QVector<SheetModel::Edit> edits;
    for (const QRect &r : selectionRects())
        for (int row = r.top(); row <= r.bottom(); ++row)
            for (int col = r.left(); col <= r.right(); ++col) {
                const Cell before = p->sheet->cell(row, col);
                if (before.input.isEmpty()) continue;
                Cell after = before;
                after.input.clear();
                edits.push_back({ row, col, before, after });
            }
    p->model->pushEdits(edits, tr("Clear contents"));
}

void ExcelDocument::styleSelection(const std::function<void(CellStyle &)> &fn, const QString &text)
{
    Page *p = page();
    if (!p || m_viewMode) return;
    QVector<SheetModel::Edit> edits;
    for (const QRect &r : selectionRects())
        for (int row = r.top(); row <= r.bottom(); ++row)
            for (int col = r.left(); col <= r.right(); ++col) {
                const Cell before = p->sheet->cell(row, col);
                Cell after = before;
                fn(after.style);
                if (!(after.style == before.style)) edits.push_back({ row, col, before, after });
            }
    p->model->pushEdits(edits, text);
}

void ExcelDocument::fill(bool down)
{
    Page *p = page();
    if (!p || m_viewMode) return;
    const QList<QRect> rects = selectionRects();
    if (rects.isEmpty()) return;
    const QRect r = rects.first();
    QVector<SheetModel::Edit> edits;
    if (down) {
        const int srcRow = r.height() > 1 ? r.top() : r.top() - 1;
        if (srcRow < 0) return;
        for (int c = r.left(); c <= r.right(); ++c) {
            const Cell src = p->sheet->cell(srcRow, c);
            for (int row = (r.height() > 1 ? r.top() + 1 : r.top()); row <= r.bottom(); ++row) {
                const Cell before = p->sheet->cell(row, c);
                Cell after = src;
                if (after.input.startsWith(QLatin1Char('='))) after.input = QLatin1Char('=') + Formula::translateReferences(src.input.mid(1), row - srcRow, 0);
                if (after.input != before.input || !(after.style == before.style)) edits.push_back({ row, c, before, after });
            }
        }
    } else {
        const int srcCol = r.width() > 1 ? r.left() : r.left() - 1;
        if (srcCol < 0) return;
        for (int row = r.top(); row <= r.bottom(); ++row) {
            const Cell src = p->sheet->cell(row, srcCol);
            for (int c = (r.width() > 1 ? r.left() + 1 : r.left()); c <= r.right(); ++c) {
                const Cell before = p->sheet->cell(row, c);
                Cell after = src;
                if (after.input.startsWith(QLatin1Char('='))) after.input = QLatin1Char('=') + Formula::translateReferences(src.input.mid(1), 0, c - srcCol);
                if (after.input != before.input || !(after.style == before.style)) edits.push_back({ row, c, before, after });
            }
        }
    }
    p->model->pushEdits(edits, down ? tr("Fill down") : tr("Fill right"));
}

void ExcelDocument::autoSum()
{
    Page *p = page();
    if (!p || m_viewMode) return;
    const QModelIndex cur = p->view->currentIndex();
    if (!cur.isValid()) return;
    const int r = cur.row(), c = cur.column();
    int top = r - 1;
    while (top >= 0 && p->sheet->value(top, c).type == CellValue::Number) --top;
    ++top;
    QString formula;
    if (top < r) formula = QStringLiteral("=SUM(%1:%2)").arg(Formula::refName(top, c), Formula::refName(r - 1, c));
    else {
        int left = c - 1;
        while (left >= 0 && p->sheet->value(r, left).type == CellValue::Number) --left;
        ++left;
        if (left < c) formula = QStringLiteral("=SUM(%1:%2)").arg(Formula::refName(r, left), Formula::refName(r, c - 1));
    }
    if (formula.isEmpty()) { emit message(tr("AutoSum needs numbers above or to the left of the active cell."), Toast::Info); return; }
    p->model->setData(cur, formula, Qt::EditRole);
}

void ExcelDocument::sortSelection(bool asc)
{
    Page *p = page();
    if (!p || m_viewMode) return;
    QRect r = selectionRects().value(0);
    const QModelIndex cur = p->view->currentIndex();
    if (r.isEmpty() || !cur.isValid()) return;
    const QSize ext = p->sheet->usedExtent();
    if (r.width() == 1 && r.height() == 1) r = QRect(0, 0, qMax(1, ext.width()), qMax(1, ext.height())); // whole data region
    const int keyCol = qBound(r.left(), cur.column(), r.right());
    int top = r.top();
    if (top == 0 && r.height() > 1 && p->sheet->value(0, keyCol).type == CellValue::Text && p->sheet->value(1, keyCol).type == CellValue::Number) ++top; // header row
    if (top >= r.bottom()) return;
    p->model->changeSnapshot(tr("Sort"), [&](Sheet &s) {
        struct Row { int idx; CellValue key; QVector<Cell> cells; };
        std::vector<Row> rows;
        for (int row = top; row <= r.bottom(); ++row) {
            Row rw{ row, s.value(row, keyCol), {} };
            for (int c = r.left(); c <= r.right(); ++c) rw.cells.push_back(s.cell(row, c));
            rows.push_back(std::move(rw));
        }
        auto less = [&](const Row &a, const Row &b) {
            const bool ae = a.key.type == CellValue::Empty, be = b.key.type == CellValue::Empty;
            if (ae || be) return !ae && be; // empties always last
            int cmp;
            if (a.key.type == CellValue::Number && b.key.type == CellValue::Number) cmp = a.key.num < b.key.num ? -1 : (a.key.num > b.key.num ? 1 : 0);
            else if (a.key.type == CellValue::Number) cmp = -1;
            else if (b.key.type == CellValue::Number) cmp = 1;
            else cmp = QString::compare(a.key.toText(), b.key.toText(), Qt::CaseInsensitive);
            return asc ? cmp < 0 : cmp > 0;
        };
        std::stable_sort(rows.begin(), rows.end(), less);
        for (size_t i = 0; i < rows.size(); ++i)
            for (int c = r.left(); c <= r.right(); ++c) s.setCell(top + int(i), c, rows[i].cells[c - r.left()]);
    });
}

void ExcelDocument::insertDelete(bool rows, bool insert)
{
    Page *p = page();
    if (!p || m_viewMode) return;
    const QRect r = [&] { QList<QRect> l = selectionRects(); return l.isEmpty() ? QRect(0, 0, 1, 1) : l.first(); }();
    const int at = rows ? r.top() : r.left();
    const int count = qBound(1, rows ? r.height() : r.width(), 1000);
    const int delta = insert ? count : -count;
    const QString label = insert ? (rows ? tr("Insert rows") : tr("Insert columns")) : (rows ? tr("Delete rows") : tr("Delete columns"));
    p->model->changeSnapshot(label, [&](Sheet &s) {
        const QHash<quint64, Cell> old = s.cells();
        QHash<quint64, Cell> out;
        for (auto it = old.constBegin(); it != old.constEnd(); ++it) {
            int row = Sheet::keyRow(it.key()), col = Sheet::keyCol(it.key());
            Cell cell = it.value();
            if (cell.input.startsWith(QLatin1Char('='))) cell.input = QLatin1Char('=') + Formula::shiftReferences(cell.input.mid(1), rows, at, delta);
            int &idx = rows ? row : col;
            if (delta > 0) { if (idx >= at) idx += delta; }
            else { if (idx >= at && idx < at + count) continue; if (idx >= at + count) idx -= count; }
            if (row >= Sheet::kMaxRows || col >= Sheet::kMaxCols) continue;
            out.insert(Sheet::key(row, col), cell);
        }
        s.replaceAll(out);
    });
}

// ------------------------------------------------------------------ sheets
void ExcelDocument::addSheet()
{
    if (m_viewMode) return;
    int n = int(m_pages.size()) + 1;
    QString name;
    bool clash = true;
    while (clash) { name = tr("Sheet%1").arg(n++); clash = false; for (auto &p : m_pages) if (p->name == name) clash = true; }
    addPage(name);
    m_tabs->setCurrentIndex(int(m_pages.size()) - 1);
    m_extraDirty = true;
    emit modifiedChanged();
}

void ExcelDocument::deleteSheet()
{
    if (m_viewMode) return;
    if (m_pages.size() <= 1) { emit message(tr("A workbook must contain at least one sheet."), Toast::Info); return; }
    const int i = m_tabs->currentIndex();
    Page *p = m_pages[i].get();
    m_stack->removeWidget(p->view);
    p->view->deleteLater();
    p->model->deleteLater();
    m_tabs->blockSignals(true);
    m_tabs->removeTab(i);
    m_tabs->blockSignals(false);
    Sheet *orphan = p->sheet.release(); // the model may still repaint once; free the sheet when the model is gone
    connect(p->model, &QObject::destroyed, qApp, [orphan] { delete orphan; });
    m_pages.erase(m_pages.begin() + i);
    m_undo.clear();
    m_extraDirty = true;
    m_tabs->setCurrentIndex(qMin(i, int(m_pages.size()) - 1));
    m_stack->setCurrentIndex(m_tabs->currentIndex());
    emit modifiedChanged();
    emit stateChanged();
}

void ExcelDocument::renameSheet()
{
    Page *p = page();
    if (!p || m_viewMode) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Rename sheet"), tr("Sheet name:"), QLineEdit::Normal, p->name, &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    name.remove(QRegularExpression(QStringLiteral("[\\\\/?*\\[\\]:]")));
    name = name.left(31);
    p->name = name;
    m_tabs->setTabText(m_tabs->currentIndex(), name);
    m_extraDirty = true;
    emit modifiedChanged();
}

// ------------------------------------------------------------------ analytics
ExcelDocument::SelStats ExcelDocument::selectionStats() const
{
    SelStats st;
    Page *p = page();
    if (!p) return st;
    bool first = true;
    auto visit = [&](int r, int c) {
        const Cell *cell = p->sheet->find(r, c);
        if (!cell || cell->input.isEmpty()) return;
        ++st.cells;
        const CellValue v = p->sheet->value(r, c);
        if (v.type != CellValue::Number) return;
        ++st.numeric;
        st.sum += v.num;
        if (first) { st.min = st.max = v.num; first = false; } else { st.min = qMin(st.min, v.num); st.max = qMax(st.max, v.num); }
    };
    for (const QRect &r : selectionRects()) {
        if (qint64(r.width()) * r.height() > 200000) { // huge range: walk only the stored cells
            for (auto it = p->sheet->cells().constBegin(); it != p->sheet->cells().constEnd(); ++it) {
                const int row = Sheet::keyRow(it.key()), col = Sheet::keyCol(it.key());
                if (r.contains(col, row)) visit(row, col);
            }
        } else {
            for (int row = r.top(); row <= r.bottom(); ++row) for (int col = r.left(); col <= r.right(); ++col) visit(row, col);
        }
    }
    return st;
}

// Grows a rectangle around (row, col) while the border cells next to it contain data.
QRect ExcelDocument::dataRegionAround(Page *p, int row, int col) const
{
    int t = row, b = row, l = col, r = col;
    auto filled = [&](int rr, int cc) { const Cell *c = p->sheet->find(rr, cc); return rr >= 0 && cc >= 0 && c && !c->input.isEmpty(); };
    bool grew = true;
    for (int guard = 0; grew && guard < 600; ++guard) {
        grew = false;
        bool any = false;
        for (int c = l - 1; c <= r + 1; ++c) any = any || filled(t - 1, c);
        if (any && t > 0) { --t; grew = true; }
        any = false;
        for (int c = l - 1; c <= r + 1; ++c) any = any || filled(b + 1, c);
        if (any) { ++b; grew = true; }
        any = false;
        for (int rr = t - 1; rr <= b + 1; ++rr) any = any || filled(rr, l - 1);
        if (any && l > 0) { --l; grew = true; }
        any = false;
        for (int rr = t - 1; rr <= b + 1; ++rr) any = any || filled(rr, r + 1);
        if (any) { ++r; grew = true; }
    }
    return QRect(QPoint(l, t), QPoint(r, b));
}

ChartData ExcelDocument::buildChart(Page *p, const QRect &r) const
{
    ChartData d;
    const int rows = qMin(r.height(), 501), cols = qMin(r.width(), 13);
    if (rows < 1 || cols < 1) return d;
    const int top = r.top(), left = r.left();
    auto isNum = [&](int row, int col) { return p->sheet->value(row, col).type == CellValue::Number; };
    auto isText = [&](int row, int col) { return p->sheet->value(row, col).type == CellValue::Text; };
    bool header = false;
    if (rows >= 2) {
        int text = 0, num = 0;
        for (int c = 0; c < cols; ++c) { text += isText(top, left + c); num += isNum(top + 1, left + c); }
        header = text > 0 && num > 0;
    }
    const int dataTop = top + (header ? 1 : 0), nRows = rows - (header ? 1 : 0);
    if (nRows < 1) return d;
    bool catCol = false;
    if (cols >= 2) { int t = 0; for (int i = 0; i < nRows; ++i) t += isText(dataTop + i, left); catCol = t > 0 && t * 2 >= nRows; }
    const int firstSeries = catCol ? 1 : 0;
    for (int i = 0; i < nRows; ++i) d.categories << (catCol ? p->sheet->display(dataTop + i, left) : QString::number(i + 1));
    for (int c = firstSeries; c < cols; ++c) {
        ChartSeries s;
        s.name = header ? p->sheet->display(top, left + c) : tr("Series %1").arg(c - firstSeries + 1);
        if (s.name.isEmpty()) s.name = tr("Series %1").arg(c - firstSeries + 1);
        for (int i = 0; i < nRows; ++i) { const CellValue v = p->sheet->value(dataTop + i, left + c); s.values << (v.type == CellValue::Number ? v.num : 0.0); }
        d.series << s;
    }
    d.title = p->name + QStringLiteral("  ·  ") + Formula::refName(top, left) + QLatin1Char(':') + Formula::refName(top + rows - 1, left + cols - 1);
    return d;
}

void ExcelDocument::showChart(ChartView::Type type)
{
    Page *p = page();
    if (!p) return;
    QRect r = selectionRects().value(0);
    if (r.isEmpty()) return;
    if (r.width() * r.height() == 1) r = dataRegionAround(p, r.top(), r.left());
    ChartData d = buildChart(p, r);
    bool hasNumbers = false;
    for (const ChartSeries &s : d.series) for (double v : s.values) hasNumbers = hasNumbers || v != 0.0;
    if (d.isEmpty() || !hasNumbers) { emit message(tr("Select a range that contains numbers to create a chart."), Toast::Info); return; }
    m_chartRect = r;
    m_chartPage = m_tabs->currentIndex();
    const bool wasHidden = !m_chart->isVisible();
    m_chart->setChart(d, type);
    m_chart->view()->setData(d, true); // animate the reveal
    if (wasHidden) {
        m_chart->show();
        m_split->setSizes({ qMax(300, int(width() * 0.58)), qMax(320, int(width() * 0.42)) });
    }
}

void ExcelDocument::refreshChart()
{
    if (!m_chart->isVisible() || m_chartPage < 0 || m_chartPage >= int(m_pages.size())) return;
    m_chart->view()->setData(buildChart(m_pages[m_chartPage].get(), m_chartRect), false); // live update without replaying the animation
}

void ExcelDocument::hideChart() { m_chart->hide(); }
bool ExcelDocument::chartVisible() const { return m_chart->isVisible(); }

void ExcelDocument::exportChartImage()
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QString path = QFileDialog::getSaveFileName(this, tr("Export chart"), dir + QLatin1String("/chart.png"), tr("PNG image (*.png)"));
    if (path.isEmpty()) return;
    if (m_chart->view()->grab().save(path, "PNG")) emit message(tr("Chart exported."), Toast::Success);
    else emit message(tr("Could not write the image."), Toast::Error);
}

void ExcelDocument::showStatistics()
{
    Page *p = page();
    if (!p) return;
    std::vector<double> v;
    for (const QRect &r : selectionRects()) {
        const QRect rr = (r.width() * r.height() == 1) ? dataRegionAround(p, r.top(), r.left()) : r;
        for (int row = rr.top(); row <= rr.bottom() && v.size() < 2000000; ++row)
            for (int col = rr.left(); col <= rr.right(); ++col) { const CellValue c = p->sheet->value(row, col); if (c.type == CellValue::Number) v.push_back(c.num); }
    }
    if (v.empty()) { emit message(tr("Select cells that contain numbers to see statistics."), Toast::Info); return; }
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    double sum = 0;
    for (double x : v) sum += x;
    const double mean = sum / n, median = n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
    double sq = 0;
    for (double x : v) sq += (x - mean) * (x - mean);
    const double var = n > 1 ? sq / (n - 1) : 0, sd = std::sqrt(var);
    auto fmt = [](double x) { return QString::number(x, 'g', 10); };

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Statistics"));
    dlg.setMinimumWidth(560);
    auto *col = new QVBoxLayout(&dlg);
    col->setContentsMargins(20, 18, 20, 18);
    auto *html = new QLabel(tr("<table cellpadding='5'>"
        "<tr><td>Count</td><td><b>%1</b></td><td>&nbsp;&nbsp;Sum</td><td><b>%2</b></td></tr>"
        "<tr><td>Mean</td><td><b>%3</b></td><td>&nbsp;&nbsp;Median</td><td><b>%4</b></td></tr>"
        "<tr><td>Minimum</td><td><b>%5</b></td><td>&nbsp;&nbsp;Maximum</td><td><b>%6</b></td></tr>"
        "<tr><td>Range</td><td><b>%7</b></td><td>&nbsp;&nbsp;Std. deviation (sample)</td><td><b>%8</b></td></tr>"
        "<tr><td>Variance</td><td><b>%9</b></td><td></td><td></td></tr></table>")
        .arg(n).arg(fmt(sum), fmt(mean), fmt(median), fmt(v.front()), fmt(v.back())).arg(fmt(v.back() - v.front()), fmt(sd)).arg(fmt(var)), &dlg);
    html->setTextFormat(Qt::RichText);
    col->addWidget(html);

    // Distribution histogram.
    const int bins = qBound(5, int(std::sqrt(double(n))), 12);
    const double lo = v.front(), hi = v.back(), width = (hi - lo) > 0 ? (hi - lo) / bins : 1.0;
    ChartData hist;
    hist.title = tr("Distribution of %1 values").arg(n);
    ChartSeries counts;
    counts.name = tr("Count");
    counts.values = QVector<double>(bins, 0.0);
    for (double x : v) counts.values[qMin(bins - 1, int((x - lo) / width))] += 1;
    for (int i = 0; i < bins; ++i) hist.categories << QStringLiteral("%1").arg(QString::number(lo + i * width, 'g', 4));
    hist.series << counts;
    auto *chart = new ChartView(&dlg);
    chart->setData(hist, true);
    chart->setMinimumHeight(260);
    col->addWidget(chart);
    auto *close = new PillButton(tr("Close"), QString(), true, &dlg);
    connect(close, &PillButton::clicked, &dlg, &QDialog::accept);
    col->addWidget(close, 0, Qt::AlignRight);
    dlg.exec();
}

// ------------------------------------------------------------------ find & replace
void ExcelDocument::findReplace()
{
    if (m_findDlg) { m_findDlg->show(); m_findDlg->raise(); return; }
    m_findDlg = new QDialog(this);
    m_findDlg->setAttribute(Qt::WA_DeleteOnClose);
    m_findDlg->setWindowTitle(tr("Find and replace"));
    connect(m_findDlg, &QObject::destroyed, this, [this] { m_findDlg = nullptr; });
    auto *form = new QFormLayout(m_findDlg);
    auto *find = new QLineEdit(m_findDlg), *repl = new QLineEdit(m_findDlg);
    auto *matchCase = new QCheckBox(tr("Match case"), m_findDlg);
    auto *status = new QLabel(m_findDlg);
    auto *row = new QHBoxLayout;
    auto *next = new PillButton(tr("Find next"), QString(), true, m_findDlg);
    auto *rep = new PillButton(tr("Replace"), QString(), false, m_findDlg);
    auto *all = new PillButton(tr("Replace all"), QString(), false, m_findDlg);
    row->addWidget(next); row->addWidget(rep); row->addWidget(all);
    form->addRow(tr("Find what:"), find);
    form->addRow(tr("Replace with:"), repl);
    form->addRow(matchCase);
    form->addRow(row);
    form->addRow(status);
    auto matches = [matchCase](const QString &hay, const QString &needle) { return hay.contains(needle, matchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive); };
    connect(next, &PillButton::clicked, m_findDlg, [=] {
        Page *p = page();
        if (!p || find->text().isEmpty()) return;
        QList<quint64> keys = p->sheet->cells().keys();
        std::sort(keys.begin(), keys.end());
        const QModelIndex cur = p->view->currentIndex();
        const quint64 curKey = cur.isValid() ? Sheet::key(cur.row(), cur.column()) : 0;
        for (int pass = 0; pass < 2; ++pass)
            for (quint64 k : keys) {
                if (pass == 0 && k <= curKey) continue;
                if (matches(p->sheet->cells().value(k).input, find->text()) || matches(p->sheet->display(Sheet::keyRow(k), Sheet::keyCol(k)), find->text())) {
                    goToCell(Sheet::keyRow(k), Sheet::keyCol(k));
                    status->setText(tr("Found in %1").arg(Formula::refName(Sheet::keyRow(k), Sheet::keyCol(k))));
                    return;
                }
            }
        status->setText(tr("Not found"));
    });
    auto replaceIn = [=](bool onlyCurrent) {
        Page *p = page();
        if (!p || m_viewMode || find->text().isEmpty()) return;
        QVector<SheetModel::Edit> edits;
        auto handle = [&](int r, int c) {
            const Cell before = p->sheet->cell(r, c);
            if (!matches(before.input, find->text())) return;
            Cell after = before;
            after.input.replace(find->text(), repl->text(), matchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive);
            edits.push_back({ r, c, before, after });
        };
        if (onlyCurrent) { const QModelIndex cur = p->view->currentIndex(); if (cur.isValid()) handle(cur.row(), cur.column()); }
        else for (auto it = p->sheet->cells().constBegin(); it != p->sheet->cells().constEnd(); ++it) handle(Sheet::keyRow(it.key()), Sheet::keyCol(it.key()));
        p->model->pushEdits(edits, tr("Replace"));
        status->setText(tr("%1 replaced").arg(edits.size()));
    };
    connect(rep, &PillButton::clicked, m_findDlg, [=] { replaceIn(true); next->clicked(); });
    connect(all, &PillButton::clicked, m_findDlg, [=] { replaceIn(false); });
    m_findDlg->show();
    find->setFocus();
}

// ------------------------------------------------------------------ files
void ExcelDocument::load(const QString &path, bool viewMode)
{
    m_path = path;
    m_viewMode = viewMode;
    applyViewMode();
    emit titleChanged();
    FileIO::readAsync(path, this, [this, path](const QByteArray &data, const QString &err) {
        QString perr = err;
        if (!err.isEmpty() || !parseBytes(path, data, &perr)) { emit loadFinished(false, perr); return; }
        m_undo.clear();
        m_undo.setClean();
        m_extraDirty = false;
        emit titleChanged();
        emit modifiedChanged();
        emit viewModeChanged();
        emit loadFinished(true, QString());
    });
}

bool ExcelDocument::parseBytes(const QString &path, const QByteArray &data, QString *error)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "csv") {
        Page *p = page();
        return p && CsvIO::read(data, *p->sheet, error) && (p->model->refreshAll(), p->name = QFileInfo(path).completeBaseName().left(31), m_tabs->setTabText(0, p->name), true);
    }
    QList<XlsxIO::SheetData> sheets;
    if (!XlsxIO::read(data, sheets, error)) return false;
    // Replace everything with the loaded sheets.
    for (int i = 0; i < sheets.size(); ++i) {
        Page *p = i < int(m_pages.size()) ? m_pages[i].get() : addPage(sheets[i].name);
        p->name = sheets[i].name;
        m_tabs->setTabText(i, p->name);
        p->sheet->replaceAll(sheets[i].cells);
        p->model->refreshAll();
    }
    m_tabs->setCurrentIndex(0);
    return true;
}

bool ExcelDocument::writeFile(const QString &path, QString *error)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "pdf") return exportPdf(path, error);
    QByteArray bytes;
    if (ext == "csv") { if (page()) bytes = CsvIO::write(*page()->sheet); }
    else if (ext == "xlsx") {
        QStringList names;
        QList<const Sheet *> sheets;
        for (auto &p : m_pages) { names << p->name; sheets << p->sheet.get(); }
        bytes = XlsxIO::write(names, sheets);
        if (bytes.isEmpty()) { if (error) *error = tr("Could not create the XLSX file."); return false; }
    } else { if (error) *error = tr("Unsupported format \"%1\".").arg(ext); return false; }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) { if (error) *error = f.errorString(); return false; }
    return true;
}

bool ExcelDocument::saveToPath(const QString &path)
{
    QString err;
    if (!writeFile(path, &err)) { emit message(tr("Save failed: %1").arg(err), Toast::Error); return false; }
    m_path = path;
    m_undo.setClean();
    m_extraDirty = false;
    FileManager::instance().addRecent(path);
    emit titleChanged();
    emit modifiedChanged();
    emit message(QFileInfo(path).suffix().toLower() == "csv" ? tr("Saved the active sheet as CSV") : tr("Saved %1").arg(QFileInfo(path).fileName()), Toast::Success);
    return true;
}

bool ExcelDocument::save()
{
    if (m_path.isEmpty()) return saveAs();
    if (m_viewMode) setViewMode(false);
    return saveToPath(m_path);
}

bool ExcelDocument::saveAs()
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    QString selected;
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), dir + QLatin1Char('/') + (m_path.isEmpty() ? m_untitled : QFileInfo(m_path).completeBaseName()),
                                                tr("Excel Workbook (*.xlsx);;CSV (*.csv)"), &selected);
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty()) path += selected.contains(QLatin1String("csv")) ? QStringLiteral(".csv") : QStringLiteral(".xlsx");
    QSettings().setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    return saveToPath(path);
}

bool ExcelDocument::exportTo(const QString &path)
{
    QString err;
    if (!writeFile(path, &err)) { emit message(tr("Export failed: %1").arg(err), Toast::Error); return false; }
    emit message(tr("Exported %1").arg(QFileInfo(path).fileName()), Toast::Success);
    return true;
}

void ExcelDocument::exportDialog(const QString &suffix)
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QString path = QFileDialog::getSaveFileName(this, tr("Export"), dir + QLatin1Char('/') + (m_path.isEmpty() ? m_untitled : QFileInfo(m_path).completeBaseName()) + QLatin1Char('.') + suffix,
                                                      QStringLiteral("*.%1").arg(suffix));
    if (!path.isEmpty()) exportTo(path);
}

// Prints the used range of the active sheet (landscape A4), splitting wide sheets into column bands.
bool ExcelDocument::exportPdf(const QString &path, QString *error)
{
    Page *p = page();
    if (!p) return false;
    QPdfWriter pdf(path);
    pdf.setResolution(96);
    pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageOrientation(QPageLayout::Landscape);
    pdf.setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
    QPainter pa;
    if (!pa.begin(&pdf)) { if (error) *error = tr("Cannot write %1").arg(path); return false; }
    const QSize ext = p->sheet->usedExtent();
    const int pageW = pdf.width(), pageH = pdf.height(), rowH = 22, rowsPerPage = qMax(1, pageH / rowH);
    std::vector<int> widths;
    for (int c = 0; c < ext.width(); ++c) widths.push_back(qMax(40, int(p->view->columnWidth(c) * 100.0 / m_zoom)));
    pa.setFont(QFont(QStringLiteral("Arial"), 9));
    bool first = true;
    int colStart = 0;
    while (colStart < ext.width() || (colStart == 0 && first)) {
        int colEnd = colStart, acc = 0;
        while (colEnd < ext.width() && (acc + widths[colEnd] <= pageW || colEnd == colStart)) acc += widths[colEnd++];
        for (int rowStart = 0; rowStart < qMax(1, ext.height()); rowStart += rowsPerPage) {
            if (!first) pdf.newPage();
            first = false;
            for (int r = rowStart; r < qMin(ext.height(), rowStart + rowsPerPage); ++r) {
                int x = 0;
                for (int c = colStart; c < colEnd; ++c) {
                    const QRect cell(x, (r - rowStart) * rowH, widths[c], rowH);
                    const Cell *cl = p->sheet->find(r, c);
                    if (cl && cl->style.bg.isValid()) pa.fillRect(cell, cl->style.bg);
                    pa.setPen(QColor(200, 200, 205));
                    pa.drawRect(cell);
                    pa.setPen(cl && cl->style.fg.isValid() ? cl->style.fg : QColor(30, 30, 40));
                    QFont f = pa.font(); f.setBold(cl && cl->style.bold); f.setItalic(cl && cl->style.italic); pa.setFont(f);
                    const CellValue v = p->sheet->value(r, c);
                    const bool right = (cl && cl->style.align == 3) || (!cl || cl->style.align == 0 ? v.type == CellValue::Number : false);
                    pa.drawText(cell.adjusted(4, 0, -4, 0), (right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, pa.fontMetrics().elidedText(p->sheet->display(r, c), Qt::ElideRight, cell.width() - 8));
                    x += widths[c];
                }
            }
        }
        colStart = colEnd;
        if (ext.width() == 0) break;
    }
    pa.end();
    return true;
}

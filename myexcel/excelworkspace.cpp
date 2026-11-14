#include "excelworkspace.h"

#include <QActionGroup>
#include <QComboBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QTableView>
#include <cmath>

#include "core/thememanager.h"
#include "myexcel/spreadsheet/chartpanel.h"
#include "myexcel/spreadsheet/exceldocument.h"
#include "ui/ribbon/colormenu.h"
#include "ui/ribbon/ribbon.h"
#include "ui/widgets/iconbutton.h"
#include "ui/widgets/statusbar.h"
#include "ui/widgets/themeutil.h"

ExcelWorkspace::ExcelWorkspace(QWidget *parent) : Workspace(ApplicationManager::AppId::Excel, parent)
{
    buildFile();
    buildHome();
    buildInsert();
    buildFormulas();
    buildData();
    buildView();
    buildStatusBar();
    m_ribbon->setCurrentIndex(1);
}

ExcelDocument *ExcelWorkspace::excelDoc() const { return qobject_cast<ExcelDocument *>(currentDocument()); }
DocumentView *ExcelWorkspace::createBlankDocument() { return new ExcelDocument; }
QAction *ExcelWorkspace::editAction(const QString &t, const QString &i, const QKeySequence &sc, bool c) { return makeAction(t, i, sc, c, true); }

void ExcelWorkspace::buildFile()
{
    RibbonPage *page = m_ribbon->addPage(tr("File"), true);
    RibbonGroup *g = page->addGroup(tr("Workbook"));
    g->addLarge(m_actNew);
    g->addLarge(m_actOpen);
    g->addLarge(m_actSave);
    g->addLarge(m_actSaveAs);
    RibbonGroup *ex = page->addGroup(tr("Export"));
    const struct { const char *label, *suffix; } formats[] = { { "PDF", "pdf" }, { "Excel (.xlsx)", "xlsx" }, { "CSV (active sheet)", "csv" } };
    ex->beginColumn();
    for (const auto &f : formats) {
        QAction *a = makeAction(tr(f.label), QStringLiteral("export"));
        const QString suffix = QLatin1String(f.suffix);
        connect(a, &QAction::triggered, this, [this, suffix] { if (auto *d = excelDoc()) d->exportDialog(suffix); });
        ex->addSmall(a);
    }
    page->addGroup(tr("Close"))->addLarge(m_actClose);
}

void ExcelWorkspace::buildHome()
{
    RibbonPage *page = m_ribbon->addPage(tr("Home"));
    m_undo = makeAction(tr("Undo"), QStringLiteral("undo"), QKeySequence::Undo, false, true);
    m_redo = makeAction(tr("Redo"), QStringLiteral("redo"), QKeySequence::Redo, false, true);
    m_cut = editAction(tr("Cut"), QStringLiteral("cut"), QKeySequence::Cut);
    m_copy = makeAction(tr("Copy"), QStringLiteral("copy"), QKeySequence::Copy);
    QAction *paste = editAction(tr("Paste"), QStringLiteral("paste"), QKeySequence::Paste);
    connect(m_undo, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->undo(); });
    connect(m_redo, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->redo(); });
    connect(m_cut, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->cut(); });
    connect(m_copy, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->copy(); });
    connect(paste, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->paste(); });
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
    m_bold = editAction(tr("Bold"), QStringLiteral("bold"), QKeySequence::Bold, true);
    m_italic = editAction(tr("Italic"), QStringLiteral("italic"), QKeySequence::Italic, true);
    m_underline = editAction(tr("Underline"), QStringLiteral("underline"), QKeySequence::Underline, true);
    QAction *textColor = editAction(tr("Font color"), QStringLiteral("text-color"));
    QAction *fillColor = editAction(tr("Fill color"), QStringLiteral("highlight"));
    QAction *clearFmt = editAction(tr("Clear formatting"), QStringLiteral("clear-format"));
    connect(m_bold, &QAction::triggered, this, [this](bool on) { if (auto *d = excelDoc()) d->styleSelection([on](CellStyle &s) { s.bold = on; }, tr("Bold")); });
    connect(m_italic, &QAction::triggered, this, [this](bool on) { if (auto *d = excelDoc()) d->styleSelection([on](CellStyle &s) { s.italic = on; }, tr("Italic")); });
    connect(m_underline, &QAction::triggered, this, [this](bool on) { if (auto *d = excelDoc()) d->styleSelection([on](CellStyle &s) { s.underline = on; }, tr("Underline")); });
    connect(clearFmt, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->styleSelection([](CellStyle &s) { s = CellStyle(); }, tr("Clear formatting")); });
    auto *colorMenu = new ColorMenu(tr("Automatic"), this);
    auto *fillMenu = new ColorMenu(tr("No fill"), this);
    connect(textColor, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) { const QColor c = m_textColor; d->styleSelection([c](CellStyle &s) { s.fg = c; }, tr("Font color")); } });
    connect(colorMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { m_textColor = c; m_colorBtn->setIndicatorColor(c); if (auto *d = excelDoc()) d->styleSelection([c](CellStyle &s) { s.fg = c; }, tr("Font color")); });
    connect(colorMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = excelDoc()) d->styleSelection([](CellStyle &s) { s.fg = QColor(); }, tr("Font color")); });
    connect(fillColor, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) { const QColor c = m_fillColor; d->styleSelection([c](CellStyle &s) { s.bg = c; }, tr("Fill color")); } });
    connect(fillMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { m_fillColor = c; m_fillBtn->setIndicatorColor(c); if (auto *d = excelDoc()) d->styleSelection([c](CellStyle &s) { s.bg = c; }, tr("Fill color")); });
    connect(fillMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = excelDoc()) d->styleSelection([](CellStyle &s) { s.bg = QColor(); }, tr("Fill color")); });
    RibbonGroup *font = page->addGroup(tr("Font"));
    auto small = [&](QAction *a, QMenu *m = nullptr) { auto *b = new RibbonButton(a, RibbonButton::Size::SmallIcon, font); if (m) { b->setMenu(m); b->setSplit(true); b->setFixedSize(b->sizeHint()); } return b; };
    m_colorBtn = small(textColor, colorMenu);
    m_fillBtn = small(fillColor, fillMenu);
    m_colorBtn->setIndicatorColor(m_textColor);
    m_fillBtn->setIndicatorColor(m_fillColor);
    font->beginColumn();
    font->addRow({ small(m_bold), small(m_italic), small(m_underline) });
    font->addRow({ m_colorBtn, m_fillBtn, small(clearFmt) });

    // Alignment
    m_alignL = editAction(tr("Align left"), QStringLiteral("align-left"), {}, true);
    m_alignC = editAction(tr("Center"), QStringLiteral("align-center"), {}, true);
    m_alignR = editAction(tr("Align right"), QStringLiteral("align-right"), {}, true);
    auto *grp = new QActionGroup(this);
    grp->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    for (QAction *a : { m_alignL, m_alignC, m_alignR }) grp->addAction(a);
    auto setAlign = [this](int v, bool on) { if (auto *d = excelDoc()) d->styleSelection([v, on](CellStyle &s) { s.align = on ? v : 0; }, tr("Alignment")); };
    connect(m_alignL, &QAction::triggered, this, [setAlign](bool on) { setAlign(1, on); });
    connect(m_alignC, &QAction::triggered, this, [setAlign](bool on) { setAlign(2, on); });
    connect(m_alignR, &QAction::triggered, this, [setAlign](bool on) { setAlign(3, on); });
    RibbonGroup *align = page->addGroup(tr("Alignment"));
    align->beginColumn();
    align->addRow({ small(m_alignL), small(m_alignC), small(m_alignR) });

    // Number
    m_numFmt = new QComboBox;
    m_numFmt->setFixedSize(132, 26);
    m_numFmt->addItems({ tr("General"), tr("Number"), tr("Percentage"), tr("Currency"), tr("Date"), tr("Text") });
    connect(m_numFmt, &QComboBox::activated, this, [this](int i) { if (auto *d = excelDoc()) { d->styleSelection([i](CellStyle &s) { s.numFmt = i; }, tr("Number format")); d->activated(); } });
    QAction *pct = editAction(tr("Percent"), QStringLiteral("percent"));
    QAction *cur = editAction(tr("Currency"), QStringLiteral("currency"));
    connect(pct, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->styleSelection([](CellStyle &s) { s.numFmt = 2; }, tr("Percent")); });
    connect(cur, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->styleSelection([](CellStyle &s) { s.numFmt = 3; }, tr("Currency")); });
    RibbonGroup *num = page->addGroup(tr("Number"));
    num->beginColumn();
    num->addRow({ m_numFmt });
    num->addRow({ small(cur), small(pct) });

    // Editing
    QAction *sum = editAction(tr("AutoSum"), QStringLiteral("sigma"));
    QAction *fdown = editAction(tr("Fill down"), QStringLiteral("fill-down"), QKeySequence(QStringLiteral("Ctrl+D")));
    QAction *fright = editAction(tr("Fill right"), QStringLiteral("fill-right"), QKeySequence(QStringLiteral("Ctrl+R")));
    QAction *clear = editAction(tr("Clear"), QStringLiteral("trash"));
    connect(sum, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->autoSum(); });
    connect(fdown, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->fill(true); });
    connect(fright, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->fill(false); });
    connect(clear, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->clearSelection(); });
    RibbonGroup *edit = page->addGroup(tr("Editing"));
    edit->addLarge(sum);
    edit->beginColumn();
    edit->addSmall(fdown);
    edit->addSmall(fright);
    edit->addSmall(clear);
    QAction *asc = editAction(tr("Sort A → Z"), QStringLiteral("sort-az"));
    QAction *desc = editAction(tr("Sort Z → A"), QStringLiteral("sort-za"));
    QAction *find = makeAction(tr("Find &\nReplace"), QStringLiteral("find"), QKeySequence::Find);
    connect(asc, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->sortSelection(true); });
    connect(desc, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->sortSelection(false); });
    connect(find, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->findReplace(); });
    RibbonGroup *sf = page->addGroup(tr("Sort & find"));
    sf->addLarge(find);
    sf->beginColumn();
    sf->addSmall(asc);
    sf->addSmall(desc);
}

void ExcelWorkspace::buildInsert()
{
    RibbonPage *page = m_ribbon->addPage(tr("Insert"));
    const struct { const char *t, *i; bool rows, insert; } ops[] = { { "Insert rows", "row-below", true, true }, { "Insert columns", "col-right", false, true },
                                                                       { "Delete rows", "row-delete", true, false }, { "Delete columns", "col-delete", false, false } };
    RibbonGroup *cells = page->addGroup(tr("Cells"));
    for (int i = 0; i < 4; ++i) {
        if (i == 0 || i == 2) cells->beginColumn();
        QAction *a = editAction(tr(ops[i].t), QLatin1String(ops[i].i));
        const bool rows = ops[i].rows, insert = ops[i].insert;
        connect(a, &QAction::triggered, this, [this, rows, insert] { if (auto *d = excelDoc()) d->insertDelete(rows, insert); });
        cells->addSmall(a);
    }
    QAction *add = editAction(tr("New\nSheet"), QStringLiteral("new-file"));
    QAction *del = editAction(tr("Delete sheet"), QStringLiteral("trash"));
    QAction *ren = editAction(tr("Rename sheet"), QStringLiteral("edit"));
    connect(add, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->addSheet(); });
    connect(del, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->deleteSheet(); });
    connect(ren, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->renameSheet(); });
    // Charts
    RibbonGroup *charts = page->addGroup(tr("Charts"));
    const struct { const char *t, *icon; ChartView::Type type; } kinds[] = {
        { "Column", "chart-column", ChartView::Column }, { "Line", "chart-line", ChartView::Line }, { "Area", "chart-area", ChartView::Area },
        { "Pie", "chart-pie", ChartView::Pie }, { "Donut", "chart-donut", ChartView::Donut } };
    for (const auto &k : kinds) {
        QAction *a = makeAction(tr(k.t), QLatin1String(k.icon));
        const ChartView::Type type = k.type;
        connect(a, &QAction::triggered, this, [this, type] { if (auto *d = excelDoc()) d->showChart(type); });
        charts->addLarge(a);
    }
    RibbonGroup *ctools = page->addGroup(tr("Chart tools"));
    QAction *exp = makeAction(tr("Export\nChart"), QStringLiteral("image-export"));
    QAction *hide = makeAction(tr("Hide\nChart"), QStringLiteral("close"));
    connect(exp, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->exportChartImage(); });
    connect(hide, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->hideChart(); });
    ctools->addLarge(exp);
    ctools->addLarge(hide);

    RibbonGroup *sheets = page->addGroup(tr("Sheets"));
    sheets->addLarge(add);
    sheets->beginColumn();
    sheets->addSmall(del);
    sheets->addSmall(ren);
}

void ExcelWorkspace::buildFormulas()
{
    RibbonPage *page = m_ribbon->addPage(tr("Formulas"));
    QAction *sum = editAction(tr("AutoSum"), QStringLiteral("sigma"));
    connect(sum, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->autoSum(); });
    page->addGroup(tr("Quick"))->addLarge(sum);

    struct Cat { const char *name; QStringList fns; };
    const Cat cats[] = { { "Math", { "SUM", "AVERAGE", "MIN", "MAX", "ROUND", "ABS", "SQRT", "POWER", "MOD", "INT" } },
                         { "Statistical", { "COUNT", "COUNTA" } },
                         { "Logical", { "IF", "AND", "OR", "NOT" } },
                         { "Text", { "CONCAT", "LEFT", "RIGHT", "LEN", "UPPER", "LOWER", "TRIM" } },
                         { "Date & Time", { "TODAY", "NOW" } } };
    RibbonGroup *lib = page->addGroup(tr("Function library"));
    for (const Cat &c : cats) {
        auto *menu = new QMenu(this);
        for (const QString &fn : c.fns)
            connect(menu->addAction(fn), &QAction::triggered, this, [this, fn] { if (auto *d = excelDoc()) d->focusFormulaBar(QLatin1Char('=') + fn + QLatin1Char('(')); });
        QAction *a = editAction(tr(c.name), QStringLiteral("function"));
        lib->addLarge(a, menu);
    }
}

void ExcelWorkspace::buildData()
{
    RibbonPage *page = m_ribbon->addPage(tr("Data"));
    QAction *asc = editAction(tr("Sort\nA → Z"), QStringLiteral("sort-az"));
    QAction *desc = editAction(tr("Sort\nZ → A"), QStringLiteral("sort-za"));
    connect(asc, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->sortSelection(true); });
    connect(desc, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->sortSelection(false); });
    RibbonGroup *s = page->addGroup(tr("Sort"));
    s->addLarge(asc);
    s->addLarge(desc);
    QAction *fd = editAction(tr("Fill\nDown"), QStringLiteral("fill-down"));
    QAction *fr = editAction(tr("Fill\nRight"), QStringLiteral("fill-right"));
    connect(fd, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->fill(true); });
    connect(fr, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->fill(false); });
    RibbonGroup *f = page->addGroup(tr("Fill"));
    f->addLarge(fd);
    f->addLarge(fr);
    QAction *stats = makeAction(tr("Statistics"), QStringLiteral("stats"));
    connect(stats, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->showStatistics(); });
    page->addGroup(tr("Analyze"))->addLarge(stats);
}

void ExcelWorkspace::buildView()
{
    RibbonPage *page = m_ribbon->addPage(tr("View"));
    QAction *zin = makeAction(tr("Zoom in"), QStringLiteral("zoom-in"));
    QAction *zout = makeAction(tr("Zoom out"), QStringLiteral("zoom-out"));
    QAction *z100 = makeAction(tr("100%"), QStringLiteral("zoom-in"));
    connect(zin, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->setZoom(d->zoom() + 10); });
    connect(zout, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->setZoom(d->zoom() - 10); });
    connect(z100, &QAction::triggered, this, [this] { if (auto *d = excelDoc()) d->setZoom(100); });
    RibbonGroup *z = page->addGroup(tr("Zoom"));
    z->addLarge(zin);
    z->addLarge(zout);
    z->beginColumn();
    z->addSmall(z100);
    m_grid = makeAction(tr("Gridlines"), QStringLiteral("grid"), {}, true);
    m_grid->setChecked(true);
    connect(m_grid, &QAction::triggered, this, [this](bool on) { if (auto *d = excelDoc()) d->setGridlines(on); });
    page->addGroup(tr("Show"))->addLarge(m_grid);
    m_readMode = makeAction(tr("Read\nMode"), QStringLiteral("eye"), {}, true);
    connect(m_readMode, &QAction::triggered, this, [this](bool on) { if (auto *d = excelDoc()) d->setViewMode(on); });
    page->addGroup(tr("Views"))->addLarge(m_readMode);
}

void ExcelWorkspace::buildStatusBar()
{
    m_selLabel = m_status->addLeftText();
    m_statLabel = m_status->addLeftText();
    m_modeLabel = m_status->addLeftText();
    auto *out = new IconButton(QStringLiteral("zoom-out"), tr("Zoom out"));
    auto *in = new IconButton(QStringLiteral("zoom-in"), tr("Zoom in"));
    for (IconButton *b : { out, in }) { b->setOnChrome(true); b->setFixedSize(26, 24); }
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(40, 300);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(140);
    m_zoomSlider->setAccessibleName(tr("Zoom"));
    m_zoomLabel = new QLabel(QStringLiteral("100%"));
    m_zoomLabel->setFixedWidth(44);
    m_zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setTextColor(m_zoomLabel, ThemeManager::instance().palette().chromeText);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { setTextColor(m_zoomLabel, ThemeManager::instance().palette().chromeText); });
    m_status->addRight(out);
    m_status->addRight(m_zoomSlider);
    m_status->addRight(in);
    m_status->addRight(m_zoomLabel);
    connect(out, &IconButton::clicked, this, [this] { if (auto *d = excelDoc()) d->setZoom(d->zoom() - 10); });
    connect(in, &IconButton::clicked, this, [this] { if (auto *d = excelDoc()) d->setZoom(d->zoom() + 10); });
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) { if (auto *d = excelDoc()) d->setZoom(v); });
}

void ExcelWorkspace::syncStatus()
{
    auto *d = excelDoc();
    if (!d) return;
    m_modeLabel->setText(d->isViewMode() ? tr("View mode") : tr("Ready"));
    {
        QSignalBlocker b(m_zoomSlider);
        m_zoomSlider->setValue(d->zoom());
    }
    m_zoomLabel->setText(QString::number(d->zoom()) + QLatin1Char('%'));
    const auto ss = d->selectionStats();
    if (ss.numeric > 0)
        m_statLabel->setText(tr("Average: %1     Count: %2     Sum: %3").arg(QString::number(ss.sum / ss.numeric, 'g', 8)).arg(ss.numeric).arg(QString::number(ss.sum, 'g', 10)));
    else if (ss.cells > 0) m_statLabel->setText(tr("Count: %1").arg(ss.cells));
    else m_statLabel->clear();
    m_selLabel->clear();
    const auto st = d->state();
    m_selLabel->setText(st.sheetName);
}

void ExcelWorkspace::syncState()
{
    auto *d = excelDoc();
    if (!d) return;
    const auto s = d->state();
    auto set = [](QAction *a, bool on) { QSignalBlocker b(a); a->setChecked(on); };
    set(m_bold, s.style.bold); set(m_italic, s.style.italic); set(m_underline, s.style.underline);
    set(m_alignL, s.style.align == 1); set(m_alignC, s.style.align == 2); set(m_alignR, s.style.align == 3);
    set(m_grid, d->gridlines()); set(m_readMode, d->isViewMode());
    {
        QSignalBlocker b(m_numFmt);
        m_numFmt->setCurrentIndex(qBound(0, s.style.numFmt, 5));
    }
    const bool editable = !d->isViewMode();
    m_undo->setEnabled(editable && s.canUndo);
    m_redo->setEnabled(editable && s.canRedo);
    m_cut->setEnabled(editable);
    m_numFmt->setEnabled(editable);
}

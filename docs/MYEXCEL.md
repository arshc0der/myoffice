# MyExcel internals (`myexcel/`)

```
myexcel/
├── formula/       formula.{h,cpp}                  – tokenizer, parser, evaluator (no Qt Widgets)
├── spreadsheet/   sheet  sheetmodel  exceldocument – data, model, UI
├── importexport/  csvio  xlsxio
└── excelworkspace.*                                – ribbon + status bar
```

## Data model — `Sheet`

A **sparse** worksheet: `QHash<quint64, Cell>` where the key is `(row << 14) | column` (`Sheet::key/keyRow/keyCol`). Limits mirror Excel: 1,048,576 rows × 16,384 columns. Empty cells with default style are never stored, so memory is proportional to the used area.

```cpp
struct CellStyle { bool bold, italic, underline; QColor fg, bg; int align /*0 auto,1 L,2 C,3 R*/; int numFmt /*0 General,1 Number,2 Percent,3 Currency,4 Date,5 Text*/; };
struct Cell      { QString input; CellStyle style; };        // input is the raw text: "42", "hello", "=SUM(A1:A3)"
```

| Method | Description |
|---|---|
| `find(r,c)` / `cell(r,c)` / `setCell(r,c,cell)` | Pointer lookup / copy / insert-or-erase |
| `value(r,c)` | Evaluated `CellValue`, **cached**; formula cells are evaluated on demand |
| `display(r,c)` | Value formatted by the cell's number format |
| `usedExtent()` | Columns × rows that contain data |
| `rangeValues(...)` | `CellProvider` callback used by functions with ranges. Small ranges walk coordinates; huge ranges walk only stored cells (sorted row-major) |
| `replaceAll(cells)` | Replace storage (used by undo, sort, insert/delete) |

**Recalculation:** every `setCell/replaceAll` clears the value cache; cells are re-evaluated lazily when painted. Cycles are detected with an "evaluating" set and show `#CIRC!`; recursion depth is capped at 2,500. A true dependency graph is on the roadmap.

`parseLiteral(text)` turns typed text into a value (number, `TRUE/FALSE`, `12%` → 0.12, ISO date `2026-11-08` → serial number, else text). `autoFormat(cell)` switches a *General* cell to Percent or Date when the text looks like one.

## Formula engine (`formula/formula.*`)

Pipeline: **tokenize → recursive-descent parse (AST) → evaluate** with a `CellProvider` callback.

Grammar (lowest to highest precedence): comparison (`= <> < > <= >=`) → concatenation `&` → `+ -` → `* /` → `^` → unary `- +` → postfix `%` → primary (number, string, `TRUE/FALSE`, cell, range `A1:B9`, function call, parentheses). Parser depth is limited to 200.

Functions (see `callFunction()` in `formula.cpp`):

| Category | Functions |
|---|---|
| Math | `SUM AVERAGE MIN MAX ROUND ABS SQRT POWER MOD INT` |
| Statistical | `COUNT COUNTA` |
| Logical | `IF` (lazy) `AND OR NOT` |
| Text | `CONCAT CONCATENATE LEFT RIGHT LEN UPPER LOWER TRIM` |
| Date/time | `TODAY NOW` |

Errors: `#DIV/0!`, `#VALUE!`, `#NAME?`, `#REF!`, `#NUM!`, `#CIRC!`. Errors propagate through operators and functions.

Helpers used elsewhere: `Formula::parseRef("B12", &row, &col)`, `colName(i)`, `refName(r,c)`, `shiftReferences(formula, isRow, at, delta)` (insert/delete rows/columns; deleted refs become `#REF!`), `translateReferences(formula, dRow, dCol)` (copy/paste and fill; `$` absolute markers are respected).

## `SheetModel` (`QAbstractTableModel`)

Reports 1,048,576 × 16,384 cells but only produces data for visible ones. Roles: display, edit (raw input), alignment, font, foreground, background, tooltip (error text).

**All mutations go through the undo stack:**

* `pushEdits(QVector<Edit>, text)` → `CellsCommand` (stores before/after per cell).
* `changeSnapshot(text, mutate)` → `SnapshotCommand` (stores the whole cell map before/after) for sort, insert/delete rows/columns.
* `setData()` (typing in a cell) builds an `Edit` and applies `autoFormat`.
* After any change the model emits `dataChanged` for the whole range; views repaint only what is visible.

## `ExcelDocument`

Layout: name box + formula bar, `QStackedWidget` with one `QTableView` per sheet, `QTabBar` (sheet tabs) with a "+" button.

| Area | Methods |
|---|---|
| Clipboard | `copy`, `cut`, `paste` (TSV with RFC-4180 quoting; formulas copied inside MyExcel get relative references shifted), `clearSelection` |
| Formatting | `styleSelection(std::function<void(CellStyle&)>, undoText)` |
| Editing | `fill(down)`, `autoSum()`, `sortSelection(asc)`, `insertDelete(rows, insert)`, `findReplace()`, `focusFormulaBar(prefill)` |
| Sheets | `addSheet`, `deleteSheet`, `renameSheet` (sheet operations clear the undo stack) |
| View | `setZoom(40–300)`, `setGridlines(bool)` |
| Files | `load` (CSV/XLSX), `save`, `saveAs`, `exportTo`, `exportDialog` (PDF/CSV/XLSX) |

Safety clamp: a selection that spans huge ranges (e.g. a whole column) is limited to the used area (`clamp()`) before styling/clearing so an operation can never touch millions of cells.

## Import / export

* `CsvIO::read/write` — RFC 4180 parser; delimiter auto-detected (`,` `;` tab); UTF-8 (BOM written on export so Excel detects the encoding).
* `XlsxIO::read/write` — multiple sheets, values, formulas with cached results, fonts, solid fills, alignment, number formats; see [FILE_FORMATS.md](FILE_FORMATS.md#xlsx).
* PDF — prints the used range of the active sheet in landscape A4, splitting wide sheets into column bands.

## Charts and analytics

### `ChartView` / `ChartPanel` (`spreadsheet/chartpanel.*`)
A dependency-free chart widget (no Qt Charts module). Types: `Column`, `Line`, `Area`, `Pie`, `Donut`. Data is a plain `ChartData { title, categories, series[] }`.

* Painted with `QPainter`: grid with "nice" axis ticks, legend, Office default palette (`#4472C4 #ED7D31 #A5A5A5 #FFC000 #5B9BD5 #70AD47 …`), animated reveal (`reveal` property, honours reduced motion), hover highlight + tooltip.
* `ChartPanel` adds the type buttons (`IconButton::setChecked` marks the active type), *Export PNG* and *Close*.

### How a chart is built from cells (`ExcelDocument::buildChart`)
1. A single selected cell expands to the contiguous data block around it (`dataRegionAround`).
2. If the first row has text and the second row has numbers, it is used as the **header** (series names).
3. If at least half of the first column is text it becomes the **category** column; otherwise categories are 1…n.
4. Every remaining column is a series; non-numeric cells count as 0. Limits: 500 rows × 12 series.

The chart is **live**: any `dataChanged` restarts a 180 ms timer that rebuilds the chart from the remembered range (`m_chartRect`) without replaying the animation. Charts are not saved in XLSX yet.

### Statistics
* `ExcelDocument::selectionStats()` → `SelStats { cells, numeric, sum, min, max }`, shown in the status bar as *Average / Count / Sum*. Huge selections walk only the stored cells.
* `ExcelDocument::showStatistics()` (Data → Statistics): count, sum, mean, median, min, max, range, sample standard deviation, variance and a histogram (√n bins, 5–12) drawn by `ChartView`.

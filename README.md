# MyOffice 0.3.0

C++17 · Qt 6.5+ Widgets · qmake · Windows & Linux.
Apps: **MyWord** (blue), **MyExcel** (green) — **MyPowerPoint** (orange) is NOT implemented yet (card opens a labelled placeholder).

## Build
Requirements: Qt 6.5+ with modules Core, Gui, Widgets, Concurrent, Svg **and Qt private headers**
(Debian/Ubuntu: `qt6-base-private-dev`, `qt6-svg-dev`).
```
qmake MyOffice.pro CONFIG+=release
make -j            # nmake/jom on Windows MSVC
./MyOffice [file ...]
```
Qt Creator: open `MyOffice.pro`, choose a Qt 6.5+ kit, Run.
**This code has not been compiled by its author yet** – please send compiler output for fixes.

## What works
- Launcher: 3 cards, Open file…, recent files, "open files in view mode first" switch, drag-and-drop of files onto the window.
- Open ANY supported file (docx, html, md, txt/log/json/xml…, csv, xlsx; unknown text files open in MyWord) in **View mode**; "Enable editing" banner switches to edit mode. Command-line files too.
- Shared UI: per-app colours (title bar, tabs, ribbon accent, status bar), light/dark/system theme (Ctrl+Shift+D), SVG icons everywhere (resources/icons), ribbon with tabs, document tabs (close, reorder, unsaved dot, +), toasts, unsaved-changes prompts, autosave + crash recovery (MyWord).
- **MyWord**: ribbon File/Home/Insert/Layout/Review/View; fonts, size, B/I/U/S, colours, highlight, lists, indent, alignment, line & paragraph spacing, styles, tables (+row/col ops, merge), pictures, links, page break, find & replace bar, word count, zoom, margins/orientation/page size; open/save DOCX, HTML (images embedded), Markdown, TXT; export PDF, ODT, DOCX, HTML, MD, TXT.
- **MyExcel**: 1,048,576 × 16,384 sparse grid, formula bar, name box, sheet tabs, formulas (SUM AVERAGE MIN MAX COUNT COUNTA IF AND OR NOT ROUND ABS SQRT POWER MOD INT CONCAT LEFT RIGHT LEN UPPER LOWER TRIM TODAY NOW, ranges, auto-recalc, cycle detection), undo/redo, copy/cut/paste (formula refs shift), fill down/right, AutoSum, sort, insert/delete rows & columns (formulas adjusted), cell formatting (bold/italic/underline, colours, alignment, number/percent/currency/date/text), find & replace, zoom, gridlines; open/save XLSX and CSV; export PDF.

## Not implemented yet (honest list)
MyPowerPoint (everything) · MyWord: headers/footers, rulers, navigation pane, images/headers in DOCX, RTF, spell-check, true pagination on screen (PDF export paginates) · MyExcel: charts, conditional formatting, filters, freeze panes, merge cells, drag-fill handle, column widths in XLSX, cross-sheet formulas (Sheet2!A1), autosave recovery, notes on dependency graph (cache is cleared on every edit instead) · unit tests.

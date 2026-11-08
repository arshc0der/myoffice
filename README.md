# MyOffice 0.4.0

A modern desktop productivity suite — **MyWord**, **MyExcel**, **MyPowerPoint** — in C++17 and Qt 6 Widgets (qmake), Windows & Linux.

Repository: https://github.com/arshc0der/myoffice

## Build
Requirements: Qt 6.5+ with Core, Gui, Widgets, Concurrent, Svg **and Qt private headers**
(Debian/Ubuntu: `qt6-base-private-dev`, `qt6-svg-dev`).
```
qmake MyOffice.pro CONFIG+=release
make -j            # nmake / jom on Windows MSVC
./MyOffice [file ...]
```
Qt Creator: open `MyOffice.pro`, pick a Qt 6.5+ kit, Run.

## Highlights
- Launcher with 3 colour-coded apps, recent files, drag-and-drop, "open files in view mode first".
- Open **any** supported file in view mode (read-only banner → *Enable editing*). Command-line files too.
- Microsoft-style ribbon, browser-style document tabs, light/dark/system theme, ~125 SVG icons, toasts, About / Settings / Keyboard-shortcut dialogs (F1).
- **MyWord**: rich text, ruler, tables, pictures, links, find & replace, page setup; DOCX / HTML / Markdown / TXT, export PDF / ODT.
- **MyExcel**: formulas (SUM, AVERAGE, IF, …), undo/redo, fill, sort, insert/delete rows & columns, formats; XLSX / CSV, export PDF.
- **MyPowerPoint**: slide thumbnails (drag to reorder), canvas with move / resize, text boxes, shapes, lines, pictures, speaker notes,
  backgrounds & slide themes, 8 slide transitions, 9 object animations + animation pane, full-screen slide show (F5 / Shift+F5);
  open/save PPTX, export PDF / PNG.

## Known limits
- MyWord: no headers/footers, spell-check, RTF; on-screen page breaks are approximate (PDF export paginates exactly).
- MyExcel: no charts, filters, conditional formatting, merged cells or cross-sheet formulas.
- MyPowerPoint: no tables, charts, grouping, presenter view or video; PPTX tables/charts/SmartArt are skipped on import;
  PowerPoint does not play MyPowerPoint's animations (they are stored in a private part and restored by MyPowerPoint).
- The code is developed without a compiler in the loop — please report build errors as GitHub issues.

## Layout
`core/` services · `ui/` shared widgets, ribbon, workspace, dialogs · `myword/` · `myexcel/` · `myppt/` · `resources/icons/` SVG icons.

<div align="center">

# MyOffice

**A modern, native desktop productivity suite — documents, spreadsheets and presentations in one fast C++ / Qt application.**

[Repository](https://github.com/arshc0der/myoffice) · [Documentation](docs/README.md) · [Contributing](CONTRIBUTING.md) · [Report a bug](https://github.com/arshc0der/myoffice/issues)

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue) ![Qt](https://img.shields.io/badge/Qt-6.5%2B-41cd52) ![Build](https://img.shields.io/badge/build-qmake-orange) ![Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)

</div>

---

## Overview

MyOffice is a three-in-one office suite built with **C++17** and **Qt 6 Widgets**. It does not use Electron, web views or QML: every control in the main interface (title bar, ribbon, tabs, status bar, dialogs) is a custom-painted Qt widget, and all icons are SVG.

| App | Colour | Purpose | Native formats |
|---|---|---|---|
| **MyWord** | blue | Word processing | DOCX, HTML, Markdown, TXT · export PDF, ODT |
| **MyExcel** | green | Spreadsheets with a formula engine | XLSX, CSV · export PDF |
| **MyPowerPoint** | orange | Presentations with animations and slide show | PPTX · export PDF, PNG |

### Highlights

- **Microsoft-style ribbon** with File / Home / Insert / … tabs, grouped commands, split buttons and colour menus.
- **Browser-style document tabs** — open many documents per app; reorder by drag, close with the tab button or middle click, unsaved-changes dot.
- **A different colour theme per app**, plus light, dark and follow-system modes.
- **Open any file in View mode.** Files open read-only with an *Enable editing* banner. Works from the launcher, recent list, drag-and-drop and the command line.
- **Safe by default** — atomic saves, unsaved-change prompts, crash-recovery autosave (MyWord), input validation (size limit, ZIP signature checks).
- **126 SVG icons**, tinted at runtime to match the theme; `About`, `Settings` and keyboard-shortcut dialogs.
- **Small, readable codebase** (~11k lines) organised as shared services + one module per app.

## Quick start

```bash
git clone https://github.com/arshc0der/myoffice.git
cd myoffice
qmake MyOffice.pro CONFIG+=release
make -j$(nproc)            # nmake / jom on Windows (MSVC)
./MyOffice [file ...]      # optional: open files from the command line
```

Requirements: **Qt 6.5+** with the modules *Core, Gui, Widgets, Concurrent, Svg* **and the Qt private headers**
(Debian/Ubuntu: `sudo apt install qt6-base-dev qt6-base-private-dev qt6-svg-dev`).
Full instructions per platform: [docs/BUILDING.md](docs/BUILDING.md).

## Using the application

1. **Launcher** — pick *MyWord*, *MyExcel* or *MyPowerPoint* to start a new document, use **Open file…**, or click a recent file.
2. **View mode first** — files open read-only. Press **Enable editing** in the banner (or `Save`) to edit. The switch on the launcher (and *Settings*) turns this behaviour off.
3. **Tabs** — `+` opens a new document in the current app, `Ctrl+W` closes a tab, `Ctrl+O` opens files (they are routed to the right app automatically).
4. **Themes** — `Ctrl+Shift+D` toggles light/dark; *Settings* offers *Follow system*.
5. **Help** — `F1` opens *About*; *Settings → Keyboard shortcuts* lists every shortcut.

## Feature matrix

<details>
<summary><b>MyWord</b></summary>

Rich text (font, size, bold/italic/underline/strike, colour, highlight) · paragraph alignment, lists, indentation, line and paragraph spacing · styles (Normal, Title, Heading 1–3, Quote) · tables with row/column insert/delete and cell merge · pictures · hyperlinks · page breaks · find & replace bar with live highlighting · page setup (size, orientation, margins) · ruler · zoom (25 %–400 %) · word count · autosave with crash recovery · multi-tab.
</details>

<details>
<summary><b>MyExcel</b></summary>

1,048,576 × 16,384 sparse grid · formula bar and name box · multiple sheets · formulas (`SUM AVERAGE MIN MAX COUNT COUNTA IF AND OR NOT ROUND ABS SQRT POWER MOD INT CONCAT CONCATENATE LEFT RIGHT LEN UPPER LOWER TRIM TODAY NOW`) with ranges, operators, automatic recalculation and cycle detection · undo/redo · copy/cut/paste with relative-reference shifting · fill down/right · AutoSum · sort · insert/delete rows and columns (formulas are rewritten) · formats (bold/italic/underline, colours, alignment, number, percent, currency, date, text) · find & replace · zoom · gridline toggle.
</details>

<details>
<summary><b>MyPowerPoint</b></summary>

Slide thumbnails (drag to reorder) · canvas with select / move / resize handles / nudge with arrow keys · text boxes, rectangles, rounded rectangles, ellipses, lines, pictures · font, colour, alignment · fill, outline, line weight · arrange (front/back) and align to slide · speaker notes · slide themes and backgrounds · 7 slide transitions · 9 object animations with *On click / With previous / After previous* triggers and an animation pane · full-screen slide show (`F5`, `Shift+F5`) · undo/redo · PPTX import/export · PDF and PNG export.
</details>

## Documentation

| Document | What is inside |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the application is structured and how the pieces talk to each other |
| [docs/BUILDING.md](docs/BUILDING.md) | Build on Windows, Linux and Qt Creator; dependencies |
| [docs/CORE.md](docs/CORE.md) | Core services: navigation, files, autosave, theme, icons, async I/O |
| [docs/UI_FRAMEWORK.md](docs/UI_FRAMEWORK.md) | Reusable widgets: ribbon, tabs, buttons, toasts, dialogs |
| [docs/MYWORD.md](docs/MYWORD.md) · [MYEXCEL.md](docs/MYEXCEL.md) · [MYPOWERPOINT.md](docs/MYPOWERPOINT.md) | Per-app internals and class reference |
| [docs/FILE_FORMATS.md](docs/FILE_FORMATS.md) | What is read/written for DOCX, XLSX, PPTX, CSV, HTML, Markdown |
| [docs/ADDING_FEATURES.md](docs/ADDING_FEATURES.md) | **Cookbook**: add a ribbon command, icon, formula, format, transition, setting, or a whole new app |
| [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Icons missing, build errors, runtime issues |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Known limitations and planned work |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Workflow, coding standards, pull-request checklist |

## Project layout

```
myoffice/
├── MyOffice.pro            # root qmake project (includes the .pri of each module)
├── main.cpp                # application entry point
├── core/                   # services without UI: navigation, files, autosave, theme, icons, async I/O
├── ui/                     # shared UI framework
│   ├── titlebar/  widgets/  ribbon/  workspace/  launcher/  dialogs/
│   └── mainwindow.*        # frameless window, resize hit-zones, routing of files to apps
├── myword/                 # MyWord:  document/  editor/  importexport/
├── myexcel/                # MyExcel: formula/  spreadsheet/  importexport/
├── myppt/                  # MyPowerPoint: document/  editor/  importexport/
├── resources/              # resources.qrc + icons/*.svg
└── docs/                   # documentation
```

## Status and limitations

MyOffice is under active development. The most important current limitations are listed here; see [docs/ROADMAP.md](docs/ROADMAP.md) for details.

- MyWord: no headers/footers, spell-check or RTF; on-screen page breaks are approximate (PDF export paginates exactly).
- MyExcel: no charts, filters, conditional formatting, merged cells or cross-sheet formulas.
- MyPowerPoint: no tables, charts, grouping, presenter view or video; unsupported PPTX elements are skipped on import.
- There is no automated test suite or CI yet — good first contributions (see [CONTRIBUTING.md](CONTRIBUTING.md)).

## Contributing

Contributions are welcome — bug reports, documentation, new features and tests. Read [CONTRIBUTING.md](CONTRIBUTING.md) first; [docs/ADDING_FEATURES.md](docs/ADDING_FEATURES.md) shows how to extend each part of the code base.

## Author

Created by **Arsh** ([@arshc0der](https://github.com/arshc0der)).

## License

A license has not been chosen yet. Until a `LICENSE` file is added to the repository, all rights are reserved by the author. If you intend to use or redistribute the code, open an issue to ask — and maintainers: add a `LICENSE` file (MIT, Apache-2.0 or GPL-3.0 are the usual choices for Qt applications; note that Qt itself is LGPL/GPL/commercial).
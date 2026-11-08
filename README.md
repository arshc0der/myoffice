# MyOffice — Milestone 1 (application shell)

C++17 · Qt 6.5+ Widgets · qmake. Windows and Linux.

## Build
```
qmake MyOffice.pro CONFIG+=release   # use the qmake from your Qt 6 kit
make -j                              # nmake / jom on Windows MSVC
./MyOffice
```
Qt Creator: open `MyOffice.pro`, pick a Qt 6.5+ kit, Run. Modules needed: Core, Gui, Widgets, Concurrent.

## Shortcuts
Esc = launcher · Ctrl+Shift+D = toggle theme. Reduced motion: set env `MYOFFICE_REDUCE_MOTION=1` (Windows also follows the OS setting).

## Layout
- `core/` design tokens, ThemeManager, ApplicationManager, FileManager, AutosaveManager
- `ui/titlebar`, `ui/widgets`, `ui/launcher`, `ui/mainwindow.*`
- `core/core.pri` and `ui/ui.pri` are shared by the root project; app modules become static-lib subprojects in their phases.

## Not implemented yet
MyWord, MyExcel, MyPowerPoint workspaces (cards open a clearly labelled placeholder), unit tests (`tests/`), wiring of FileManager recents and AutosaveManager into documents.

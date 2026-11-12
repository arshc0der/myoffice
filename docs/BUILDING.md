# Building MyOffice

## Requirements

| Item | Version / note |
|---|---|
| C++ compiler | C++17: GCC ≥ 9, Clang ≥ 10 or MSVC 2019+ |
| Qt | **6.5 or newer** (`QStyleHints::colorScheme()` is used for system dark mode) |
| Qt modules | `core gui widgets concurrent svg` **+ `gui-private`** |
| Build tool | `qmake` (shipped with Qt) and `make` / `nmake` / `jom` |

`gui-private` is required because DOCX/XLSX/PPTX are read and written with Qt's internal zip classes (`<private/qzipreader_p.h>`, `<private/qzipwriter_p.h>`). Private headers are **not** an ABI-stable API: always build against the same Qt version you link with.

## Linux (Debian / Ubuntu)

```bash
sudo apt update
sudo apt install build-essential qt6-base-dev qt6-base-private-dev qt6-svg-dev libgl1-mesa-dev
git clone https://github.com/arshc0der/myoffice.git && cd myoffice
qmake6 MyOffice.pro CONFIG+=release      # may be called "qmake" on some distributions
make -j"$(nproc)"
./MyOffice
```

Fedora: `sudo dnf install qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtsvg-devel`.
Arch: `sudo pacman -S qt6-base qt6-svg` (private headers are included in `qt6-base`).

## Windows (MSVC)

1. Install Qt 6.5+ with the *MSVC 2019/2022 64-bit* component and *Qt Creator* (Qt Online Installer). Private headers are installed with the kit.
2. Open **x64 Native Tools Command Prompt** and run

```bat
qmake MyOffice.pro CONFIG+=release
nmake            :: or jom
```

3. Deploy: `windeployqt --release MyOffice.exe` copies the Qt DLLs and plugins (including `svg` support libraries).

## Qt Creator

1. *File → Open File or Project…* → `MyOffice.pro`.
2. Choose a **Qt 6.5+** kit.
3. After adding or removing source files or icons, run *Build → Run qmake*, then build. Resource files (`.qrc`) are only regenerated when qmake/rcc runs.

## Project file overview

`MyOffice.pro` sets the Qt modules and includes one `.pri` per module:

```
core/core.pri   ui/ui.pri   myword/myword.pri   myexcel/myexcel.pri   myppt/myppt.pri
```

Resources: `RESOURCES += resources/resources.qrc`. The `.qrc` lists `icons/<name>.svg` under prefix `/`, so an icon is addressed as `:/icons/<name>.svg`.

## Build options

| Option | Effect |
|---|---|
| `CONFIG+=release` / `CONFIG+=debug` | Optimised / debug build |
| `MYOFFICE_REDUCE_MOTION=1` (runtime env var) | Force reduced motion |

## Verifying a build

1. The launcher appears with three coloured cards.
2. Open MyWord → the ribbon shows **icons** above the labels (if not, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md#icons-are-missing)).
3. Type text, `Ctrl+S`, save as `.docx`, reopen it from *Recent*.

## Common build errors

| Message | Fix |
|---|---|
| `fatal error: private/qzipreader_p.h: No such file` | Install the Qt private-headers package, and make sure `QT += gui-private` is present. |
| `Project ERROR: Unknown module(s) in QT: svg` | Install `qt6-svg-dev` (Linux) or add the *Qt SVG* component (Windows). |
| `MyOffice requires Qt 6.5 or newer` | Select a newer Qt kit. |
| `rcc: Cannot find file 'icons/….svg'` | Paths in `resources/resources.qrc` are relative to the `.qrc` file's folder. Keep the `.qrc` inside `resources/` next to `icons/`. |

# Cookbook: adding things to MyOffice

Each recipe lists the **files to touch** and a minimal working example. After adding source files or icons, add them to the matching `.pri` / `.qrc` and run *qmake* again.

Contents

1. [Add an icon](#add-an-icon)
2. [Add a ribbon command (MyWord example)](#add-a-ribbon-command)
3. [Add a ribbon tab or group](#add-a-ribbon-tab-or-group)
4. [Add a spreadsheet function](#add-a-spreadsheet-function)
5. [Add a cell number format (MyExcel)](#add-a-cell-number-format)
6. [Add a slide transition (MyPowerPoint)](#add-a-slide-transition)
7. [Add an object animation effect](#add-an-object-animation-effect)
8. [Add a new shape type](#add-a-new-shape-type)
9. [Support a new file extension](#support-a-new-file-extension)
10. [Add a setting](#add-a-setting)
11. [Add a theme colour](#add-a-theme-colour)
12. [Add a dialog](#add-a-dialog)
13. [Add a custom widget](#add-a-custom-widget)
14. [Add a whole new application](#add-a-whole-new-application)
15. [Write a unit test](#write-a-unit-test)

---

## Add an icon

Files: `resources/icons/<name>.svg`, `resources/resources.qrc`.

1. Create a 24 × 24 stroke icon. Use `currentColor` so the app can tint it:

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor"
     stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">
  <path d="M5 12h14M12 5v14"/>
</svg>
```

2. Register it in `resources/resources.qrc` (path relative to the `.qrc`):

```xml
<file>icons/my-icon.svg</file>
```

3. Re-run qmake, rebuild, and use the name: `makeAction(tr("Do it"), QStringLiteral("my-icon"))` or `new IconButton("my-icon", tr("Do it"))` or `Icons::pixmap("my-icon", color, 16, dpr)`.

Guidelines: simple geometry, no fills (except `fill="currentColor" fill-opacity=".25"` accents), no colours, no text, no external references. Draw your own artwork or use a permissively licensed set whose licence you can satisfy.

## Add a ribbon command

Example: a **Subscript** toggle in MyWord.

Files: `myword/document/worddocument.{h,cpp}` (behaviour), `myword/editor/wordworkspace.{h,cpp}` (button).

1. **Behaviour** in `WordDocument`:

```cpp
// worddocument.h
void setSubscript(bool on);

// worddocument.cpp
void WordDocument::setSubscript(bool on)
{
    QTextCharFormat cf;
    cf.setVerticalAlignment(on ? QTextCharFormat::AlignSubScript : QTextCharFormat::AlignNormal);
    m_edit->mergeCurrentCharFormat(cf);
}
```

2. **Button** in `WordWorkspace::buildHome()` inside the Font group:

```cpp
m_subscript = editAction(tr("Subscript"), QStringLiteral("subscript"), {}, /*checkable*/ true);
connect(m_subscript, &QAction::triggered, this, [this](bool on) { if (auto *d = wordDoc()) d->setSubscript(on); });
font->addRow({ small(m_subscript) });                 // or font->addSmall(m_subscript) in a column
```

3. **Reflect state** — add `bool subscript` to `WordDocument::FormatState`, fill it in `formatState()`, and in `WordWorkspace::syncState()`:

```cpp
set(m_subscript, s.subscript);
```

4. Add the `subscript.svg` icon ([recipe](#add-an-icon)).

**Rules:** use `editAction()` for anything that changes the document (it is disabled automatically in view mode); use `makeAction()` for read-only commands (copy, find, zoom). Always resolve the document inside the lambda (`wordDoc()`).

## Add a ribbon tab or group

```cpp
void WordWorkspace::buildReferences()
{
    RibbonPage *page = m_ribbon->addPage(tr("References"));
    RibbonGroup *toc = page->addGroup(tr("Table of contents"));
    toc->addLarge(editAction(tr("Insert\nTOC"), QStringLiteral("list-number")));
}
// call it from the WordWorkspace constructor, between the other build…() calls
```

Tab order = order of `addPage()` calls. `File` should be first and `primary = true`.

## Add a spreadsheet function

Files: `myexcel/formula/formula.cpp` (+ docs list in `formula.h` and `docs/MYEXCEL.md`), `myexcel/excelworkspace.cpp` (menu entry).

Add a branch to `callFunction()`. Example `MEDIAN`:

```cpp
if (name == QLatin1String("MEDIAN")) {
    std::vector<V> vals;
    if (!gather(f, cp, vals, &err)) return err;          // expands ranges, propagates errors
    std::vector<double> nums;
    for (const V &v : vals) if (v.type == V::Number) nums.push_back(v.num);
    if (nums.empty()) return V::error(QStringLiteral("#NUM!"));
    std::sort(nums.begin(), nums.end());
    const size_t n = nums.size();
    return V::number(n % 2 ? nums[n / 2] : (nums[n / 2 - 1] + nums[n / 2]) / 2.0);
}
```

Helpers available inside `callFunction`: `numArg(i,&x,&err)`, `textArg(i,&s,&err)`, `gather(f, cp, vec, &err)` (all arguments, ranges expanded), `bad()` (`#VALUE!`). Functions that need lazy evaluation (like `IF`) evaluate arguments with `eval(*f.a[i], cp)` themselves.

Then list it in the Formulas tab (`buildFormulas()` — add the name to the right category list).

## Add a cell number format

Files: `sheet.cpp` (`formatValue`), `excelworkspace.cpp` (combo items), `xlsxio.cpp` (id mapping).

1. Use the next free `numFmt` code (6, 7, …) — keep existing values stable because they are persisted.
2. Format it in `formatValue()`: `case 6: return c.toString(v.num, 'e', 3);`
3. Add the item at the **same index** in the *Number* combo in `buildHome()`.
4. Map it in `XlsxIO::write()` (`fmtIds[]`) and `mapNumFmt()`.

## Add a slide transition

Files: `myppt/document/slidemodel.h`, `myppt/editor/slideshow.cpp`, `myppt/importexport/pptxio.cpp`.

1. **Append** the value to `Ppt::Transition` *and* its label to `transitionNames()` at the same position (the ribbon uses the combo index as the enum value).
2. Draw it in `SlideShow::paintTransition()`; `v` runs from 0 to 1, `m_from` is the outgoing pixmap and `m_to` the incoming one:

```cpp
case T::PushDown:
    p.drawPixmap(o + QPointF(0,  t.height() * v), m_from);
    p.drawPixmap(o + QPointF(0, -t.height() * (1 - v)), m_to);
    break;
```

3. Map it to/from the PPTX `<p:transition>` element in `pptxio.cpp` (search for `PushLeft`).

## Add an object animation effect

Files: `slidemodel.h` (enum + `effectNames()`), `slidepainter.cpp` (`paintObject`), optionally `pptxio.cpp` (extras store the numeric value, so no change is needed).

In `SlidePainter::paintObject(p, o, progress)` add a `case` that transforms the painter according to `progress` (0 = start, 1 = finished) before the object is drawn. `SlideAnimator` needs no change: it only produces progress values.

## Add a new shape type

1. Add the value to `SlideObject::Type` (**append**; the numeric value is not persisted but keep order tidy).
2. Give it defaults in `makeObject()`.
3. Draw it in `SlidePainter::paintObject()`.
4. Create the tool: add a row to the `shapes[]` table in `PptWorkspace::buildInsert()` (label, icon, type).
5. Map it in `pptxio.cpp` (`prstGeom` name) for read and write.

## Support a new file extension

*Same app, new format* (e.g. `.tsv` in MyExcel):

1. Add the extension to the app's `extensions` list in `ApplicationManager::apps()` — routing and dialogs pick it up.
2. Implement reading in the document's `parseBytes()` and writing in `writeFile()` (switch on the lower-case suffix).
3. If it is a ZIP-based format also add it to `isZipBased()` in `FileManager` so the signature check applies.
4. Add a *Save As* / export filter in the document's `saveAs()` / ribbon export list.
5. Document it in `docs/FILE_FORMATS.md`.

## Add a setting

1. Choose a key (`group/name`) and default; read with `QSettings().value("files/example", false).toBool()`.
2. Add a control in `AppDialogs::showSettings()` (`ToggleSwitch`, `QComboBox`, …) that writes the key when it changes.
3. If other objects must react immediately, add a signal to the owning manager (as `ThemeManager::themeChanged`).
4. Add the key to the table in `docs/ARCHITECTURE.md` §10.

## Add a theme colour

1. Add a `QColor` to `struct Palette` in `core/thememanager.h`.
2. Set it for light and dark in `ThemeManager::apply()`.
3. Use it in `paintEvent` via `ThemeManager::instance().palette().yourColor`.

## Add a dialog

Add a function to `ui/dialogs/appdialogs.{h,cpp}` following `showShortcuts()`: build a `QDialog` on the stack, use `PillButton` for buttons, `setTextColor()` for secondary labels, and finish with `dlg.exec()`. Call it from the title bar signal, a ribbon action or a shortcut.

## Add a custom widget

Follow the checklist in [UI_FRAMEWORK.md](UI_FRAMEWORK.md#style-guidelines-for-new-widgets). Put it in `ui/widgets/`, and add the `.h` (with `Q_OBJECT`) and `.cpp` to `ui/ui.pri`.

## Add a whole new application

Example: **MyNotes**.

1. **Identity** — `ApplicationManager::AppId` gets `Notes`; add an `AppInfo` entry (name, tagline, detail, accent colour, extensions, noun) to `apps()` **at the matching index** (the enum value indexes the list). Add a case to `paintAppGlyph()` (`ui/widgets/appglyph.cpp`).
2. **Module folder** — `mynotes/` with `mynotes.pri`; include it in `MyOffice.pro`.
3. **Document** — subclass `DocumentView`, implement the pure virtuals (`displayName`, `filePath`, `isModified`, `save`, `saveAs`, `isViewMode`, `setViewMode`, `load`), emit `titleChanged/modifiedChanged/stateChanged/statusChanged/loadFinished`.

```cpp
class NotesDocument : public DocumentView { Q_OBJECT /* … */ };
```

4. **Workspace** — subclass `Workspace`:

```cpp
class NotesWorkspace : public Workspace {
public:
    explicit NotesWorkspace(QWidget *p = nullptr) : Workspace(ApplicationManager::AppId::Notes, p) {
        RibbonPage *file = m_ribbon->addPage(tr("File"), true);
        RibbonGroup *g = file->addGroup(tr("Notes"));
        g->addLarge(m_actNew); g->addLarge(m_actOpen); g->addLarge(m_actSave); g->addLarge(m_actSaveAs);
        m_ribbon->addPage(tr("Home"));
        m_ribbon->setCurrentIndex(1);
    }
protected:
    DocumentView *createBlankDocument() override { return new NotesDocument; }
    void syncState() override {}  void syncStatus() override {}
};
```

5. **Wire into `MainWindow`** — add `NotesWorkspace *m_notes`, `ensureNotes()` (copy `ensureExcel()`), a branch in `onNavigationChanged()`, a branch in `openFiles()`, and include it in the `closeEvent` loop.
6. The launcher card, recent-file colouring, tab bar, banner, theming and routing work automatically.

## Write a unit test

There is no test project yet. The formula engine is a good first target because it has no UI dependencies. Suggested layout:

```
tests/
├── tests.pro
└── tst_formula.cpp
```

`tests/tests.pro`

```qmake
QT += testlib
CONFIG += c++17 console testcase
INCLUDEPATH += $$PWD/..
SOURCES += tst_formula.cpp ../myexcel/formula/formula.cpp
```

`tests/tst_formula.cpp`

```cpp
#include <QtTest>
#include "myexcel/formula/formula.h"

struct Grid : CellProvider {
    QHash<QPair<int,int>, double> v;
    CellValue cellValue(int r, int c) override { return v.contains({r,c}) ? CellValue::number(v[{r,c}]) : CellValue(); }
    void rangeValues(int r1, int c1, int r2, int c2, std::vector<CellValue> &out) override {
        for (int r = r1; r <= r2; ++r) for (int c = c1; c <= c2; ++c) if (v.contains({r,c})) out.push_back(CellValue::number(v[{r,c}]));
    }
};

class TstFormula : public QObject {
    Q_OBJECT
private slots:
    void sum() { Grid g; g.v[{0,0}] = 1; g.v[{1,0}] = 2; QCOMPARE(Formula::evaluate("SUM(A1:A2)", g).num, 3.0); }
    void divZero() { Grid g; QCOMPARE(Formula::evaluate("1/0", g).text, QString("#DIV/0!")); }
};
QTEST_APPLESS_MAIN(TstFormula)
#include "tst_formula.moc"
```

Run: `cd tests && qmake && make && ./tst_formula`. Good next tests: `Formula::shiftReferences`, `CsvIO::parse`, `DocxIO` round-trip, `PptxIO` round-trip, `FileManager::route`.
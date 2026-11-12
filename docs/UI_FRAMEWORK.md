# UI framework (`ui/`)

All widgets are painted with `QPainter` using the palette from `ThemeManager`. They repaint on `ThemeManager::themeChanged()` and animate with `QPropertyAnimation` using `ThemeManager::duration()`.

## Window shell

| Class | File | Role |
|---|---|---|
| `MainWindow` | `ui/mainwindow.{h,cpp}` | Frameless translucent window; shadow + resize zones; hosts the launcher and workspaces in a `FadeStack`; `openFiles(paths)` routes files; `checkRecovery()`; drag-and-drop; `closeEvent` asks every workspace to close its documents. |
| `TitleBar` | `ui/titlebar/` | Brand logo, back button, breadcrumb, About / Settings / theme / minimise / maximise / close buttons. Painted in `palette().chrome`. Drag = `startSystemMove()`, double-click = maximise. |
| `LauncherPage` | `ui/launcher/` | Greeting, three `AppCard`s, *Open file…*, view-mode switch, `RecentList`. Emits `newRequested(AppId)`, `openFileRequested()`, `openPathRequested(path)`. |
| `AppCard` | `ui/widgets/appcard.*` | Animated card per app (hover lift, press feedback, keyboard focusable). |

## Workspace layer

| Class | Description |
|---|---|
| `DocumentView` / `Workspace` | See [ARCHITECTURE.md §5](ARCHITECTURE.md#5-the-two-base-classes). |
| `ViewBanner` (private in `workspace.cpp`) | "VIEW MODE — Enable editing" bar. |
| `DocumentTabBar` | `addTab`, `removeTab`, `setTabTitle`, `setTabModified`, `setCurrentIndex`; signals `currentChanged`, `closeRequested`, `newRequested`, `tabMoved`. Drag to reorder, middle click closes, a dot replaces the close button when a tab has unsaved changes. |
| `OfficeStatusBar` | `QLabel *addLeftText()` for text segments; `addRight(QWidget*)` for controls such as the zoom slider. Painted in the app colour. |
| `FadeStack` | `QStackedWidget` with `fadeTo(QWidget*)` cross-fade (instant with reduced motion). The opacity effect is removed after the animation, because effects force off-screen rendering. |

## Ribbon (`ui/ribbon/`)

```
Ribbon
 ├─ RibbonTabStrip           (File | Home | Insert | …)  – keyboard: ← →
 └─ FadeStack of RibbonPage
      └─ RibbonGroup (titled)   ← addLarge / addSmall / addRow / addWidget
           └─ RibbonButton      ← bound to a QAction
```

### `Ribbon`
```cpp
RibbonPage *addPage(const QString &title, bool primary = false);  // primary = filled "File" style tab
void setCurrentIndex(int);   void setCornerWidget(QWidget *);
```

### `RibbonGroup`
```cpp
RibbonButton *addLarge(QAction *, QMenu *menu = nullptr);        // big icon + label, laid out horizontally
void beginColumn();                                             // start a vertical stack of small items
RibbonButton *addSmall(QAction *, bool showText = true, QMenu *menu = nullptr);
void addRow(const QList<QWidget *> &widgets);                   // horizontal row inside the current column
void addWidget(QWidget *);                                      // e.g. a QComboBox
```
Layout pattern for a typical group: `beginColumn(); addRow({fontCombo, sizeCombo}); addRow({boldBtn, italicBtn, …});`.

### `RibbonButton`
```cpp
enum class Size { Large, Small, SmallIcon };
RibbonButton(QAction *action, Size size, QWidget *parent = nullptr);
void setMenu(QMenu *);  void setSplit(bool);  void setIndicatorColor(const QColor &);
```
* The **icon name is read from the action's `iconName` property** (set by `Workspace::makeAction`). The text is the action's text; a `\n` creates a two-line label on large buttons.
* Checkable actions are drawn highlighted when checked; disabled actions are dimmed.
* *Split* buttons trigger the action when the main part is clicked and open the menu from the arrow part (like "font colour").
* Buttons use `Qt::NoFocus` so keyboard focus stays in the document.

### `ColorMenu` / `SwatchGrid`
A drop-down palette (10 × 4 swatches + "no colour" + "More colors…"). Signals: `colorPicked(QColor)`, `noColorPicked()`.

## Small widgets (`ui/widgets/`)

| Widget | Constructor | Notes |
|---|---|---|
| `IconButton` | `(iconName, accessibleName)` | 40×30 icon button; `setOnChrome(true)` for use on the coloured title/status bars; emits `clicked()`. |
| `PillButton` | `(text, iconName, primary)` | Rounded button; primary uses the accent colour. |
| `ToggleSwitch` | `(accessibleName)` | `setChecked(bool, animate = true)`, signal `toggled(bool)`. |
| `Toast` | `Toast::show(host, text, kind, ms = 3200)` | `kind` ∈ `Info`, `Success`, `Error`. Fades in/out at the bottom of the top-level window. |
| `RecentList` | — | Shows `FileManager::recentFiles()`; signal `openRequested(path)`. |
| `themeutil.h` | `setTextColor(QWidget*, QColor)` | Sets a label's text colour through `QPalette` (no style sheets). |
| `appglyph.h` | `paintAppGlyph(painter, rect, AppId)` | The painted app logos used on cards. |

All custom widgets set an accessible name and support keyboard activation (Space/Enter).

## Dialogs (`ui/dialogs/appdialogs.*`)

```cpp
AppDialogs::showAbout(parent);       // version, Qt version, feature list, GitHub link and buttons
AppDialogs::showSettings(parent);    // theme, reduce motion, open files in view mode, clear recent, shortcuts
AppDialogs::showShortcuts(parent);
inline const QString AppDialogs::kRepoUrl = "https://github.com/arshc0der/myoffice";
```
They are plain functions (no `Q_OBJECT`); change the repository URL in one place: `kRepoUrl`.

## Style guidelines for new widgets

1. Derive from `QWidget`, paint in `paintEvent`, take colours **only** from `ThemeManager::palette()`.
2. `connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));`
3. Hover/press feedback: a `qreal hover` property animated with `QPropertyAnimation`, duration `ThemeManager::instance().duration(Design::Duration::fast)`.
4. Set `setAccessibleName()`, a focus policy, and handle Space/Enter if the widget is clickable.
5. Use `Icons::pixmap(name, color, size, devicePixelRatioF())` for icons.
6. Use `Design::` tokens instead of literal numbers.

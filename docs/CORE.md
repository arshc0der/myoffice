# Core services (`core/`)

Everything here is independent of any particular application. All singletons are created lazily by `instance()` and live on the UI thread.

## ApplicationManager

`core/applicationmanager.{h,cpp}` — registry of apps and the navigation state.

```cpp
enum class AppId { Word, Excel, PowerPoint };
struct AppInfo { AppId id; QString name, tagline, detail; QColor accent; QStringList extensions; QString noun; };

static const QList<AppInfo> &apps();           // all apps, in launcher order
static const AppInfo &info(AppId id);
static bool isImplemented(AppId id);

bool atHome() const;   AppId currentApp() const;
void openApp(AppId id);   void goHome();       // emit navigationChanged()
```

`extensions` is the single source of truth for "which app opens which file type". Add an extension here and `FileManager::route()` and the open-file dialog filter pick it up automatically.

## FileManager

`core/filemanager.{h,cpp}` — recent files and file routing.

| Member | Description |
|---|---|
| `recentFiles()`, `addRecent(path)`, `removeRecent(path)`, `clearRecent()` | Up to `kMaxRecent = 20` absolute paths in `QSettings("files/recent")`; emits `recentFilesChanged()`. |
| `static Route route(path)` | `Route { bool ok; AppId app; QString error; }`. Validates existence, readability, size ≤ `kMaxFileBytes` (2 GiB), maps extension → app, checks the ZIP signature for `docx/xlsx/pptx`, and treats unknown extensions as MyWord text if `looksLikeText()`. Rejects `rtf doc xls ppt odt pdf` with a clear message. |
| `static QString allSupportedFilter()` | File-dialog filter built from all `AppInfo::extensions`. |
| `static bool looksLikeText(path)` | `true` if the first 8 KB contain no NUL byte. |

## AutosaveManager

`core/autosavemanager.{h,cpp}` — crash-recovery snapshots.

```cpp
using SnapshotProvider = std::function<QByteArray()>;
using DirtyCheck       = std::function<bool()>;

void registerDocument(const QString &id, SnapshotProvider, DirtyCheck);
void unregisterDocument(const QString &id, bool discardRecovery);
void setIntervalSeconds(int seconds);            // default 60, minimum 5
QString recoveryDir() const;                     // <AppLocalData>/recovery
QStringList recoverableIds() const;
QByteArray readRecovery(const QString &id) const;
void discardRecovery(const QString &id);
signals: void autosaveFailed(const QString &id, const QString &error);
```

Every interval, for each registered document whose `DirtyCheck` returns true: the snapshot bytes are produced **on the UI thread** (documents are not thread-safe) and written **atomically on a worker thread** with `QSaveFile` to `<id>.autosave`. File names are sanitised to `[A-Za-z0-9_-]`.

Currently MyWord registers itself (snapshot = embedded-image HTML). On start-up `MainWindow::checkRecovery()` offers to recover every `word-*` snapshot. MyExcel and MyPowerPoint do not autosave yet (roadmap).

## ThemeManager

`core/thememanager.{h,cpp}` — see [ARCHITECTURE.md §7](ARCHITECTURE.md#7-theming).

```cpp
enum class Mode { System, Light, Dark };
const Palette &palette() const;      bool isDark() const;
Mode mode() const;   void setMode(Mode);   void toggleLightDark();
void setActiveApp(std::optional<ApplicationManager::AppId>);   bool inApp() const;
bool reducedMotion() const;   void setReducedMotion(bool);
int  duration(int ms) const;         // returns 0 when reduced motion is on – always use it for animations
QFont uiFont(qreal pointSize, QFont::Weight = QFont::Normal) const;
signals: void themeChanged();
```

**Rule:** every animation duration must go through `ThemeManager::duration()` so the reduce-motion setting is respected.

## Icons

`core/iconprovider.{h,cpp}`

```cpp
QPixmap Icons::pixmap(const QString &name, const QColor &color, int logicalSize, qreal dpr);
```

Loads `:/icons/<name>.svg` (falls back to `:/resources/icons/`), replaces the literal `currentColor` with `color`, renders with `QSvgRenderer` at `size × dpr` pixels and caches the result by `name|color|size@dpr`. Missing icons log `Missing icon resource: <name>`.

Icon design rules: 24 × 24 viewBox, `fill="none"`, `stroke="currentColor"`, `stroke-width="1.7"`, round caps and joins. See [ADDING_FEATURES.md](ADDING_FEATURES.md#add-an-icon).

## FileIO

`core/fileio.{h,cpp}`

```cpp
using ReadCallback = std::function<void(const QByteArray &data, const QString &error)>;
void FileIO::readAsync(const QString &path, QObject *context, ReadCallback callback);
```

Reads the file on a worker thread (`QtConcurrent::run`) and calls `callback` on the UI thread. If `context` is destroyed first, the callback is dropped (the `QFutureWatcher` is its child), so closing a tab while a file loads is safe.

## Design tokens

`core/designtokens.h` — `Design::Space`, `Design::Radius`, `Design::IconSize`, `Design::Duration`, `Design::Type` (point sizes), `Design::Window` (shadow margin, corner radius, title-bar height) and `Design::mix(a, b, t)` for colour blending. Prefer these over magic numbers.

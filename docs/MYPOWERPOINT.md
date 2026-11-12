# MyPowerPoint internals (`myppt/`)

```
myppt/
├── document/      slidemodel.h  ppdocument.{h,cpp}
├── editor/        slidecanvas  slidepainter  slideshow  pptworkspace
└── importexport/  pptxio
```

## Data model (`document/slidemodel.h`)

Slides use a fixed logical canvas of **1280 × 720** units (`Ppt::kSlideW/kSlideH`, 16:9). One unit = 9525 EMU, so PPTX coordinates convert exactly.

```cpp
struct Deck        { QList<Slide> slides; };
struct Slide       { QList<SlideObject> objects; QColor background; Ppt::Transition transition;
                     int transitionMs; int advanceAfterMs /*0 = on click*/; QString notes; };
struct SlideObject { enum Type { Text, Rect, RoundRect, Ellipse, Line, Image };
                     Type type; QRectF rect; qreal rotation; bool flipV;
                     QColor fill, line; qreal lineWidth;
                     QString text; qreal fontSize; bool bold, italic, underline; QColor textColor;
                     int align /*0 L,1 C,2 R*/, valign /*0 T,1 M,2 B*/;
                     QImage image; Animation anim; };
struct Animation   { Ppt::Effect effect; Ppt::Trigger trigger; int durationMs, delayMs; };
```
Objects are stored back-to-front: the **list order is the z-order**. Enumerations:

* `Ppt::Effect`: `None, Appear, Fade, FlyLeft, FlyRight, FlyTop, FlyBottom, Zoom, Wipe, Spin`
* `Ppt::Trigger`: `OnClick, WithPrevious, AfterPrevious`
* `Ppt::Transition`: `None, Fade, PushLeft, PushUp, Wipe, Cover, Zoom, Dissolve`

`effectNames()`, `triggerNames()`, `transitionNames()` return the translated labels **in enum order** — the ribbon combo boxes use the combo index as the enum value, so **never reorder an enum without reordering its names list**.

`makeObject(type)` returns a sensible default object for each type.

## Rendering — `SlidePainter` (`editor/slidepainter.*`)

The *single* renderer used by the editor, thumbnails, the slide show and the PDF/PNG exporters, so everything looks identical.

```cpp
void   paintObject(QPainter*, const SlideObject&, qreal progress = 1.0);
void   paintSlide (QPainter*, const Slide&, const QRectF &target, const QVector<qreal> *progress = nullptr);
QPixmap renderSlide(const Slide&, const QSize &pixelSize, qreal dpr = 1.0, const QVector<qreal> *progress = nullptr);
```
`progress` holds one value (0…1) per object: `0` hides the object, `1` shows it, anything between applies its animation effect (fade opacity, fly offset, zoom scale, wipe clip, spin rotation). `nullptr` means "fully revealed".

## Editing surface — `SlideCanvas` (`editor/slidecanvas.*`)

A custom widget that paints the current slide scaled to fit and implements direct manipulation: click to select, drag to move, 8 resize handles, rotation handle, double-click (or `Enter`/`F2`) to edit text in place, arrow keys nudge, `Delete` removes, `Esc` cancels the insert tool/selection. It does not own the data; it asks the document for the current slide through a `SlideProvider` callback.

Signals: `checkpointRequested(text)` (emitted **once per gesture before the first change** so the document can snapshot for undo), `modified()`, `selectionChanged()`.
API: `setSelected(i)`, `setReadOnly(bool)`, `setInsertTool(type | -1)`, `editText(i)`.

## `PptDocument` (`document/ppdocument.*`)

Owns the `Deck` and the layout: thumbnail list on the left (`QListWidget`, drag to reorder), canvas in the middle, notes editor below, optional animation pane on the right.

| Area | Methods |
|---|---|
| Slides | `newSlide`, `duplicateSlide`, `deleteSlide`, `moveSlide(±1)`, `setCurrent(i)` |
| Objects | `startInsert(type)`, `insertPicture`, `duplicateObject`, `deleteObject`, `arrange(op)`, `alignObject(how)` |
| Generic edits | `modifySelected(fn, text)`, `modifySlide(fn, text)`, `modifyAllSlides(fn, text)` – **the way to change properties** (they create the undo step and refresh the UI) |
| Clipboard/undo | `undo`, `redo`, `copy`, `cut`, `paste` (objects, internal) |
| Show | `startShow(fromBeginning)` |
| Panels | `setNotesVisible`, `setAnimationPaneVisible` |
| State | `state()` → `State` (selection copy, background, transition, slide index/count, can-undo/redo) |
| Files | `load`, `save`, `saveAs`, `exportTo`, `exportDialog("pdf" \| "png")` |

**Undo model:** `checkpoint(text, key)` pushes a `Snap { Deck, current, serial }` (max 100). Consecutive checkpoints with the same *key* (e.g. typing in notes) are coalesced. Document is dirty when `m_serial != m_savedSerial`.

## Slide show (`editor/slideshow.*`)

* **`SlideAnimator`** converts a slide's per-object animations into *click steps*: an `OnClick` object starts a new step; `WithPrevious` starts at the same time as the previous entry; `AfterPrevious` starts when it ends; each adds its own delay. `nextStep()` starts the next step, a 16 ms timer updates the per-object progress vector, and `finishAll()` jumps to the end.
* **`SlideShow`** is a frameless full-screen window (deleted on close). It renders the outgoing slide (fully revealed) and the incoming slide (initial state) into pixmaps and animates a `QVariantAnimation` for the chosen transition (`Fade, PushLeft, PushUp, Wipe, Cover, Zoom, Dissolve`). Transitions are skipped when reduced motion is on.
* Keys: `→ ↓ PgDn Space Enter N` next · `← ↑ PgUp Backspace P` previous · `Home/End` first/last · `Esc/Q` exit. Mouse: left = next, right = previous. The cursor hides after 2 s.
* `advanceAfterMs > 0` auto-advances once all animations of the slide have finished.

## `PptWorkspace`

Ribbon pages: *File, Home, Insert, Design, Transitions, Animations, Slide Show, View*. Object-related actions are created with `objectAction()`, which also adds them to `m_objActions`; `syncState()` enables them only when an object is selected and the document is editable.

## PPTX (`importexport/pptxio.*`)

`QByteArray PptxIO::write(const Deck&)` and `bool PptxIO::read(bytes, Deck&, QString *error, int *skipped)`. See [FILE_FORMATS.md](FILE_FORMATS.md#pptx).

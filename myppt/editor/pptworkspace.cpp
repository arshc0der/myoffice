#include "pptworkspace.h"

#include <QActionGroup>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>

#include "core/thememanager.h"
#include "myppt/document/ppdocument.h"
#include "ui/ribbon/colormenu.h"
#include "ui/ribbon/ribbon.h"
#include "ui/widgets/statusbar.h"
#include "ui/widgets/themeutil.h"

namespace {
QComboBox *makeCombo(const QStringList &items, int width)
{
    auto *c = new QComboBox;
    c->addItems(items);
    c->setFixedSize(width, 26);
    return c;
}
}

PptWorkspace::PptWorkspace(QWidget *parent) : Workspace(ApplicationManager::AppId::PowerPoint, parent)
{
    buildFile();
    buildHome();
    buildInsert();
    buildDesign();
    buildTransitions();
    buildAnimations();
    buildShow();
    buildView();
    buildStatusBar();
    m_ribbon->setCurrentIndex(1); // Home
}

PptDocument *PptWorkspace::pptDoc() const { return qobject_cast<PptDocument *>(currentDocument()); }
DocumentView *PptWorkspace::createBlankDocument() { return new PptDocument; }
QAction *PptWorkspace::editAction(const QString &t, const QString &i, const QKeySequence &sc, bool c) { return makeAction(t, i, sc, c, true); }

QAction *PptWorkspace::objectAction(const QString &t, const QString &i, const QKeySequence &sc, bool c)
{
    QAction *a = makeAction(t, i, sc, c, true);
    m_objActions << a;
    return a;
}

RibbonButton *PptWorkspace::iconButton(RibbonGroup *g, QAction *a, QMenu *menu)
{
    auto *b = new RibbonButton(a, RibbonButton::Size::SmallIcon, g);
    if (menu) { b->setMenu(menu); b->setSplit(true); b->setFixedSize(b->sizeHint()); }
    return b;
}

// ------------------------------------------------------------------ File
void PptWorkspace::buildFile()
{
    RibbonPage *page = m_ribbon->addPage(tr("File"), true);
    RibbonGroup *g = page->addGroup(tr("Presentation"));
    g->addLarge(m_actNew);
    g->addLarge(m_actOpen);
    g->addLarge(m_actSave);
    g->addLarge(m_actSaveAs);
    RibbonGroup *ex = page->addGroup(tr("Export"));
    const struct { const char *label, *suffix, *icon; } formats[] = { { "PDF (all slides)", "pdf", "export" }, { "PNG (current slide)", "png", "image-export" } };
    ex->beginColumn();
    for (const auto &f : formats) {
        QAction *a = makeAction(tr(f.label), QLatin1String(f.icon));
        const QString suffix = QLatin1String(f.suffix);
        connect(a, &QAction::triggered, this, [this, suffix] { if (auto *d = pptDoc()) d->exportDialog(suffix); });
        ex->addSmall(a);
    }
    page->addGroup(tr("Close"))->addLarge(m_actClose);
}

// ------------------------------------------------------------------ Home
void PptWorkspace::buildHome()
{
    RibbonPage *page = m_ribbon->addPage(tr("Home"));

    m_undo = makeAction(tr("Undo"), QStringLiteral("undo"), QKeySequence::Undo, false, true);
    m_redo = makeAction(tr("Redo"), QStringLiteral("redo"), QKeySequence::Redo, false, true);
    m_cut = objectAction(tr("Cut"), QStringLiteral("cut"), QKeySequence::Cut);
    m_copy = makeAction(tr("Copy"), QStringLiteral("copy"), QKeySequence::Copy);
    QAction *paste = editAction(tr("Paste"), QStringLiteral("paste"), QKeySequence::Paste);
    connect(m_undo, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->undo(); });
    connect(m_redo, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->redo(); });
    connect(m_cut, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->cut(); });
    connect(m_copy, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->copy(); });
    connect(paste, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->paste(); });
    RibbonGroup *clip = page->addGroup(tr("Clipboard"));
    clip->addLarge(paste);
    clip->beginColumn();
    clip->addSmall(m_cut);
    clip->addSmall(m_copy);
    RibbonGroup *undo = page->addGroup(tr("Undo"));
    undo->beginColumn();
    undo->addSmall(m_undo);
    undo->addSmall(m_redo);

    // Slides
    QAction *newSlide = editAction(tr("New\nSlide"), QStringLiteral("slide-new"), QKeySequence(QStringLiteral("Ctrl+M")));
    QAction *dupSlide = editAction(tr("Duplicate slide"), QStringLiteral("slide-duplicate"));
    QAction *delSlide = editAction(tr("Delete slide"), QStringLiteral("trash"));
    QAction *upSlide = editAction(tr("Move slide up"), QStringLiteral("forward"));
    QAction *downSlide = editAction(tr("Move slide down"), QStringLiteral("backward"));
    connect(newSlide, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->newSlide(); });
    connect(dupSlide, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->duplicateSlide(); });
    connect(delSlide, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->deleteSlide(); });
    connect(upSlide, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->moveSlide(-1); });
    connect(downSlide, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->moveSlide(1); });
    RibbonGroup *slides = page->addGroup(tr("Slides"));
    slides->addLarge(newSlide);
    slides->beginColumn();
    slides->addSmall(dupSlide);
    slides->addSmall(delSlide);
    slides->beginColumn();
    slides->addSmall(upSlide);
    slides->addSmall(downSlide);

    // Font
    m_size = new QComboBox;
    m_size->setEditable(true);
    m_size->setFixedSize(68, 26);
    for (const char *s : { "10", "12", "14", "16", "18", "20", "24", "28", "32", "36", "44", "54", "66", "80", "96" }) m_size->addItem(QLatin1String(s));
    connect(m_size, &QComboBox::textActivated, this, [this](const QString &s) {
        const double v = s.toDouble();
        if (v >= 1 && v <= 400) if (auto *d = pptDoc()) d->modifySelected([v](SlideObject &o) { o.fontSize = v; }, tr("Font size"));
    });
    m_bold = objectAction(tr("Bold"), QStringLiteral("bold"), QKeySequence::Bold, true);
    m_italic = objectAction(tr("Italic"), QStringLiteral("italic"), QKeySequence::Italic, true);
    m_underline = objectAction(tr("Underline"), QStringLiteral("underline"), QKeySequence::Underline, true);
    connect(m_bold, &QAction::triggered, this, [this](bool on) { if (auto *d = pptDoc()) d->modifySelected([on](SlideObject &o) { o.bold = on; }, tr("Bold")); });
    connect(m_italic, &QAction::triggered, this, [this](bool on) { if (auto *d = pptDoc()) d->modifySelected([on](SlideObject &o) { o.italic = on; }, tr("Italic")); });
    connect(m_underline, &QAction::triggered, this, [this](bool on) { if (auto *d = pptDoc()) d->modifySelected([on](SlideObject &o) { o.underline = on; }, tr("Underline")); });
    QAction *textColor = objectAction(tr("Font color"), QStringLiteral("text-color"));
    auto *textMenu = new ColorMenu(tr("Automatic"), this);
    connect(textColor, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) { const QColor c = m_textColor; d->modifySelected([c](SlideObject &o) { o.textColor = c; }, tr("Font color")); } });
    connect(textMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { m_textColor = c; m_textBtn->setIndicatorColor(c); if (auto *d = pptDoc()) d->modifySelected([c](SlideObject &o) { o.textColor = c; }, tr("Font color")); });
    connect(textMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = pptDoc()) d->modifySelected([](SlideObject &o) { o.textColor = QColor(); }, tr("Font color")); });
    RibbonGroup *font = page->addGroup(tr("Font"));
    m_textBtn = iconButton(font, textColor, textMenu);
    m_textBtn->setIndicatorColor(m_textColor);
    font->beginColumn();
    font->addRow({ m_size });
    font->addRow({ iconButton(font, m_bold), iconButton(font, m_italic), iconButton(font, m_underline), m_textBtn });

    // Paragraph
    m_alignL = objectAction(tr("Align left"), QStringLiteral("align-left"), {}, true);
    m_alignC = objectAction(tr("Center"), QStringLiteral("align-center"), {}, true);
    m_alignR = objectAction(tr("Align right"), QStringLiteral("align-right"), {}, true);
    auto *grp = new QActionGroup(this);
    for (QAction *a : { m_alignL, m_alignC, m_alignR }) grp->addAction(a);
    auto setAlign = [this](int v) { if (auto *d = pptDoc()) d->modifySelected([v](SlideObject &o) { o.align = v; }, tr("Alignment")); };
    connect(m_alignL, &QAction::triggered, this, [setAlign] { setAlign(0); });
    connect(m_alignC, &QAction::triggered, this, [setAlign] { setAlign(1); });
    connect(m_alignR, &QAction::triggered, this, [setAlign] { setAlign(2); });
    QAction *vTop = objectAction(tr("Align text to top"), QStringLiteral("valign-top"));
    QAction *vMid = objectAction(tr("Align text to middle"), QStringLiteral("valign-middle"));
    QAction *vBot = objectAction(tr("Align text to bottom"), QStringLiteral("valign-bottom"));
    auto setV = [this](int v) { if (auto *d = pptDoc()) d->modifySelected([v](SlideObject &o) { o.valign = v; }, tr("Text alignment")); };
    connect(vTop, &QAction::triggered, this, [setV] { setV(0); });
    connect(vMid, &QAction::triggered, this, [setV] { setV(1); });
    connect(vBot, &QAction::triggered, this, [setV] { setV(2); });
    RibbonGroup *para = page->addGroup(tr("Paragraph"));
    para->beginColumn();
    para->addRow({ iconButton(para, m_alignL), iconButton(para, m_alignC), iconButton(para, m_alignR) });
    para->addRow({ iconButton(para, vTop), iconButton(para, vMid), iconButton(para, vBot) });

    // Drawing
    QAction *fill = objectAction(tr("Shape fill"), QStringLiteral("fill"));
    QAction *outline = objectAction(tr("Shape outline"), QStringLiteral("outline"));
    auto *fillMenu = new ColorMenu(tr("No fill"), this);
    auto *lineMenu = new ColorMenu(tr("No outline"), this);
    connect(fill, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) { const QColor c = m_fillColor; d->modifySelected([c](SlideObject &o) { o.fill = c; }, tr("Shape fill")); } });
    connect(fillMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { m_fillColor = c; m_fillBtn->setIndicatorColor(c); if (auto *d = pptDoc()) d->modifySelected([c](SlideObject &o) { o.fill = c; }, tr("Shape fill")); });
    connect(fillMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = pptDoc()) d->modifySelected([](SlideObject &o) { o.fill = QColor(); }, tr("Shape fill")); });
    connect(outline, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) { const QColor c = m_lineColor; d->modifySelected([c](SlideObject &o) { o.line = c; }, tr("Shape outline")); } });
    connect(lineMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { m_lineColor = c; m_lineBtn->setIndicatorColor(c); if (auto *d = pptDoc()) d->modifySelected([c](SlideObject &o) { o.line = c; }, tr("Shape outline")); });
    connect(lineMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = pptDoc()) d->modifySelected([](SlideObject &o) { o.line = QColor(); }, tr("Shape outline")); });
    auto *weightMenu = new QMenu(this);
    for (double w : { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0 })
        connect(weightMenu->addAction(tr("%1 px").arg(w)), &QAction::triggered, this, [this, w] { if (auto *d = pptDoc()) d->modifySelected([w](SlideObject &o) { o.lineWidth = w; }, tr("Line weight")); });
    QAction *weight = objectAction(tr("Line weight"), QStringLiteral("line-weight"));
    RibbonGroup *draw = page->addGroup(tr("Drawing"));
    m_fillBtn = iconButton(draw, fill, fillMenu);
    m_lineBtn = iconButton(draw, outline, lineMenu);
    m_fillBtn->setIndicatorColor(m_fillColor);
    m_lineBtn->setIndicatorColor(m_lineColor);
    auto *wb = iconButton(draw, weight, weightMenu);
    wb->setSplit(false);
    draw->beginColumn();
    draw->addRow({ m_fillBtn, m_lineBtn, wb });

    // Arrange
    auto *arrangeMenu = new QMenu(this);
    const struct { const char *t; int op; } arr[] = { { "Bring to front", 0 }, { "Send to back", 1 }, { "Bring forward", 2 }, { "Send backward", 3 } };
    for (const auto &a : arr) connect(arrangeMenu->addAction(tr(a.t)), &QAction::triggered, this, [this, op = a.op] { if (auto *d = pptDoc()) d->arrange(op); });
    auto *alignMenu = new QMenu(this);
    const struct { const char *t; int how; } al[] = { { "Align left", 0 }, { "Align center", 1 }, { "Align right", 2 }, { "Align top", 3 }, { "Align middle", 4 }, { "Align bottom", 5 } };
    for (const auto &a : al) connect(alignMenu->addAction(tr(a.t)), &QAction::triggered, this, [this, how = a.how] { if (auto *d = pptDoc()) d->alignObject(how); });
    QAction *arrange = objectAction(tr("Arrange"), QStringLiteral("bring-front"));
    QAction *align = objectAction(tr("Align"), QStringLiteral("obj-center"));
    QAction *dupObj = objectAction(tr("Duplicate"), QStringLiteral("slide-duplicate"), QKeySequence(QStringLiteral("Ctrl+D")));
    QAction *delObj = objectAction(tr("Delete"), QStringLiteral("trash"));
    connect(dupObj, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->duplicateObject(); });
    connect(delObj, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->deleteObject(); });
    RibbonGroup *arr2 = page->addGroup(tr("Arrange"));
    arr2->addLarge(arrange, arrangeMenu);
    arr2->addLarge(align, alignMenu);
    arr2->beginColumn();
    arr2->addSmall(dupObj);
    arr2->addSmall(delObj);
}

// ------------------------------------------------------------------ Insert
void PptWorkspace::buildInsert()
{
    RibbonPage *page = m_ribbon->addPage(tr("Insert"));
    const struct { const char *t, *icon; SlideObject::Type type; } shapes[] = {
        { "Text\nBox", "text-box", SlideObject::Text }, { "Rectangle", "rect", SlideObject::Rect }, { "Rounded\nRectangle", "round-rect", SlideObject::RoundRect },
        { "Ellipse", "ellipse", SlideObject::Ellipse }, { "Line", "line", SlideObject::Line } };
    RibbonGroup *draw = page->addGroup(tr("Text & shapes"));
    for (const auto &s : shapes) {
        QAction *a = editAction(tr(s.t), QLatin1String(s.icon));
        const SlideObject::Type type = s.type;
        connect(a, &QAction::triggered, this, [this, type] { if (auto *d = pptDoc()) d->startInsert(type); });
        draw->addLarge(a);
    }
    QAction *pic = editAction(tr("Picture"), QStringLiteral("image"));
    connect(pic, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->insertPicture(); });
    page->addGroup(tr("Images"))->addLarge(pic);
    QAction *slide = editAction(tr("New\nSlide"), QStringLiteral("slide-new"));
    connect(slide, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->newSlide(); });
    page->addGroup(tr("Slides"))->addLarge(slide);
}

// ------------------------------------------------------------------ Design
void PptWorkspace::buildDesign()
{
    RibbonPage *page = m_ribbon->addPage(tr("Design"));
    // Preset themes: background colour (the text colour of new slides adapts automatically).
    auto *themes = new QMenu(this);
    const struct { const char *name, *color; } presets[] = { { "White", "#FFFFFF" }, { "Soft grey", "#F1F5F9" }, { "Sky", "#E0F2FE" }, { "Warm", "#FFF7ED" },
                                                             { "Mint", "#ECFDF5" }, { "Slate (dark)", "#1E293B" }, { "Midnight (dark)", "#0F172A" }, { "Charcoal (dark)", "#262626" } };
    for (const auto &p : presets) {
        const QColor c(QLatin1String(p.color));
        connect(themes->addAction(tr(p.name)), &QAction::triggered, this, [this, c] {
            if (auto *d = pptDoc()) d->modifySlide([c](Slide &s) { s.background = c; }, tr("Background"));
        });
    }
    QAction *variants = editAction(tr("Slide\nThemes"), QStringLiteral("palette"));
    page->addGroup(tr("Themes"))->addLarge(variants, themes);

    auto *bgMenu = new ColorMenu(tr("White"), this);
    connect(bgMenu, &ColorMenu::colorPicked, this, [this](const QColor &c) { if (auto *d = pptDoc()) d->modifySlide([c](Slide &s) { s.background = c; }, tr("Background")); });
    connect(bgMenu, &ColorMenu::noColorPicked, this, [this] { if (auto *d = pptDoc()) d->modifySlide([](Slide &s) { s.background = QColor(Qt::white); }, tr("Background")); });
    QAction *bg = editAction(tr("Background\nColor"), QStringLiteral("background"));
    QAction *bgAll = editAction(tr("Apply to\nall slides"), QStringLiteral("apply-all"));
    connect(bgAll, &QAction::triggered, this, [this] {
        auto *d = pptDoc();
        if (!d) return;
        const QColor c = d->state().background;
        d->modifyAllSlides([c](Slide &s) { s.background = c; }, tr("Background (all slides)"));
    });
    RibbonGroup *g = page->addGroup(tr("Background"));
    g->addLarge(bg, bgMenu);
    g->addLarge(bgAll);
}

// ------------------------------------------------------------------ Transitions
void PptWorkspace::buildTransitions()
{
    RibbonPage *page = m_ribbon->addPage(tr("Transitions"));
    m_transCombo = makeCombo(Ppt::transitionNames(), 140);
    m_transDur = makeCombo({ tr("0.3 s"), tr("0.5 s"), tr("0.7 s"), tr("1.0 s"), tr("1.5 s"), tr("2.0 s") }, 140);
    m_advance = makeCombo({ tr("On click"), tr("After 3 s"), tr("After 5 s"), tr("After 10 s"), tr("After 15 s"), tr("After 30 s") }, 140);
    static const int durations[] = { 300, 500, 700, 1000, 1500, 2000 };
    static const int advances[] = { 0, 3000, 5000, 10000, 15000, 30000 };
    connect(m_transCombo, &QComboBox::activated, this, [this](int i) { if (auto *d = pptDoc()) d->modifySlide([i](Slide &s) { s.transition = Ppt::Transition(i); }, tr("Transition")); });
    connect(m_transDur, &QComboBox::activated, this, [this](int i) { if (auto *d = pptDoc()) d->modifySlide([i](Slide &s) { s.transitionMs = durations[qBound(0, i, 5)]; }, tr("Transition duration")); });
    connect(m_advance, &QComboBox::activated, this, [this](int i) { if (auto *d = pptDoc()) d->modifySlide([i](Slide &s) { s.advanceAfterMs = advances[qBound(0, i, 5)]; }, tr("Advance slide")); });
    RibbonGroup *g = page->addGroup(tr("Transition to this slide"));
    g->beginColumn();
    g->addRow({ m_transCombo });
    g->addRow({ m_transDur });
    RibbonGroup *t = page->addGroup(tr("Timing"));
    t->beginColumn();
    t->addRow({ m_advance });
    QAction *all = editAction(tr("Apply to\nall slides"), QStringLiteral("apply-all"));
    connect(all, &QAction::triggered, this, [this] {
        auto *d = pptDoc();
        if (!d) return;
        const auto s = d->state();
        d->modifyAllSlides([s](Slide &sl) { sl.transition = s.transition; sl.transitionMs = s.transitionMs; sl.advanceAfterMs = s.advanceAfterMs; }, tr("Transition (all slides)"));
    });
    QAction *preview = makeAction(tr("Preview"), QStringLiteral("play-circle"));
    connect(preview, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->startShow(false); });
    page->addGroup(tr("Preview"))->addLarge(preview);
    page->addGroup(tr("Apply"))->addLarge(all);
}

// ------------------------------------------------------------------ Animations
void PptWorkspace::buildAnimations()
{
    RibbonPage *page = m_ribbon->addPage(tr("Animations"));
    m_effect = makeCombo(Ppt::effectNames(), 160);
    m_trigger = makeCombo(Ppt::triggerNames(), 160);
    m_animDur = makeCombo({ tr("0.3 s"), tr("0.6 s"), tr("1.0 s"), tr("1.5 s"), tr("2.0 s"), tr("3.0 s") }, 100);
    m_animDelay = makeCombo({ tr("No delay"), tr("0.5 s"), tr("1.0 s"), tr("2.0 s") }, 100);
    static const int durs[] = { 300, 600, 1000, 1500, 2000, 3000 };
    static const int delays[] = { 0, 500, 1000, 2000 };
    connect(m_effect, &QComboBox::activated, this, [this](int i) { if (auto *d = pptDoc()) d->modifySelected([i](SlideObject &o) { o.anim.effect = Ppt::Effect(i); }, tr("Animation")); });
    connect(m_trigger, &QComboBox::activated, this, [this](int i) { if (auto *d = pptDoc()) d->modifySelected([i](SlideObject &o) { o.anim.trigger = Ppt::Trigger(i); }, tr("Animation start")); });
    connect(m_animDur, &QComboBox::activated, this, [this](int i) { if (auto *d = pptDoc()) d->modifySelected([i](SlideObject &o) { o.anim.durationMs = durs[qBound(0, i, 5)]; }, tr("Animation duration")); });
    connect(m_animDelay, &QComboBox::activated, this, [this](int i) { if (auto *d = pptDoc()) d->modifySelected([i](SlideObject &o) { o.anim.delayMs = delays[qBound(0, i, 3)]; }, tr("Animation delay")); });
    RibbonGroup *g = page->addGroup(tr("Animation (selected object)"));
    g->beginColumn();
    g->addRow({ m_effect });
    g->addRow({ m_trigger });
    RibbonGroup *t = page->addGroup(tr("Timing"));
    t->beginColumn();
    t->addRow({ m_animDur });
    t->addRow({ m_animDelay });
    m_animPaneAct = makeAction(tr("Animation\nPane"), QStringLiteral("animation"), {}, true);
    connect(m_animPaneAct, &QAction::triggered, this, [this](bool on) { if (auto *d = pptDoc()) d->setAnimationPaneVisible(on); });
    page->addGroup(tr("Advanced"))->addLarge(m_animPaneAct);
}

// ------------------------------------------------------------------ Slide Show
void PptWorkspace::buildShow()
{
    RibbonPage *page = m_ribbon->addPage(tr("Slide Show"));
    QAction *begin = makeAction(tr("From\nBeginning"), QStringLiteral("play"), QKeySequence(Qt::Key_F5));
    QAction *current = makeAction(tr("From Current\nSlide"), QStringLiteral("play-circle"), QKeySequence(QStringLiteral("Shift+F5")));
    connect(begin, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->startShow(true); });
    connect(current, &QAction::triggered, this, [this] { if (auto *d = pptDoc()) d->startShow(false); });
    RibbonGroup *g = page->addGroup(tr("Start slide show"));
    g->addLarge(begin);
    g->addLarge(current);
}

// ------------------------------------------------------------------ View
void PptWorkspace::buildView()
{
    RibbonPage *page = m_ribbon->addPage(tr("View"));
    m_notesAct = makeAction(tr("Notes"), QStringLiteral("notes"), {}, true);
    m_notesAct->setChecked(true);
    connect(m_notesAct, &QAction::triggered, this, [this](bool on) { if (auto *d = pptDoc()) d->setNotesVisible(on); });
    RibbonGroup *show = page->addGroup(tr("Show"));
    show->addLarge(m_notesAct);
    m_readMode = makeAction(tr("Read\nMode"), QStringLiteral("eye"), {}, true);
    connect(m_readMode, &QAction::triggered, this, [this](bool on) { if (auto *d = pptDoc()) d->setViewMode(on); });
    page->addGroup(tr("Views"))->addLarge(m_readMode);
}

void PptWorkspace::buildStatusBar()
{
    m_slideLabel = m_status->addLeftText();
    m_modeLabel = m_status->addLeftText();
}

void PptWorkspace::syncStatus()
{
    auto *d = pptDoc();
    if (!d) return;
    const auto s = d->state();
    m_slideLabel->setText(tr("Slide %1 of %2").arg(s.slideIndex + 1).arg(s.slideCount));
    m_modeLabel->setText(d->isViewMode() ? tr("View mode") : tr("Editing"));
}

void PptWorkspace::syncState()
{
    auto *d = pptDoc();
    if (!d) return;
    const auto s = d->state();
    const bool editable = !d->isViewMode();
    auto set = [](QAction *a, bool on) { QSignalBlocker b(a); a->setChecked(on); };
    set(m_bold, s.hasSel && s.obj.bold);
    set(m_italic, s.hasSel && s.obj.italic);
    set(m_underline, s.hasSel && s.obj.underline);
    set(m_alignL, s.hasSel && s.obj.align == 0);
    set(m_alignC, s.hasSel && s.obj.align == 1);
    set(m_alignR, s.hasSel && s.obj.align == 2);
    set(m_animPaneAct, d->animationPaneVisible());
    set(m_notesAct, d->notesVisible());
    set(m_readMode, d->isViewMode());

    for (QAction *a : m_objActions) a->setEnabled(editable && s.hasSel);
    m_undo->setEnabled(editable && s.canUndo);
    m_redo->setEnabled(editable && s.canRedo);
    m_copy->setEnabled(s.hasSel);

    static const int durations[] = { 300, 500, 700, 1000, 1500, 2000 }, advances[] = { 0, 3000, 5000, 10000, 15000, 30000 };
    static const int adur[] = { 300, 600, 1000, 1500, 2000, 3000 }, adelay[] = { 0, 500, 1000, 2000 };
    auto indexOf = [](const int *arr, int n, int v) { for (int i = 0; i < n; ++i) if (arr[i] == v) return i; return 0; };
    {
        QSignalBlocker b1(m_transCombo), b2(m_transDur), b3(m_advance), b4(m_effect), b5(m_trigger), b6(m_animDur), b7(m_animDelay), b8(m_size);
        m_transCombo->setCurrentIndex(int(s.transition));
        m_transDur->setCurrentIndex(indexOf(durations, 6, s.transitionMs));
        m_advance->setCurrentIndex(indexOf(advances, 6, s.advanceAfterMs));
        if (s.hasSel) {
            m_effect->setCurrentIndex(int(s.obj.anim.effect));
            m_trigger->setCurrentIndex(int(s.obj.anim.trigger));
            m_animDur->setCurrentIndex(indexOf(adur, 6, s.obj.anim.durationMs));
            m_animDelay->setCurrentIndex(indexOf(adelay, 4, s.obj.anim.delayMs));
            m_size->setCurrentText(QString::number(s.obj.fontSize, 'g', 4));
        }
    }
    for (QWidget *w : { static_cast<QWidget *>(m_transCombo), static_cast<QWidget *>(m_transDur), static_cast<QWidget *>(m_advance) }) w->setEnabled(editable);
    for (QWidget *w : { static_cast<QWidget *>(m_effect), static_cast<QWidget *>(m_trigger), static_cast<QWidget *>(m_animDur), static_cast<QWidget *>(m_animDelay), static_cast<QWidget *>(m_size) })
        w->setEnabled(editable && s.hasSel);
}

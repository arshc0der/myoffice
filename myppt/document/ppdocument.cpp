#include "ppdocument.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/filemanager.h"
#include "core/fileio.h"
#include "core/thememanager.h"
#include "myppt/editor/slidecanvas.h"
#include "myppt/editor/slidepainter.h"
#include "myppt/editor/slideshow.h"
#include "myppt/importexport/pptxio.h"
#include "ui/widgets/toast.h"

QList<SlideObject> PptDocument::s_clip;

namespace {
QColor inkFor(const QColor &bg) { return bg.isValid() && bg.lightness() < 128 ? QColor(Qt::white) : QColor("#1F2937"); }
const QSize kThumb(150, 84);
}

PptDocument::PptDocument(QWidget *parent) : DocumentView(parent)
{
    static int counter = 0;
    m_untitled = tr("Presentation %1").arg(++counter);

    Slide first;
    SlideObject title = makeObject(SlideObject::Text);
    title.rect = { 100, 220, 1080, 130 };
    title.text = tr("Presentation title");
    title.fontSize = 54;
    title.bold = true;
    title.align = 1;
    title.valign = 1;
    SlideObject sub = makeObject(SlideObject::Text);
    sub.rect = { 200, 370, 880, 70 };
    sub.text = tr("Subtitle");
    sub.fontSize = 28;
    sub.align = 1;
    sub.textColor = QColor("#64748B");
    first.objects << title << sub;
    m_deck.slides << first;

    m_canvas = new SlideCanvas([this]() -> Slide * { return m_deck.slides.isEmpty() ? nullptr : &m_deck.slides[m_cur]; }, this);

    m_thumbs = new QListWidget(this);
    m_thumbs->setIconSize(kThumb);
    m_thumbs->setSpacing(4);
    m_thumbs->setDragDropMode(QAbstractItemView::InternalMove);
    m_thumbs->setDefaultDropAction(Qt::MoveAction);
    m_thumbs->setMinimumWidth(210);
    m_thumbs->setMaximumWidth(280);
    m_thumbs->setAccessibleName(tr("Slides"));

    m_notes = new QPlainTextEdit(this);
    m_notes->setPlaceholderText(tr("Click to add speaker notes"));
    m_notes->setFixedHeight(84);
    m_notes->setAccessibleName(tr("Speaker notes"));

    m_animList = new QListWidget(this);
    m_animList->setAccessibleName(tr("Animation pane"));
    auto *paneTitle = new QLabel(tr("Animation pane"), this);
    paneTitle->setContentsMargins(10, 8, 10, 4);
    paneTitle->setFont(ThemeManager::instance().uiFont(Design::Type::body, QFont::DemiBold));
    m_animPane = new QWidget(this);
    auto *paneCol = new QVBoxLayout(m_animPane);
    paneCol->setContentsMargins(0, 0, 0, 0);
    paneCol->setSpacing(0);
    paneCol->addWidget(paneTitle);
    paneCol->addWidget(m_animList, 1);
    m_animPane->setMinimumWidth(230);
    m_animPane->setMaximumWidth(320);
    m_animPane->hide();

    auto *center = new QWidget(this);
    auto *centerCol = new QVBoxLayout(center);
    centerCol->setContentsMargins(0, 0, 0, 0);
    centerCol->setSpacing(0);
    centerCol->addWidget(m_canvas, 1);
    centerCol->addWidget(m_notes);

    auto *split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    split->addWidget(m_thumbs);
    split->addWidget(center);
    split->addWidget(m_animPane);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 0);
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->addWidget(split);

    m_thumbTimer.setSingleShot(true);
    m_thumbTimer.setInterval(120);
    connect(&m_thumbTimer, &QTimer::timeout, this, [this] { refreshCurrentThumb(); rebuildAnimPane(); });

    connect(m_canvas, &SlideCanvas::checkpointRequested, this, [this](const QString &t) {
        if (t == tr("Nudge")) checkpoint(t, QStringLiteral("nudge"));
        else checkpoint(t);
    });
    connect(m_canvas, &SlideCanvas::modified, this, [this] { m_thumbTimer.start(); emit stateChanged(); });
    connect(m_canvas, &SlideCanvas::selectionChanged, this, [this] { emit stateChanged(); });
    connect(m_thumbs, &QListWidget::currentRowChanged, this, [this](int row) { if (!m_rebuilding && row >= 0) setCurrent(row); });
    connect(m_thumbs->model(), &QAbstractItemModel::rowsMoved, this, [this] { scheduleOrderSync(); });
    connect(m_thumbs->model(), &QAbstractItemModel::rowsInserted, this, [this] { if (!m_rebuilding) scheduleOrderSync(); });
    connect(m_animList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) { m_canvas->setSelected(it->data(Qt::UserRole).toInt()); m_canvas->setFocus(); });
    connect(m_notes, &QPlainTextEdit::textChanged, this, [this] {
        if (m_viewMode || m_rebuilding) return;
        checkpoint(tr("Edit notes"), QStringLiteral("notes%1").arg(m_cur));
        cur().notes = m_notes->toPlainText();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { styleViews(); });

    styleViews();
    rebuildAll();
    m_savedSerial = m_serial;
}

PptDocument::~PptDocument() = default;

QString PptDocument::displayName() const { return m_path.isEmpty() ? m_untitled : QFileInfo(m_path).fileName(); }

void PptDocument::styleViews()
{
    const Palette &pal = ThemeManager::instance().palette();
    const QString list = QStringLiteral("QListWidget{background:%1;border:none;color:%2;outline:0;}"
                                        "QListWidget::item{padding:6px;border-radius:6px;}"
                                        "QListWidget::item:selected{background:%3;color:%2;}")
                             .arg(pal.surface.name(), pal.text.name(), Design::mix(pal.surface, pal.accent, 0.3).name());
    m_thumbs->setStyleSheet(list);
    m_animList->setStyleSheet(list);
    m_notes->setStyleSheet(QStringLiteral("QPlainTextEdit{background:%1;color:%2;border:none;border-top:1px solid %3;padding:6px;}")
                               .arg(pal.surface.name(), pal.text.name(), pal.border.name()));
}

// ------------------------------------------------------------------ history
void PptDocument::checkpoint(const QString &text, const QString &key)
{
    Q_UNUSED(text);
    if (!key.isEmpty() && key == m_lastKey) return;
    m_lastKey = key;
    m_undo.append({ m_deck, m_cur, m_serial });
    if (m_undo.size() > 100) m_undo.removeFirst();
    m_redo.clear();
    m_serial = ++m_nextSerial;
    emit modifiedChanged();
}

void PptDocument::afterEdit()
{
    m_canvas->update();
    m_thumbTimer.start();
    emit stateChanged();
    emit statusChanged();
}

void PptDocument::undo()
{
    if (auto *n = focusedNotes()) { n->undo(); return; }
    if (m_undo.isEmpty() || m_viewMode) return;
    m_redo.append({ m_deck, m_cur, m_serial });
    const Snap s = m_undo.takeLast();
    m_deck = s.deck;
    m_cur = qBound(0, s.cur, int(m_deck.slides.size()) - 1);
    m_serial = s.serial;
    m_lastKey.clear();
    rebuildAll();
    emit modifiedChanged();
}

void PptDocument::redo()
{
    if (auto *n = focusedNotes()) { n->redo(); return; }
    if (m_redo.isEmpty() || m_viewMode) return;
    m_undo.append({ m_deck, m_cur, m_serial });
    const Snap s = m_redo.takeLast();
    m_deck = s.deck;
    m_cur = qBound(0, s.cur, int(m_deck.slides.size()) - 1);
    m_serial = s.serial;
    m_lastKey.clear();
    rebuildAll();
    emit modifiedChanged();
}

QPlainTextEdit *PptDocument::focusedNotes() const
{
    auto *w = qobject_cast<QPlainTextEdit *>(QApplication::focusWidget());
    return (w && w == m_notes) ? w : nullptr;
}

// ------------------------------------------------------------------ view rebuilding
void PptDocument::rebuildAll()
{
    rebuildThumbs();
    loadNotes();
    m_canvas->setSelected(-1);
    m_canvas->update();
    rebuildAnimPane();
    emit stateChanged();
    emit statusChanged();
}

void PptDocument::rebuildThumbs()
{
    m_rebuilding = true;
    {
        QSignalBlocker b1(m_thumbs);
        QSignalBlocker b2(m_thumbs->model());
        m_thumbs->clear();
        const qreal dpr = devicePixelRatioF();
        for (int i = 0; i < m_deck.slides.size(); ++i) {
            auto *it = new QListWidgetItem(QIcon(SlidePainter::renderSlide(m_deck.slides.at(i), kThumb, dpr)), QString::number(i + 1));
            it->setData(Qt::UserRole, i);
            m_thumbs->addItem(it);
        }
        m_thumbs->setCurrentRow(m_cur);
    }
    m_rebuilding = false;
}

void PptDocument::refreshCurrentThumb()
{
    if (QListWidgetItem *it = m_thumbs->item(m_cur))
        it->setIcon(QIcon(SlidePainter::renderSlide(cur(), kThumb, devicePixelRatioF())));
}

void PptDocument::rebuildAnimPane()
{
    m_animList->clear();
    const QStringList effects = Ppt::effectNames(), triggers = Ppt::triggerNames();
    int n = 0;
    for (int i = 0; i < cur().objects.size(); ++i) {
        const SlideObject &o = cur().objects.at(i);
        if (o.anim.effect == Ppt::Effect::None) continue;
        auto *it = new QListWidgetItem(tr("%1.  %2 — %3 (%4)").arg(++n).arg(objectLabel(o), effects.value(int(o.anim.effect)), triggers.value(int(o.anim.trigger))));
        it->setData(Qt::UserRole, i);
        m_animList->addItem(it);
    }
}

void PptDocument::loadNotes()
{
    QSignalBlocker b(m_notes);
    m_notes->setPlainText(cur().notes);
}

void PptDocument::scheduleOrderSync()
{
    if (m_syncPending || m_rebuilding) return;
    m_syncPending = true;
    QTimer::singleShot(0, this, [this] { m_syncPending = false; syncOrderFromThumbs(); });
}

void PptDocument::syncOrderFromThumbs()
{
    if (m_viewMode || m_thumbs->count() != m_deck.slides.size()) { rebuildThumbs(); return; }
    QList<int> order;
    bool identity = true;
    for (int r = 0; r < m_thumbs->count(); ++r) {
        order << m_thumbs->item(r)->data(Qt::UserRole).toInt();
        if (order.last() != r) identity = false;
    }
    if (identity) return;
    checkpoint(tr("Reorder slides"));
    Deck nd;
    for (int idx : order) nd.slides << m_deck.slides.at(idx);
    const int newCur = order.indexOf(m_cur);
    m_deck = nd;
    m_cur = newCur < 0 ? 0 : newCur;
    rebuildAll();
}

void PptDocument::setCurrent(int index)
{
    index = qBound(0, index, int(m_deck.slides.size()) - 1);
    m_cur = index;
    m_lastKey.clear();
    if (m_thumbs->currentRow() != index) {
        m_rebuilding = true;
        m_thumbs->setCurrentRow(index);
        m_rebuilding = false;
    }
    loadNotes();
    m_canvas->setSelected(-1);
    m_canvas->update();
    rebuildAnimPane();
    emit stateChanged();
    emit statusChanged();
}

// ------------------------------------------------------------------ state
PptDocument::State PptDocument::state() const
{
    State s;
    s.canUndo = !m_undo.isEmpty();
    s.canRedo = !m_redo.isEmpty();
    const int sel = m_canvas->selected();
    if (sel >= 0 && sel < cur().objects.size()) { s.hasSel = true; s.obj = cur().objects.at(sel); }
    s.background = cur().background;
    s.transition = cur().transition;
    s.transitionMs = cur().transitionMs;
    s.advanceAfterMs = cur().advanceAfterMs;
    s.slideIndex = m_cur;
    s.slideCount = int(m_deck.slides.size());
    return s;
}

void PptDocument::setViewMode(bool on)
{
    m_viewMode = on;
    applyViewMode();
    emit viewModeChanged();
}

void PptDocument::applyViewMode()
{
    m_canvas->setReadOnly(m_viewMode);
    m_notes->setReadOnly(m_viewMode);
    m_thumbs->setDragDropMode(m_viewMode ? QAbstractItemView::NoDragDrop : QAbstractItemView::InternalMove);
}

void PptDocument::activated()
{
    m_canvas->setFocus();
    emit stateChanged();
    emit statusChanged();
}

void PptDocument::aboutToClose()
{
    if (m_show) m_show->close();
}

// ------------------------------------------------------------------ slides
void PptDocument::insertSlideAt(int index, const Slide &s)
{
    checkpoint(tr("Slide"));
    m_deck.slides.insert(index, s);
    m_cur = index;
    rebuildAll();
}

void PptDocument::newSlide()
{
    if (m_viewMode) return;
    Slide s;
    s.background = cur().background;
    s.transition = cur().transition;
    s.transitionMs = cur().transitionMs;
    SlideObject title = makeObject(SlideObject::Text);
    title.rect = { 80, 40, 1120, 100 };
    title.text = tr("Slide title");
    title.fontSize = 40;
    title.bold = true;
    title.valign = 1;
    title.textColor = inkFor(s.background);
    SlideObject body = makeObject(SlideObject::Text);
    body.rect = { 80, 170, 1120, 480 };
    body.text = tr("Add your content here");
    body.fontSize = 26;
    body.textColor = inkFor(s.background);
    s.objects << title << body;
    insertSlideAt(m_cur + 1, s);
}

void PptDocument::duplicateSlide()
{
    if (m_viewMode) return;
    insertSlideAt(m_cur + 1, cur());
}

void PptDocument::deleteSlide()
{
    if (m_viewMode) return;
    checkpoint(tr("Delete slide"));
    if (m_deck.slides.size() <= 1) {
        m_deck.slides[0] = Slide(); // keep one (empty) slide
        m_cur = 0;
    } else {
        m_deck.slides.removeAt(m_cur);
        m_cur = qMin(m_cur, int(m_deck.slides.size()) - 1);
    }
    rebuildAll();
}

void PptDocument::moveSlide(int delta)
{
    const int to = m_cur + delta;
    if (m_viewMode || to < 0 || to >= m_deck.slides.size()) return;
    checkpoint(tr("Move slide"));
    m_deck.slides.move(m_cur, to);
    m_cur = to;
    rebuildAll();
}

// ------------------------------------------------------------------ objects
void PptDocument::startInsert(SlideObject::Type type)
{
    if (m_viewMode) return;
    m_canvas->setInsertTool(int(type));
}

void PptDocument::insertPicture()
{
    if (m_viewMode) return;
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("Insert picture"), dir, tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty()) return;
    QSettings().setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    const QImage img(path);
    if (img.isNull()) { emit message(tr("Could not read the picture."), Toast::Error); return; }
    insertImage(img);
}

void PptDocument::insertImage(QImage img)
{
    if (img.width() > 2400) img = img.scaledToWidth(2400, Qt::SmoothTransformation);
    const qreal s = qMin(2.0, qMin(800.0 / img.width(), 450.0 / img.height()));
    SlideObject o;
    o.type = SlideObject::Image;
    o.image = img;
    o.rect = QRectF(0, 0, img.width() * s, img.height() * s);
    o.rect.moveCenter(QPointF(Ppt::kSlideW / 2.0, Ppt::kSlideH / 2.0));
    o.fill = QColor();
    o.line = QColor();
    checkpoint(tr("Insert picture"));
    cur().objects << o;
    m_canvas->setSelected(int(cur().objects.size()) - 1);
    afterEdit();
}

void PptDocument::modifySelected(const std::function<void(SlideObject &)> &fn, const QString &text)
{
    const int i = m_canvas->selected();
    if (m_viewMode || i < 0 || i >= cur().objects.size()) return;
    checkpoint(text);
    fn(cur().objects[i]);
    afterEdit();
}

void PptDocument::modifySlide(const std::function<void(Slide &)> &fn, const QString &text)
{
    if (m_viewMode) return;
    checkpoint(text);
    fn(cur());
    afterEdit();
}

void PptDocument::modifyAllSlides(const std::function<void(Slide &)> &fn, const QString &text)
{
    if (m_viewMode) return;
    checkpoint(text);
    for (int i = 0; i < m_deck.slides.size(); ++i) fn(m_deck.slides[i]);
    rebuildThumbs();
    afterEdit();
}

void PptDocument::duplicateObject()
{
    const int i = m_canvas->selected();
    if (m_viewMode || i < 0 || i >= cur().objects.size()) return;
    checkpoint(tr("Duplicate"));
    SlideObject o = cur().objects.at(i);
    o.rect.translate(24, 24);
    cur().objects << o;
    m_canvas->setSelected(int(cur().objects.size()) - 1);
    afterEdit();
}

void PptDocument::deleteObject()
{
    const int i = m_canvas->selected();
    if (m_viewMode || i < 0 || i >= cur().objects.size()) return;
    checkpoint(tr("Delete"));
    cur().objects.removeAt(i);
    m_canvas->setSelected(-1);
    afterEdit();
}

void PptDocument::copy()
{
    if (auto *n = focusedNotes()) { n->copy(); return; }
    const int i = m_canvas->selected();
    if (i < 0 || i >= cur().objects.size()) return;
    s_clip = { cur().objects.at(i) };
    QApplication::clipboard()->setText(cur().objects.at(i).text); // replaces any older image on the system clipboard
}

void PptDocument::cut()
{
    if (auto *n = focusedNotes()) { if (!m_viewMode) n->cut(); return; }
    copy();
    deleteObject();
}

void PptDocument::paste()
{
    if (m_viewMode) return;
    if (auto *n = focusedNotes()) { n->paste(); return; }
    const QMimeData *md = QApplication::clipboard()->mimeData();
    if (md && md->hasImage()) {
        const QImage img = qvariant_cast<QImage>(md->imageData());
        if (!img.isNull()) { insertImage(img); return; }
    }
    if (s_clip.isEmpty()) return;
    checkpoint(tr("Paste"));
    for (SlideObject o : s_clip) {
        o.rect.translate(24, 24);
        cur().objects << o;
    }
    m_canvas->setSelected(int(cur().objects.size()) - 1);
    afterEdit();
}

void PptDocument::arrange(int op)
{
    const int i = m_canvas->selected();
    const int n = int(cur().objects.size());
    if (m_viewMode || i < 0 || i >= n) return;
    int to = i;
    if (op == 0) to = n - 1;
    else if (op == 1) to = 0;
    else if (op == 2) to = qMin(n - 1, i + 1);
    else to = qMax(0, i - 1);
    if (to == i) return;
    checkpoint(tr("Arrange"));
    cur().objects.move(i, to);
    m_canvas->setSelected(to);
    afterEdit();
}

void PptDocument::alignObject(int how)
{
    modifySelected([how](SlideObject &o) {
        QRectF r = o.rect;
        switch (how) {
        case 0: r.moveLeft(0); break;
        case 1: r.moveLeft((Ppt::kSlideW - r.width()) / 2); break;
        case 2: r.moveLeft(Ppt::kSlideW - r.width()); break;
        case 3: r.moveTop(0); break;
        case 4: r.moveTop((Ppt::kSlideH - r.height()) / 2); break;
        default: r.moveTop(Ppt::kSlideH - r.height()); break;
        }
        o.rect = r;
    }, tr("Align"));
}

// ------------------------------------------------------------------ panes & show
void PptDocument::startShow(bool fromBeginning)
{
    if (m_show) return;
    auto *show = new SlideShow(m_deck, fromBeginning ? 0 : m_cur);
    m_show = show;
    connect(show, &SlideShow::finished, this, [this](int idx) { setCurrent(idx); });
    show->run();
}

void PptDocument::setAnimationPaneVisible(bool on) { m_animPane->setVisible(on); }
bool PptDocument::animationPaneVisible() const { return m_animPane->isVisibleTo(const_cast<PptDocument *>(this)); }
void PptDocument::setNotesVisible(bool on) { m_notes->setVisible(on); }
bool PptDocument::notesVisible() const { return m_notes->isVisibleTo(const_cast<PptDocument *>(this)); }

// ------------------------------------------------------------------ files
void PptDocument::load(const QString &path, bool viewMode)
{
    m_path = path;
    m_viewMode = viewMode;
    applyViewMode();
    emit titleChanged();
    FileIO::readAsync(path, this, [this, path](const QByteArray &data, const QString &err) {
        QString perr = err;
        Deck d;
        int skipped = 0;
        if (!err.isEmpty() || !PptxIO::read(data, d, &perr, &skipped)) { emit loadFinished(false, perr); return; }
        m_deck = d;
        m_cur = 0;
        m_undo.clear();
        m_redo.clear();
        m_serial = m_savedSerial = m_nextSerial = 0;
        m_lastKey.clear();
        rebuildAll();
        emit titleChanged();
        emit modifiedChanged();
        emit viewModeChanged();
        if (skipped > 0) emit message(tr("%n unsupported element(s) (tables, charts, media) were skipped.", "", skipped), Toast::Info);
        emit loadFinished(true, QString());
    });
}

bool PptDocument::writeFile(const QString &path, QString *error)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("pdf")) return exportPdf(path, error);
    QByteArray bytes;
    if (ext == QLatin1String("png")) {
        const QPixmap pm = SlidePainter::renderSlide(cur(), QSize(1920, 1080), 1.0);
        QBuffer buf(&bytes);
        buf.open(QIODevice::WriteOnly);
        pm.save(&buf, "PNG");
    } else if (ext == QLatin1String("pptx")) {
        bytes = PptxIO::write(m_deck);
        if (bytes.isEmpty()) { if (error) *error = tr("Could not create the PPTX file."); return false; }
    } else {
        if (error) *error = tr("Unsupported format \"%1\".").arg(ext);
        return false;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) { if (error) *error = f.errorString(); return false; }
    return true;
}

bool PptDocument::saveToPath(const QString &path)
{
    QString err;
    if (!writeFile(path, &err)) { emit message(tr("Save failed: %1").arg(err), Toast::Error); return false; }
    m_path = path;
    m_savedSerial = m_serial;
    FileManager::instance().addRecent(path);
    emit titleChanged();
    emit modifiedChanged();
    emit message(tr("Saved %1").arg(QFileInfo(path).fileName()), Toast::Success);
    return true;
}

bool PptDocument::save()
{
    if (m_path.isEmpty() || QFileInfo(m_path).suffix().toLower() != QLatin1String("pptx")) return saveAs();
    if (m_viewMode) setViewMode(false);
    return saveToPath(m_path);
}

bool PptDocument::saveAs()
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), dir + QLatin1Char('/') + (m_path.isEmpty() ? m_untitled : QFileInfo(m_path).completeBaseName()),
                                                tr("PowerPoint Presentation (*.pptx)"));
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".pptx");
    QSettings().setValue(QStringLiteral("files/lastDir"), QFileInfo(path).absolutePath());
    return saveToPath(path);
}

bool PptDocument::exportTo(const QString &path)
{
    QString err;
    if (!writeFile(path, &err)) { emit message(tr("Export failed: %1").arg(err), Toast::Error); return false; }
    emit message(tr("Exported %1").arg(QFileInfo(path).fileName()), Toast::Success);
    return true;
}

void PptDocument::exportDialog(const QString &suffix)
{
    const QString dir = QSettings().value(QStringLiteral("files/lastDir")).toString();
    const QString path = QFileDialog::getSaveFileName(this, tr("Export"), dir + QLatin1Char('/') + (m_path.isEmpty() ? m_untitled : QFileInfo(m_path).completeBaseName()) + QLatin1Char('.') + suffix,
                                                      QStringLiteral("*.%1").arg(suffix));
    if (!path.isEmpty()) exportTo(path);
}

// One slide per PDF page (13.33 x 7.5 in, 16:9).
bool PptDocument::exportPdf(const QString &path, QString *error)
{
    QPdfWriter pdf(path);
    pdf.setResolution(96);
    pdf.setPageSize(QPageSize(QSizeF(338.667, 190.5), QPageSize::Millimeter));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    QPainter pa;
    if (!pa.begin(&pdf)) { if (error) *error = tr("Cannot write %1").arg(path); return false; }
    for (int i = 0; i < m_deck.slides.size(); ++i) {
        if (i > 0) pdf.newPage();
        SlidePainter::paintSlide(&pa, m_deck.slides.at(i), QRectF(0, 0, pdf.width(), pdf.height()));
    }
    pa.end();
    return true;
}

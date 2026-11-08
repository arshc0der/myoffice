#pragma once
#include <QList>
#include <QPointer>
#include <QTimer>
#include <functional>

#include "myppt/document/slidemodel.h"
#include "myppt/editor/slideshow.h"
#include "ui/workspace/workspace.h"

class QListWidget;
class QPlainTextEdit;
class QWidget;
class SlideCanvas;

// One MyPowerPoint presentation: slide thumbnails, slide canvas, speaker notes and the animation pane.
class PptDocument : public DocumentView
{
    Q_OBJECT
public:
    struct State {
        bool canUndo = false, canRedo = false, hasSel = false;
        SlideObject obj; // copy of the selected object (valid when hasSel)
        QColor background { Qt::white };
        Ppt::Transition transition = Ppt::Transition::None;
        int transitionMs = 700, advanceAfterMs = 0;
        int slideIndex = 0, slideCount = 1;
    };

    explicit PptDocument(QWidget *parent = nullptr);
    ~PptDocument() override;

    // DocumentView
    QString displayName() const override;
    QString filePath() const override { return m_path; }
    bool isModified() const override { return m_serial != m_savedSerial; }
    bool save() override;
    bool saveAs() override;
    bool isViewMode() const override { return m_viewMode; }
    void setViewMode(bool on) override;
    void load(const QString &path, bool viewMode) override;
    void aboutToClose() override;
    void activated() override;

    bool exportTo(const QString &path);
    void exportDialog(const QString &suffix);
    State state() const;

    // history & clipboard
    void undo();
    void redo();
    void copy();
    void cut();
    void paste();

    // slides
    void newSlide();
    void duplicateSlide();
    void deleteSlide();
    void moveSlide(int delta);
    void setCurrent(int index);

    // objects
    void startInsert(SlideObject::Type type);
    void insertPicture();
    void duplicateObject();
    void deleteObject();
    void modifySelected(const std::function<void(SlideObject &)> &fn, const QString &text);
    void modifySlide(const std::function<void(Slide &)> &fn, const QString &text);
    void modifyAllSlides(const std::function<void(Slide &)> &fn, const QString &text);
    void arrange(int op);        // 0 bring to front, 1 send to back, 2 forward, 3 backward
    void alignObject(int how);   // 0 left, 1 centre, 2 right, 3 top, 4 middle, 5 bottom (relative to the slide)

    // show & panes
    void startShow(bool fromBeginning);
    void setAnimationPaneVisible(bool on);
    bool animationPaneVisible() const;
    void setNotesVisible(bool on);
    bool notesVisible() const;

private:
    Slide &cur() { return m_deck.slides[m_cur]; }
    const Slide &cur() const { return m_deck.slides.at(m_cur); }
    void checkpoint(const QString &text, const QString &key = QString());
    void afterEdit();
    void rebuildAll();
    void rebuildThumbs();
    void refreshCurrentThumb();
    void rebuildAnimPane();
    void loadNotes();
    void scheduleOrderSync();
    void syncOrderFromThumbs();
    void insertImage(QImage img);
    void insertSlideAt(int index, const Slide &s);
    void applyViewMode();
    void styleViews();
    bool writeFile(const QString &path, QString *error);
    bool saveToPath(const QString &path);
    bool exportPdf(const QString &path, QString *error);
    QPlainTextEdit *focusedNotes() const;

    struct Snap { Deck deck; int cur; int serial; };

    Deck m_deck;
    int m_cur = 0;
    QList<Snap> m_undo, m_redo;
    int m_serial = 0, m_nextSerial = 0, m_savedSerial = 0;
    QString m_lastKey;
    QString m_path, m_untitled;
    bool m_viewMode = false, m_rebuilding = false, m_syncPending = false;

    SlideCanvas *m_canvas;
    QListWidget *m_thumbs, *m_animList;
    QWidget *m_animPane;
    QPlainTextEdit *m_notes;
    QTimer m_thumbTimer;
    QPointer<SlideShow> m_show;
    static QList<SlideObject> s_clip;
};

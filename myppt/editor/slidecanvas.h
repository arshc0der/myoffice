#pragma once
#include <QWidget>
#include <functional>

#include "myppt/document/slidemodel.h"

// Interactive view of one slide: select, move, resize (8 handles), draw new shapes, edit text.
// The canvas never stores slide pointers; it asks the provider for the current slide on every event.
class SlideCanvas : public QWidget
{
    Q_OBJECT
public:
    using SlideProvider = std::function<Slide *()>;
    explicit SlideCanvas(SlideProvider provider, QWidget *parent = nullptr);

    int selected() const { return m_sel; }
    void setSelected(int index);
    void setReadOnly(bool on);
    void setInsertTool(int objectType); // SlideObject::Type, or -1 to cancel
    void editText(int index);
    QSize sizeHint() const override { return { 800, 450 }; }

signals:
    void checkpointRequested(const QString &text); // emitted once before a change; the document snapshots for undo
    void modified();                               // slide content changed
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    enum class Mode { Idle, Move, Resize, Create };
    QRectF pageRect() const;
    QPointF toSlide(const QPointF &p) const;
    qreal scale() const;
    int hitObject(const QPointF &slidePos) const;
    int hitHandle(const QPointF &widgetPos) const;
    QPointF handlePos(const QRectF &r, int h) const;
    void begin(const QString &text);
    void updateCursor(const QPointF &widgetPos);

    SlideProvider m_provider;
    int m_sel = -1;
    int m_tool = -1;
    bool m_readOnly = false;
    Mode m_mode = Mode::Idle;
    bool m_started = false; // checkpoint already requested for the current gesture
    int m_handle = -1;
    QPointF m_pressPos;
    QRectF m_origRect;
};

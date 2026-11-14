#pragma once
#include <QPropertyAnimation>
#include <QStringList>
#include <QVector>
#include <QWidget>

struct ChartSeries { QString name; QVector<double> values; };
struct ChartData {
    QString title;
    QStringList categories;
    QVector<ChartSeries> series;
    bool isEmpty() const { return series.isEmpty() || categories.isEmpty(); }
};

// Custom-painted chart with animated reveal, grid, legend and hover tooltips (no Qt Charts dependency).
class ChartView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal reveal READ reveal WRITE setReveal)
public:
    enum Type { Column, Line, Area, Pie, Donut };

    explicit ChartView(QWidget *parent = nullptr);
    void setData(const ChartData &d, bool animate = true);
    const ChartData &data() const { return m_data; }
    void setType(Type t);
    Type type() const { return m_type; }
    qreal reveal() const { return m_reveal; }
    void setReveal(qreal v) { m_reveal = v; update(); }
    QSize sizeHint() const override { return { 460, 340 }; }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { m_hs = m_hc = -1; update(); }

private:
    struct Hit { QRectF rect; int s, c; };
    struct Slice { qreal startDeg, spanDeg; };
    void animate();
    void paintCartesian(QPainter &p, const QRectF &area);
    void paintPie(QPainter &p, const QRectF &area);
    void paintTooltip(QPainter &p);
    ChartData m_data;
    Type m_type = Column;
    qreal m_reveal = 1.0;
    QVector<Hit> m_hits;
    QVector<Slice> m_slices;
    QPointF m_pieCenter;
    qreal m_pieR = 0, m_pieInner = 0;
    int m_hs = -1, m_hc = -1;
    QPointF m_mouse;
    QPropertyAnimation m_anim;
};

class IconButton;

// Side panel hosting a ChartView with chart-type buttons, export and close.
class ChartPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ChartPanel(QWidget *parent = nullptr);
    ChartView *view() const { return m_view; }
    void setChart(const ChartData &d, ChartView::Type type);

signals:
    void closeRequested();
    void exportRequested();
    void typeChanged(ChartView::Type type);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void selectType(ChartView::Type t, bool emitSignal);
    ChartView *m_view;
    QVector<IconButton *> m_typeButtons;
};

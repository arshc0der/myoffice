#include "chartpanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <cmath>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "ui/widgets/iconbutton.h"
#include "ui/widgets/themeutil.h"

namespace {
// Office default chart palette
const QColor kColors[8] = { QColor("#4472C4"), QColor("#ED7D31"), QColor("#A5A5A5"), QColor("#FFC000"),
                            QColor("#5B9BD5"), QColor("#70AD47"), QColor("#264478"), QColor("#9E480E") };
QColor seriesColor(int i) { return kColors[i % 8]; }

void niceScale(double lo, double hi, double *outLo, double *outHi, double *step)
{
    if (hi - lo < 1e-12) hi = lo + 1;
    const double raw = (hi - lo) / 5.0, mag = std::pow(10.0, std::floor(std::log10(raw))), norm = raw / mag;
    const double nice = norm <= 1 ? 1 : norm <= 2 ? 2 : norm <= 5 ? 5 : 10;
    *step = nice * mag;
    *outLo = std::floor(lo / *step) * *step;
    *outHi = std::ceil(hi / *step) * *step;
}

QString shortNum(double v)
{
    const double a = std::fabs(v);
    if (a >= 1e9) return QString::number(v / 1e9, 'g', 3) + QLatin1Char('B');
    if (a >= 1e6) return QString::number(v / 1e6, 'g', 3) + QLatin1Char('M');
    if (a >= 1e4) return QString::number(v / 1e3, 'g', 3) + QLatin1Char('k');
    return QString::number(v, 'g', 4);
}
}

ChartView::ChartView(QWidget *parent) : QWidget(parent), m_anim(this, "reveal")
{
    setMouseTracking(true);
    setMinimumSize(280, 220);
    setAccessibleName(tr("Chart"));
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void ChartView::animate()
{
    m_anim.stop();
    const int ms = ThemeManager::instance().duration(700);
    if (ms == 0) { m_reveal = 1.0; update(); return; }
    m_anim.setDuration(ms);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.start();
}

void ChartView::setData(const ChartData &d, bool animate)
{
    m_data = d;
    m_hs = m_hc = -1;
    if (animate) this->animate(); else update();
}

void ChartView::setType(Type t) { m_type = t; m_hs = m_hc = -1; animate(); }

void ChartView::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(rect(), pal.surface);
    m_hits.clear();
    m_slices.clear();
    if (m_data.isEmpty()) {
        p.setPen(pal.textSecondary);
        p.setFont(theme.uiFont(Design::Type::body));
        p.drawText(rect().adjusted(20, 0, -20, 0), Qt::AlignCenter | Qt::TextWordWrap, tr("Select a range that contains numbers, then choose a chart type."));
        return;
    }
    p.setPen(pal.text);
    p.setFont(theme.uiFont(Design::Type::subtitle, QFont::DemiBold));
    p.drawText(QRectF(16, 10, width() - 32, 24), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(m_data.title, Qt::ElideRight, width() - 32));

    // Legend: series (cartesian) or categories (pie).
    const bool pie = m_type == Pie || m_type == Donut;
    QStringList names;
    if (pie) names = m_data.categories.mid(0, 8);
    else for (const ChartSeries &s : m_data.series) names << s.name;
    p.setFont(theme.uiFont(Design::Type::caption));
    qreal x = 16, y = 44;
    if (pie || names.size() > 1) {
        for (int i = 0; i < names.size(); ++i) {
            const qreal w = p.fontMetrics().horizontalAdvance(names[i]) + 26;
            if (x + w > width() - 12 && x > 16) { x = 16; y += 18; }
            p.setPen(Qt::NoPen);
            p.setBrush(seriesColor(i));
            p.drawEllipse(QPointF(x + 5, y), 5, 5);
            p.setPen(pal.textSecondary);
            p.drawText(QPointF(x + 16, y + 4), names[i]);
            x += w;
        }
        y += 14;
    } else y = 30;
    const QRectF area(0, y + 6, width(), height() - y - 10);
    if (pie) paintPie(p, area); else paintCartesian(p, area);
    paintTooltip(p);
}

void ChartView::paintCartesian(QPainter &p, const QRectF &area)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    const int n = m_data.categories.size(), ns = m_data.series.size();
    double lo = 0, hi = 0;
    for (const ChartSeries &s : m_data.series) for (double v : s.values) { lo = qMin(lo, v); hi = qMax(hi, v); }
    double yLo, yHi, step;
    niceScale(lo, hi, &yLo, &yHi, &step);
    const QRectF plot(area.left() + 54, area.top() + 6, area.width() - 54 - 16, area.height() - 6 - 34);
    auto yOf = [&](double v) { return plot.bottom() - (v - yLo) / (yHi - yLo) * plot.height(); };
    const qreal y0 = yOf(0), e = m_reveal;

    p.setFont(theme.uiFont(Design::Type::caption - 0.5));
    for (double v = yLo; v <= yHi + step * 0.001; v += step) {
        const qreal y = yOf(v);
        p.setPen(QPen(pal.border, 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(pal.textSecondary);
        p.drawText(QRectF(area.left(), y - 9, 48, 18), Qt::AlignRight | Qt::AlignVCenter, shortNum(v));
    }
    p.setPen(QPen(pal.textSecondary, 1.2));
    p.drawLine(QPointF(plot.left(), y0), QPointF(plot.right(), y0));

    const qreal groupW = plot.width() / qMax(1, n);
    if (m_type == Column) {
        const qreal bw = groupW * 0.72 / ns;
        for (int c = 0; c < n; ++c)
            for (int s = 0; s < ns; ++s) {
                const double v = m_data.series[s].values.value(c);
                const qreal x = plot.left() + c * groupW + groupW * 0.14 + s * bw;
                const qreal y1 = y0 + (yOf(v) - y0) * e;
                const QRectF r(x, qMin(y0, y1), qMax<qreal>(2, bw - 2), qMax<qreal>(1, qAbs(y1 - y0)));
                QColor col = seriesColor(s);
                if (s == m_hs && c == m_hc) col = col.lighter(120);
                p.setPen(Qt::NoPen);
                p.setBrush(col);
                p.drawRect(r);
                m_hits.append({ r, s, c });
            }
    } else {
        p.save();
        p.setClipRect(QRectF(plot.left() - 8, area.top(), (plot.width() + 16) * qMin<qreal>(1.0, e * 1.05), area.height()));
        for (int s = 0; s < ns; ++s) {
            QPolygonF pts;
            for (int c = 0; c < n; ++c) pts << QPointF(plot.left() + (c + 0.5) * groupW, yOf(m_data.series[s].values.value(c)));
            const QColor col = seriesColor(s);
            if (m_type == Area && pts.size() > 1) {
                QPolygonF poly = pts;
                poly << QPointF(pts.last().x(), y0) << QPointF(pts.first().x(), y0);
                QLinearGradient g(0, plot.top(), 0, plot.bottom());
                QColor top = col, bottom = col;
                top.setAlpha(120);
                bottom.setAlpha(8);
                g.setColorAt(0, top);
                g.setColorAt(1, bottom);
                p.setPen(Qt::NoPen);
                p.setBrush(g);
                p.drawPolygon(poly);
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(col, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(pts);
            for (int c = 0; c < n; ++c) {
                const bool hov = s == m_hs && c == m_hc;
                p.setPen(QPen(pal.surface, 2));
                p.setBrush(col);
                p.drawEllipse(pts[c], hov ? 6 : 4, hov ? 6 : 4);
                m_hits.append({ QRectF(pts[c].x() - 9, pts[c].y() - 9, 18, 18), s, c });
            }
        }
        p.restore();
    }
    p.setPen(pal.textSecondary);
    p.setFont(theme.uiFont(Design::Type::caption - 0.5));
    const int every = qMax(1, int(std::ceil(n / 12.0)));
    for (int c = 0; c < n; c += every)
        p.drawText(QRectF(plot.left() + c * groupW - 6, plot.bottom() + 6, groupW * every + 12, 20), Qt::AlignHCenter | Qt::AlignTop,
                   p.fontMetrics().elidedText(m_data.categories[c], Qt::ElideRight, int(groupW * every + 8)));
}

void ChartView::paintPie(QPainter &p, const QRectF &area)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    const QVector<double> &vals = m_data.series.first().values;
    double total = 0;
    for (double v : vals) total += qMax(0.0, v);
    if (total <= 0) {
        p.setPen(pal.textSecondary);
        p.drawText(area, Qt::AlignCenter, tr("Pie charts need positive values."));
        return;
    }
    m_pieCenter = area.center();
    m_pieR = qMax<qreal>(20, qMin(area.width(), area.height()) / 2 - 14);
    m_pieInner = m_type == Donut ? m_pieR * 0.58 : 0;
    qreal cursor = 90; // start at 12 o'clock, go clockwise
    const qreal e = m_reveal;
    for (int i = 0; i < vals.size(); ++i) {
        const double v = qMax(0.0, vals[i]);
        const qreal span = -(v / total) * 360.0 * e;
        m_slices.append({ cursor, span });
        QPointF off(0, 0);
        if (i == m_hc) { const qreal mid = (cursor + span / 2) * M_PI / 180.0; off = QPointF(std::cos(mid) * 7, -std::sin(mid) * 7); }
        const QRectF outer(m_pieCenter.x() - m_pieR + off.x(), m_pieCenter.y() - m_pieR + off.y(), m_pieR * 2, m_pieR * 2);
        QPainterPath path;
        path.arcMoveTo(outer, cursor);
        path.arcTo(outer, cursor, span);
        if (m_type == Donut) {
            const QRectF inner(m_pieCenter.x() - m_pieInner + off.x(), m_pieCenter.y() - m_pieInner + off.y(), m_pieInner * 2, m_pieInner * 2);
            path.arcTo(inner, cursor + span, -span);
        } else path.lineTo(m_pieCenter + off);
        path.closeSubpath();
        p.setPen(QPen(pal.surface, 2));
        p.setBrush(seriesColor(i));
        p.drawPath(path);
        if (v / total > 0.05 && e > 0.95) {
            const qreal mid = (cursor + span / 2) * M_PI / 180.0, rr = m_type == Donut ? (m_pieR + m_pieInner) / 2 : m_pieR * 0.65;
            p.setPen(Qt::white);
            p.setFont(theme.uiFont(Design::Type::caption, QFont::DemiBold));
            p.drawText(QRectF(m_pieCenter.x() + off.x() + std::cos(mid) * rr - 24, m_pieCenter.y() + off.y() - std::sin(mid) * rr - 9, 48, 18), Qt::AlignCenter,
                       QString::number(v / total * 100.0, 'f', 0) + QLatin1Char('%'));
        }
        cursor += span;
    }
    if (m_type == Donut) {
        p.setPen(pal.text);
        p.setFont(theme.uiFont(Design::Type::title, QFont::DemiBold));
        p.drawText(QRectF(m_pieCenter.x() - m_pieInner, m_pieCenter.y() - 22, m_pieInner * 2, 26), Qt::AlignCenter, shortNum(total));
        p.setPen(pal.textSecondary);
        p.setFont(theme.uiFont(Design::Type::caption));
        p.drawText(QRectF(m_pieCenter.x() - m_pieInner, m_pieCenter.y() + 2, m_pieInner * 2, 18), Qt::AlignCenter, tr("Total"));
    }
}

void ChartView::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->position();
    int hs = -1, hc = -1;
    if (m_type == Pie || m_type == Donut) {
        const QPointF d = m_mouse - m_pieCenter;
        const qreal dist = std::hypot(d.x(), d.y());
        if (dist <= m_pieR + 8 && dist >= m_pieInner) {
            const qreal ang = std::fmod(std::atan2(-d.y(), d.x()) * 180.0 / M_PI + 360.0, 360.0);
            for (int i = 0; i < m_slices.size(); ++i) {
                const qreal start = std::fmod(m_slices[i].startDeg + 720.0, 360.0), diff = std::fmod(start - ang + 720.0, 360.0);
                if (diff <= -m_slices[i].spanDeg) { hs = 0; hc = i; break; }
            }
        }
    } else {
        for (const Hit &h : m_hits) if (h.rect.contains(m_mouse)) { hs = h.s; hc = h.c; break; }
    }
    if (hs != m_hs || hc != m_hc) { m_hs = hs; m_hc = hc; }
    setCursor(hc >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    update();
}

void ChartView::paintTooltip(QPainter &p)
{
    if (m_hc < 0 || m_hc >= m_data.categories.size()) return;
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QStringList lines;
    lines << m_data.categories[m_hc];
    if (m_type == Pie || m_type == Donut) {
        double total = 0;
        for (double v : m_data.series.first().values) total += qMax(0.0, v);
        const double v = m_data.series.first().values.value(m_hc);
        lines << QStringLiteral("%1  (%2%)").arg(QString::number(v, 'g', 8), QString::number(total > 0 ? v / total * 100.0 : 0.0, 'f', 1));
    } else {
        lines << QStringLiteral("%1: %2").arg(m_data.series.value(m_hs).name, QString::number(m_data.series.value(m_hs).values.value(m_hc), 'g', 8));
    }
    p.setFont(theme.uiFont(Design::Type::caption));
    const QFontMetrics fm = p.fontMetrics();
    int w = 0;
    for (const QString &l : lines) w = qMax(w, fm.horizontalAdvance(l));
    QRectF box(m_mouse.x() + 14, m_mouse.y() + 14, w + 20, lines.size() * 18 + 12);
    if (box.right() > width() - 4) box.moveLeft(m_mouse.x() - box.width() - 10);
    if (box.bottom() > height() - 4) box.moveTop(m_mouse.y() - box.height() - 10);
    p.setBrush(pal.text);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(box, 7, 7);
    p.setPen(pal.surface);
    for (int i = 0; i < lines.size(); ++i) p.drawText(QRectF(box.left() + 10, box.top() + 6 + i * 18, box.width() - 14, 18), Qt::AlignVCenter, lines[i]);
}

// ------------------------------------------------------------------ ChartPanel
ChartPanel::ChartPanel(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(320);
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(1, 0, 0, 0);
    col->setSpacing(0);
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(10, 6, 6, 6);
    bar->setSpacing(2);
    const struct { const char *icon, *tip; ChartView::Type type; } kinds[] = {
        { "chart-column", "Column chart", ChartView::Column }, { "chart-line", "Line chart", ChartView::Line }, { "chart-area", "Area chart", ChartView::Area },
        { "chart-pie", "Pie chart", ChartView::Pie }, { "chart-donut", "Donut chart", ChartView::Donut } };
    for (const auto &k : kinds) {
        auto *b = new IconButton(QLatin1String(k.icon), tr(k.tip), this);
        b->setIconSize(18);
        m_typeButtons << b;
        const ChartView::Type t = k.type;
        connect(b, &IconButton::clicked, this, [this, t] { selectType(t, true); });
        bar->addWidget(b);
    }
    bar->addStretch(1);
    auto *exp = new IconButton(QStringLiteral("image-export"), tr("Export chart as PNG"), this);
    auto *close = new IconButton(QStringLiteral("close"), tr("Close chart panel"), this);
    connect(exp, &IconButton::clicked, this, &ChartPanel::exportRequested);
    connect(close, &IconButton::clicked, this, &ChartPanel::closeRequested);
    bar->addWidget(exp);
    bar->addWidget(close);
    m_view = new ChartView(this);
    col->addLayout(bar);
    col->addWidget(m_view, 1);
}

void ChartPanel::selectType(ChartView::Type t, bool emitSignal)
{
    m_view->setType(t);
    for (int i = 0; i < m_typeButtons.size(); ++i) m_typeButtons[i]->setChecked(i == int(t));
    if (emitSignal) emit typeChanged(t);
}

void ChartPanel::setChart(const ChartData &d, ChartView::Type type)
{
    m_view->setData(d, false);
    selectType(type, false);
}

void ChartPanel::paintEvent(QPaintEvent *)
{
    const Palette &pal = ThemeManager::instance().palette();
    QPainter p(this);
    p.fillRect(rect(), pal.surface);
    p.setPen(pal.border);
    p.drawLine(0, 0, 0, height());
}

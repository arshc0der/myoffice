#include "sheet.h"

#include <QDate>
#include <QLocale>
#include <QRegularExpression>
#include <algorithm>
#include <vector>

CellValue parseLiteral(const QString &input)
{
    const QString s = input.trimmed();
    if (s.isEmpty()) return CellValue();
    if (s.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0) return CellValue::boolean(true);
    if (s.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0) return CellValue::boolean(false);
    bool ok = false;
    const double d = s.toDouble(&ok);
    if (ok && !s.contains(QLatin1Char('x'), Qt::CaseInsensitive) && !s.startsWith(QLatin1Char('+'))) return CellValue::number(d);
    if (s.endsWith(QLatin1Char('%'))) { const double p = s.left(s.size() - 1).trimmed().toDouble(&ok); if (ok) return CellValue::number(p / 100.0); }
    static const QRegularExpression iso(QStringLiteral("^(\\d{4})-(\\d{2})-(\\d{2})$"));
    const auto m = iso.match(s);
    if (m.hasMatch()) {
        const QDate date(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
        if (date.isValid()) return CellValue::number(QDate(1899, 12, 30).daysTo(date), true);
    }
    return CellValue::str(input);
}

QString formatValue(const CellValue &v, const CellStyle &st)
{
    switch (v.type) {
    case CellValue::Empty: return QString();
    case CellValue::Text: case CellValue::Error: return v.text;
    case CellValue::Bool: return v.num != 0 ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    case CellValue::Number: break;
    }
    int f = st.numFmt;
    if (f == 0 && v.dateHint) f = 4;
    const QLocale c = QLocale::c();
    switch (f) {
    case 1: return c.toString(v.num, 'f', 2);
    case 2: return c.toString(v.num * 100.0, 'f', 2) + QLatin1Char('%');
    case 3: return (v.num < 0 ? QStringLiteral("-$") : QStringLiteral("$")) + c.toString(qAbs(v.num), 'f', 2);
    case 4: return QDate(1899, 12, 30).addDays(qint64(v.num)).toString(QStringLiteral("yyyy-MM-dd"));
    default: return Formula::formatGeneral(v.num);
    }
}

void autoFormat(Cell &cell)
{
    if (cell.style.numFmt != 0 || cell.input.startsWith(QLatin1Char('='))) return;
    const QString s = cell.input.trimmed();
    static const QRegularExpression pct(QStringLiteral("^-?\\d+(\\.\\d+)?%$")), iso(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    if (pct.match(s).hasMatch()) cell.style.numFmt = 2;
    else if (iso.match(s).hasMatch() && parseLiteral(s).type == CellValue::Number) cell.style.numFmt = 4;
}

const Cell *Sheet::find(int r, int c) const
{
    auto it = m_cells.constFind(key(r, c));
    return it == m_cells.constEnd() ? nullptr : &*it;
}
Cell Sheet::cell(int r, int c) const { const Cell *p = find(r, c); return p ? *p : Cell(); }

void Sheet::setCell(int r, int c, const Cell &cell)
{
    if (cell.isEmpty()) m_cells.remove(key(r, c)); else m_cells.insert(key(r, c), cell);
    m_cache.clear();
}
void Sheet::replaceAll(const QHash<quint64, Cell> &cells) { m_cells = cells; m_cache.clear(); }

CellValue Sheet::value(int r, int c) const
{
    const quint64 k = key(r, c);
    if (auto it = m_cache.constFind(k); it != m_cache.constEnd()) return *it;
    const Cell *cell = find(r, c);
    if (!cell || cell->input.isEmpty()) return CellValue();
    CellValue v;
    if (cell->input.startsWith(QLatin1Char('='))) {
        if (m_evaluating.contains(k) || m_depth > 2500) return CellValue::error(QStringLiteral("#CIRC!"));
        m_evaluating.insert(k);
        ++m_depth;
        v = Formula::evaluate(cell->input.mid(1), const_cast<Sheet &>(*this));
        --m_depth;
        m_evaluating.remove(k);
    } else {
        v = parseLiteral(cell->input);
    }
    m_cache.insert(k, v);
    return v;
}

QString Sheet::display(int r, int c) const
{
    const Cell *cell = find(r, c);
    if (!cell || cell->input.isEmpty()) return QString();
    if (cell->style.numFmt == 5 && !cell->input.startsWith(QLatin1Char('='))) return cell->input;
    return formatValue(value(r, c), cell->style);
}

QSize Sheet::usedExtent() const
{
    int rows = 0, cols = 0;
    for (auto it = m_cells.constBegin(); it != m_cells.constEnd(); ++it) { rows = qMax(rows, keyRow(it.key()) + 1); cols = qMax(cols, keyCol(it.key()) + 1); }
    return QSize(cols, rows);
}

void Sheet::rangeValues(int r1, int c1, int r2, int c2, std::vector<CellValue> &out)
{
    const qint64 area = qint64(r2 - r1 + 1) * (c2 - c1 + 1);
    if (area <= 4096 || area <= qint64(m_cells.size()) * 2) {
        for (int r = r1; r <= r2; ++r)
            for (int c = c1; c <= c2; ++c) if (find(r, c) && !find(r, c)->input.isEmpty()) out.push_back(value(r, c));
        return;
    }
    std::vector<quint64> keys; // sparse path: only walk the stored cells, in row-major order
    for (auto it = m_cells.constBegin(); it != m_cells.constEnd(); ++it) {
        const int r = keyRow(it.key()), c = keyCol(it.key());
        if (r >= r1 && r <= r2 && c >= c1 && c <= c2 && !it->input.isEmpty()) keys.push_back(it.key());
    }
    std::sort(keys.begin(), keys.end());
    for (quint64 k : keys) out.push_back(value(keyRow(k), keyCol(k)));
}

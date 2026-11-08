#pragma once
#include <QColor>
#include <QHash>
#include <QSet>
#include <QSize>

#include "myexcel/formula/formula.h"

struct CellStyle {
    bool bold = false, italic = false, underline = false;
    QColor fg, bg;     // invalid = default
    int align = 0;     // 0 auto, 1 left, 2 center, 3 right
    int numFmt = 0;    // 0 General, 1 Number, 2 Percent, 3 Currency, 4 Date, 5 Text
    bool operator==(const CellStyle &o) const
    {
        return bold == o.bold && italic == o.italic && underline == o.underline && fg == o.fg && bg == o.bg && align == o.align && numFmt == o.numFmt;
    }
    bool isDefault() const { return *this == CellStyle(); }
};

struct Cell {
    QString input; // raw text as typed: literal or "=formula"
    CellStyle style;
    bool isEmpty() const { return input.isEmpty() && style.isDefault(); }
};

CellValue parseLiteral(const QString &input);
QString formatValue(const CellValue &v, const CellStyle &style);
void autoFormat(Cell &cell); // sets Percent/Date format when the typed text looks like one

// Sparse worksheet (Excel limits: 1,048,576 rows x 16,384 columns). Formula results are cached and the
// cache is dropped on every change, so recalculation is automatic and always consistent.
class Sheet : public CellProvider
{
public:
    static constexpr int kMaxRows = 1048576, kMaxCols = 16384;
    static quint64 key(int r, int c) { return (quint64(r) << 14) | quint64(c); }
    static int keyRow(quint64 k) { return int(k >> 14); }
    static int keyCol(quint64 k) { return int(k & 0x3FFF); }

    const Cell *find(int r, int c) const;
    Cell cell(int r, int c) const;
    void setCell(int r, int c, const Cell &cell);
    const QHash<quint64, Cell> &cells() const { return m_cells; }
    void replaceAll(const QHash<quint64, Cell> &cells);
    CellValue value(int r, int c) const;
    QString display(int r, int c) const;
    QSize usedExtent() const; // width = columns, height = rows

    CellValue cellValue(int r, int c) override { return value(r, c); }
    void rangeValues(int r1, int c1, int r2, int c2, std::vector<CellValue> &out) override;

private:
    QHash<quint64, Cell> m_cells;
    mutable QHash<quint64, CellValue> m_cache;
    mutable QSet<quint64> m_evaluating;
    mutable int m_depth = 0;
};

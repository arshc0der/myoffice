#pragma once
#include <QString>
#include <vector>

// Result of evaluating a cell.
struct CellValue {
    enum Type { Empty, Number, Text, Bool, Error };
    Type type = Empty;
    double num = 0;
    QString text;          // Text value or error code
    bool dateHint = false; // e.g. TODAY(): display as a date when the format is General

    static CellValue number(double n, bool date = false) { CellValue v; v.type = Number; v.num = n; v.dateHint = date; return v; }
    static CellValue str(const QString &s) { CellValue v; v.type = Text; v.text = s; return v; }
    static CellValue boolean(bool b) { CellValue v; v.type = Bool; v.num = b ? 1 : 0; return v; }
    static CellValue error(const QString &e) { CellValue v; v.type = Error; v.text = e; return v; }
    bool isError() const { return type == Error; }
    bool toNumber(double *out) const;
    QString toText() const;
    bool toBool(bool *ok) const;
};

// Supplies cell values to the evaluator (implemented by Sheet).
class CellProvider
{
public:
    virtual ~CellProvider() = default;
    virtual CellValue cellValue(int row, int col) = 0;
    // Appends the non-empty values of a rectangular range in row-major order.
    virtual void rangeValues(int r1, int c1, int r2, int c2, std::vector<CellValue> &out) = 0;
};

namespace Formula {
// Evaluates an expression without the leading '='. Supports + - * / ^ & % comparisons, cell refs,
// ranges and: SUM AVERAGE MIN MAX COUNT COUNTA IF AND OR NOT ROUND ABS SQRT POWER MOD INT CONCAT
// CONCATENATE LEFT RIGHT LEN UPPER LOWER TRIM TODAY NOW.
CellValue evaluate(const QString &expr, CellProvider &provider);
bool parseRef(const QString &s, int *row, int *col);
QString colName(int col);
QString refName(int row, int col);
QString formatGeneral(double n);
// Reference rewriting used by insert/delete rows/columns and fill/paste.
QString shiftReferences(const QString &formula, bool isRow, int at, int delta);
QString translateReferences(const QString &formula, int dRow, int dCol);
}

#pragma once
#include <QByteArray>
#include <QList>
#include <QString>

#include "myexcel/spreadsheet/sheet.h"

// XLSX reader/writer on Qt's zip support. Supports multiple sheets, values, formulas (with cached results),
// fonts (bold/italic/underline/colour), solid fills, alignment and number formats (number, percent, currency, date, text).
// Not yet: shared formulas, column widths, merged cells, charts, conditional formats, theme colours.
namespace XlsxIO {
struct SheetData { QString name; QHash<quint64, Cell> cells; };
QByteArray write(const QStringList &names, const QList<const Sheet *> &sheets);
bool read(const QByteArray &zipData, QList<SheetData> &out, QString *error);
}

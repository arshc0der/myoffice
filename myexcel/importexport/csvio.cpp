#include "csvio.h"

#include <QStringConverter>
#include <QObject>
#include "myexcel/spreadsheet/sheet.h"

namespace CsvIO {

QChar detectDelimiter(const QString &text)
{
    const QString head = text.left(text.indexOf(QLatin1Char('\n')) < 0 ? 4096 : qMin<qsizetype>(text.indexOf(QLatin1Char('\n')), 4096));
    const int comma = head.count(QLatin1Char(',')), semi = head.count(QLatin1Char(';')), tab = head.count(QLatin1Char('\t'));
    if (tab > comma && tab >= semi) return QLatin1Char('\t');
    return semi > comma ? QLatin1Char(';') : QLatin1Char(',');
}

std::vector<std::vector<QString>> parse(const QString &text, QChar delim)
{
    std::vector<std::vector<QString>> rows;
    std::vector<QString> row;
    QString field;
    bool inQuotes = false, any = false;
    const int n = text.size();
    for (int i = 0; i < n; ++i) {
        const QChar ch = text[i];
        if (inQuotes) {
            if (ch == QLatin1Char('"')) { if (i + 1 < n && text[i + 1] == QLatin1Char('"')) { field += ch; ++i; } else inQuotes = false; }
            else field += ch;
        } else if (ch == QLatin1Char('"') && field.isEmpty()) { inQuotes = true; any = true; }
        else if (ch == delim) { row.push_back(field); field.clear(); any = true; }
        else if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            if (ch == QLatin1Char('\r') && i + 1 < n && text[i + 1] == QLatin1Char('\n')) ++i;
            row.push_back(field); field.clear();
            rows.push_back(std::move(row)); row.clear(); any = false;
        } else { field += ch; any = true; }
    }
    if (any || !field.isEmpty()) { row.push_back(field); rows.push_back(std::move(row)); }
    return rows;
}

bool read(const QByteArray &data, Sheet &sheet, QString *error)
{
    QString text;
    if (auto enc = QStringConverter::encodingForData(data)) { QStringDecoder dec(*enc); text = dec(data); }
    else { QStringDecoder utf8(QStringConverter::Utf8); text = utf8(data); if (utf8.hasError()) text = QString::fromLocal8Bit(data); }
    const auto rows = parse(text, detectDelimiter(text));
    if (rows.size() > size_t(Sheet::kMaxRows)) { if (error) *error = QObject::tr("The file has more rows than a sheet can hold."); return false; }
    QHash<quint64, Cell> cells;
    for (size_t r = 0; r < rows.size(); ++r)
        for (size_t c = 0; c < rows[r].size() && c < size_t(Sheet::kMaxCols); ++c) {
            if (rows[r][c].isEmpty()) continue;
            Cell cell;
            cell.input = rows[r][c];
            autoFormat(cell);
            cells.insert(Sheet::key(int(r), int(c)), cell);
        }
    sheet.replaceAll(cells);
    return true;
}

QByteArray write(const Sheet &sheet)
{
    const QSize ext = sheet.usedExtent();
    QString out;
    for (int r = 0; r < ext.height(); ++r) {
        for (int c = 0; c < ext.width(); ++c) {
            if (c) out += QLatin1Char(',');
            QString v = sheet.display(r, c);
            if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"')) || v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r'))) {
                v.replace(QLatin1Char('"'), QStringLiteral("\"\""));
                v = QLatin1Char('"') + v + QLatin1Char('"');
            }
            out += v;
        }
        out += QLatin1String("\r\n");
    }
    return QByteArray("\xEF\xBB\xBF") + out.toUtf8(); // UTF-8 with BOM so Excel detects the encoding
}

}

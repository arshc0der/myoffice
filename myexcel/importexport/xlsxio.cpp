#include "xlsxio.h"

#include <QBuffer>
#include <QXmlStreamReader>
#include <algorithm>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

namespace {
QString esc(const QString &s)
{
    QString out;
    for (QChar c : s) { const ushort u = c.unicode(); if (u < 0x20 && u != 9 && u != 10 && u != 13) continue; out += c; }
    return out.toHtmlEscaped();
}
QString argb(const QColor &c) { return QStringLiteral("FF") + c.name(QColor::HexRgb).mid(1).toUpper(); }
const char *kHeader = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
}

namespace XlsxIO {

QByteArray write(const QStringList &names, const QList<const Sheet *> &sheets)
{
    QList<CellStyle> styles{ CellStyle() };
    auto styleIndex = [&](const CellStyle &s) { int i = styles.indexOf(s); if (i < 0) { styles.append(s); i = styles.size() - 1; } return i; };

    QList<QString> sheetXml;
    for (const Sheet *sheet : sheets) {
        QList<quint64> keys = sheet->cells().keys();
        std::sort(keys.begin(), keys.end());
        QString x = QString::fromLatin1(kHeader) + QStringLiteral("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>");
        int curRow = -1;
        for (quint64 k : keys) {
            const int r = Sheet::keyRow(k), c = Sheet::keyCol(k);
            const Cell &cell = sheet->cells().value(k);
            if (r != curRow) { if (curRow >= 0) x += QStringLiteral("</row>"); x += QStringLiteral("<row r=\"%1\">").arg(r + 1); curRow = r; }
            const QString ref = Formula::refName(r, c);
            const int s = styleIndex(cell.style);
            const QString head = QStringLiteral("<c r=\"%1\" s=\"%2\"").arg(ref).arg(s);
            if (cell.input.isEmpty()) { x += head + QStringLiteral("/>"); continue; }
            if (cell.input.startsWith(QLatin1Char('='))) {
                const CellValue v = sheet->value(r, c);
                QString t, val;
                if (v.type == CellValue::Number) val = QString::number(v.num, 'g', 15);
                else if (v.type == CellValue::Bool) { t = QStringLiteral(" t=\"b\""); val = v.num != 0 ? QStringLiteral("1") : QStringLiteral("0"); }
                else if (v.type == CellValue::Error) { t = QStringLiteral(" t=\"e\""); val = v.text; }
                else { t = QStringLiteral(" t=\"str\""); val = v.toText(); }
                x += head + t + QStringLiteral("><f>") + esc(cell.input.mid(1)) + QStringLiteral("</f><v>") + esc(val) + QStringLiteral("</v></c>");
                continue;
            }
            const CellValue v = parseLiteral(cell.input);
            if (v.type == CellValue::Number) x += head + QStringLiteral("><v>") + QString::number(v.num, 'g', 15) + QStringLiteral("</v></c>");
            else if (v.type == CellValue::Bool) x += head + QStringLiteral(" t=\"b\"><v>") + (v.num != 0 ? QStringLiteral("1") : QStringLiteral("0")) + QStringLiteral("</v></c>");
            else x += head + QStringLiteral(" t=\"inlineStr\"><is><t xml:space=\"preserve\">") + esc(cell.input) + QStringLiteral("</t></is></c>");
        }
        if (curRow >= 0) x += QStringLiteral("</row>");
        x += QStringLiteral("</sheetData></worksheet>");
        sheetXml << x;
    }

    // styles.xml
    QStringList fonts{ QStringLiteral("<font><sz val=\"11\"/><name val=\"Calibri\"/></font>") };
    QStringList fills{ QStringLiteral("<fill><patternFill patternType=\"none\"/></fill>"), QStringLiteral("<fill><patternFill patternType=\"gray125\"/></fill>") };
    QString xfs;
    for (const CellStyle &s : styles) {
        QString f = QStringLiteral("<font>") + (s.bold ? QStringLiteral("<b/>") : QString()) + (s.italic ? QStringLiteral("<i/>") : QString())
                    + (s.underline ? QStringLiteral("<u/>") : QString()) + QStringLiteral("<sz val=\"11\"/>")
                    + (s.fg.isValid() ? QStringLiteral("<color rgb=\"") + argb(s.fg) + QStringLiteral("\"/>") : QString()) + QStringLiteral("<name val=\"Calibri\"/></font>");
        int fi = fonts.indexOf(f); if (fi < 0) { fonts << f; fi = fonts.size() - 1; }
        int fl = 0;
        if (s.bg.isValid()) {
            const QString fill = QStringLiteral("<fill><patternFill patternType=\"solid\"><fgColor rgb=\"") + argb(s.bg) + QStringLiteral("\"/><bgColor indexed=\"64\"/></patternFill></fill>");
            fl = fills.indexOf(fill); if (fl < 0) { fills << fill; fl = fills.size() - 1; }
        }
        static const int fmtIds[6] = { 0, 4, 10, 165, 164, 49 };
        xfs += QStringLiteral("<xf numFmtId=\"%1\" fontId=\"%2\" fillId=\"%3\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\" applyFont=\"1\" applyFill=\"1\" applyAlignment=\"1\">").arg(fmtIds[qBound(0, s.numFmt, 5)]).arg(fi).arg(fl);
        if (s.align) xfs += QStringLiteral("<alignment horizontal=\"%1\"/>").arg(s.align == 1 ? QStringLiteral("left") : s.align == 2 ? QStringLiteral("center") : QStringLiteral("right"));
        xfs += QStringLiteral("</xf>");
    }
    const QString stylesXml = QString::fromLatin1(kHeader)
        + QStringLiteral("<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><numFmts count=\"2\"><numFmt numFmtId=\"164\" formatCode=\"yyyy\\-mm\\-dd\"/><numFmt numFmtId=\"165\" formatCode=\"&quot;$&quot;#,##0.00\"/></numFmts>")
        + QStringLiteral("<fonts count=\"%1\">%2</fonts><fills count=\"%3\">%4</fills>").arg(fonts.size()).arg(fonts.join(QString())).arg(fills.size()).arg(fills.join(QString()))
        + QStringLiteral("<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>")
        + QStringLiteral("<cellXfs count=\"%1\">%2</cellXfs></styleSheet>").arg(styles.size()).arg(xfs);

    QString wbSheets, wbRels, ctSheets;
    for (int i = 0; i < names.size(); ++i) {
        wbSheets += QStringLiteral("<sheet name=\"%1\" sheetId=\"%2\" r:id=\"rId%2\"/>").arg(esc(names[i])).arg(i + 1);
        wbRels += QStringLiteral("<Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%1.xml\"/>").arg(i + 1);
        ctSheets += QStringLiteral("<Override PartName=\"/xl/worksheets/sheet%1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>").arg(i + 1);
    }
    const int styleRid = names.size() + 1;
    wbRels += QStringLiteral("<Relationship Id=\"rId%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>").arg(styleRid);

    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&buf);
        zip.setCompressionPolicy(QZipWriter::AutoCompress);
        const QString h = QString::fromLatin1(kHeader);
        zip.addFile(QStringLiteral("[Content_Types].xml"), (h + QStringLiteral("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>") + ctSheets + QStringLiteral("</Types>")).toUtf8());
        zip.addFile(QStringLiteral("_rels/.rels"), (h + QStringLiteral("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>")).toUtf8());
        zip.addFile(QStringLiteral("xl/workbook.xml"), (h + QStringLiteral("<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>") + wbSheets + QStringLiteral("</sheets></workbook>")).toUtf8());
        zip.addFile(QStringLiteral("xl/_rels/workbook.xml.rels"), (h + QStringLiteral("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">") + wbRels + QStringLiteral("</Relationships>")).toUtf8());
        zip.addFile(QStringLiteral("xl/styles.xml"), stylesXml.toUtf8());
        for (int i = 0; i < sheetXml.size(); ++i) zip.addFile(QStringLiteral("xl/worksheets/sheet%1.xml").arg(i + 1), sheetXml[i].toUtf8());
        zip.close();
        if (zip.status() != QZipWriter::NoError) return {};
    }
    return out;
}

// ------------------------------------------------------------------ read
namespace {
struct Xf { int numFmt = 0, font = 0, fill = 0, align = 0; };
struct FontInfo { bool b = false, i = false, u = false; QColor color; };

int mapNumFmt(int id, const QString &code)
{
    if (id == 9 || id == 10 || code.contains(QLatin1Char('%'))) return 2;
    if (id == 49) return 5;
    if ((id >= 14 && id <= 22) || (id >= 45 && id <= 47) || (code.contains(QLatin1Char('y'), Qt::CaseInsensitive) && code.contains(QLatin1Char('m'), Qt::CaseInsensitive))) return 4;
    if (code.contains(QLatin1Char('$'))) return 3;
    if (id >= 1 && id <= 4) return 1;
    if (code.contains(QLatin1String("0.0"))) return 1;
    return 0;
}

QColor rgbOf(QXmlStreamReader &x)
{
    const QString v = x.attributes().value(QStringLiteral("rgb")).toString();
    return v.size() == 8 ? QColor(QLatin1Char('#') + v.mid(2)) : QColor();
}
}

bool read(const QByteArray &data, QList<SheetData> &out, QString *error)
{
    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    QZipReader zip(&buf);
    const QByteArray wbXml = zip.fileData(QStringLiteral("xl/workbook.xml"));
    if (!zip.isReadable() || wbXml.isEmpty()) { if (error) *error = QObject::tr("Not a valid XLSX file."); return false; }

    QHash<QString, QString> rels;
    {
        QXmlStreamReader r(zip.fileData(QStringLiteral("xl/_rels/workbook.xml.rels")));
        while (!r.atEnd()) { r.readNext(); if (r.isStartElement() && r.name() == QLatin1String("Relationship")) rels.insert(r.attributes().value(QStringLiteral("Id")).toString(), r.attributes().value(QStringLiteral("Target")).toString()); }
    }
    QList<QPair<QString, QString>> sheetRefs; // name, target
    {
        QXmlStreamReader r(wbXml);
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement() && r.name() == QLatin1String("sheet")) {
                const QString rid = r.attributes().value(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"), QStringLiteral("id")).toString();
                sheetRefs << qMakePair(r.attributes().value(QStringLiteral("name")).toString(), rels.value(rid));
            }
        }
    }
    QStringList shared;
    {
        QXmlStreamReader r(zip.fileData(QStringLiteral("xl/sharedStrings.xml")));
        QString cur;
        bool inSi = false;
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement() && r.name() == QLatin1String("si")) { inSi = true; cur.clear(); }
            else if (r.isStartElement() && r.name() == QLatin1String("t") && inSi) cur += r.readElementText();
            else if (r.isEndElement() && r.name() == QLatin1String("si")) { shared << cur; inSi = false; }
        }
    }
    QHash<int, QString> numFmts;
    QList<FontInfo> fonts;
    QList<QColor> fills;
    QList<Xf> xfs;
    {
        QXmlStreamReader r(zip.fileData(QStringLiteral("xl/styles.xml")));
        QString section;
        bool inCellXfs = false;
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement()) {
                const QStringView n = r.name();
                if (n == QLatin1String("numFmt")) numFmts.insert(r.attributes().value(QStringLiteral("numFmtId")).toInt(), r.attributes().value(QStringLiteral("formatCode")).toString());
                else if (n == QLatin1String("fonts")) section = QStringLiteral("fonts");
                else if (n == QLatin1String("fills")) section = QStringLiteral("fills");
                else if (n == QLatin1String("cellXfs")) { section = QStringLiteral("xfs"); inCellXfs = true; }
                else if (n == QLatin1String("font") && section == QLatin1String("fonts")) fonts << FontInfo();
                else if (section == QLatin1String("fonts") && !fonts.isEmpty()) {
                    if (n == QLatin1String("b")) fonts.last().b = true;
                    else if (n == QLatin1String("i")) fonts.last().i = true;
                    else if (n == QLatin1String("u")) fonts.last().u = true;
                    else if (n == QLatin1String("color")) fonts.last().color = rgbOf(r);
                } else if (n == QLatin1String("fill") && section == QLatin1String("fills")) fills << QColor();
                else if (n == QLatin1String("fgColor") && section == QLatin1String("fills") && !fills.isEmpty()) fills.last() = rgbOf(r);
                else if (n == QLatin1String("xf") && inCellXfs) {
                    Xf x; x.numFmt = r.attributes().value(QStringLiteral("numFmtId")).toInt(); x.font = r.attributes().value(QStringLiteral("fontId")).toInt(); x.fill = r.attributes().value(QStringLiteral("fillId")).toInt();
                    xfs << x;
                } else if (n == QLatin1String("alignment") && inCellXfs && !xfs.isEmpty()) {
                    const QString h = r.attributes().value(QStringLiteral("horizontal")).toString();
                    xfs.last().align = h == QLatin1String("left") ? 1 : h == QLatin1String("center") ? 2 : h == QLatin1String("right") ? 3 : 0;
                }
            } else if (r.isEndElement() && r.name() == QLatin1String("cellXfs")) inCellXfs = false;
        }
    }
    auto styleOf = [&](int idx) {
        CellStyle s;
        if (idx < 0 || idx >= xfs.size()) return s;
        const Xf &x = xfs[idx];
        if (x.font >= 0 && x.font < fonts.size()) { s.bold = fonts[x.font].b; s.italic = fonts[x.font].i; s.underline = fonts[x.font].u; s.fg = fonts[x.font].color; }
        if (x.fill >= 2 && x.fill < fills.size()) s.bg = fills[x.fill];
        s.align = x.align;
        s.numFmt = mapNumFmt(x.numFmt, numFmts.value(x.numFmt));
        return s;
    };

    for (const auto &ref : sheetRefs) {
        QString target = ref.second;
        target = target.startsWith(QLatin1Char('/')) ? target.mid(1) : QStringLiteral("xl/") + target;
        const QByteArray xml = zip.fileData(target);
        SheetData sd;
        sd.name = ref.first;
        QXmlStreamReader r(xml);
        int row = -1, col = -1, style = 0;
        QString type, value, formula, inlineText;
        bool inCell = false;
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement()) {
                const QStringView n = r.name();
                if (n == QLatin1String("c")) {
                    inCell = true; type = r.attributes().value(QStringLiteral("t")).toString(); style = r.attributes().value(QStringLiteral("s")).toInt();
                    value.clear(); formula.clear(); inlineText.clear();
                    Formula::parseRef(r.attributes().value(QStringLiteral("r")).toString(), &row, &col);
                } else if (inCell && n == QLatin1String("v")) value = r.readElementText();
                else if (inCell && n == QLatin1String("f")) formula = r.readElementText();
                else if (inCell && n == QLatin1String("t")) inlineText += r.readElementText();
            } else if (r.isEndElement() && r.name() == QLatin1String("c") && inCell) {
                inCell = false;
                Cell cell;
                cell.style = styleOf(style);
                if (!formula.isEmpty()) cell.input = QLatin1Char('=') + formula;
                else if (type == QLatin1String("s")) cell.input = shared.value(value.toInt());
                else if (type == QLatin1String("inlineStr")) cell.input = inlineText;
                else if (type == QLatin1String("b")) cell.input = value == QLatin1String("1") ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
                else cell.input = value;
                if (row >= 0 && col >= 0 && !cell.isEmpty()) sd.cells.insert(Sheet::key(row, col), cell);
            }
        }
        out << sd;
    }
    if (out.isEmpty()) { if (error) *error = QObject::tr("The workbook contains no sheets."); return false; }
    return true;
}

}

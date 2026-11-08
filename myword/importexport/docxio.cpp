#include "docxio.h"

#include <QBuffer>
#include <QHash>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTextTable>
#include <QXmlStreamReader>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <vector>

namespace {
const QString kW = QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main");
const QString kRel = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");

QString esc(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        const ushort u = c.unicode();
        if (u < 0x20 && u != 9 && u != 10 && u != 13) continue; // illegal in XML 1.0
        out += c;
    }
    return out.toHtmlEscaped();
}
QString tw(double px) { return QString::number(qRound(px * 15.0)); } // 96dpi px -> twips
QString hex(const QColor &c) { return c.name(QColor::HexRgb).mid(1).toUpper(); }
double mmToTw(double mm) { return mm * 56.6929; }

// ------------------------------------------------------------------ writer
QString runXml(const QTextCharFormat &f, QString text)
{
    text.remove(QChar::ObjectReplacementCharacter);
    if (text.isEmpty()) return {};
    QString rpr;
    QString family = f.hasProperty(QTextFormat::FontFamilies) ? f.fontFamilies().toStringList().value(0)
                                                              : f.stringProperty(QTextFormat::FontFamily);
    if (!family.isEmpty())
        rpr += QStringLiteral("<w:rFonts w:ascii=\"%1\" w:hAnsi=\"%1\" w:cs=\"%1\"/>").arg(esc(family));
    if (f.fontWeight() >= static_cast<int>(QFont::Bold)) rpr += QStringLiteral("<w:b/>");
    if (f.fontItalic()) rpr += QStringLiteral("<w:i/>");
    if (f.fontStrikeOut()) rpr += QStringLiteral("<w:strike/>");
    if (f.hasProperty(QTextFormat::ForegroundBrush)) rpr += QStringLiteral("<w:color w:val=\"%1\"/>").arg(hex(f.foreground().color()));
    if (f.hasProperty(QTextFormat::FontPointSize)) rpr += QStringLiteral("<w:sz w:val=\"%1\"/>").arg(qRound(f.fontPointSize() * 2));
    if (f.fontUnderline()) rpr += QStringLiteral("<w:u w:val=\"single\"/>");
    if (f.hasProperty(QTextFormat::BackgroundBrush) && f.background().style() != Qt::NoBrush)
        rpr += QStringLiteral("<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"%1\"/>").arg(hex(f.background().color()));

    QString body, buf;
    auto flush = [&] { if (!buf.isEmpty()) { body += QStringLiteral("<w:t xml:space=\"preserve\">") + esc(buf) + QStringLiteral("</w:t>"); buf.clear(); } };
    for (QChar ch : text) {
        if (ch == QLatin1Char('\t')) { flush(); body += QStringLiteral("<w:tab/>"); }
        else if (ch == QChar::LineSeparator || ch == QLatin1Char('\n')) { flush(); body += QStringLiteral("<w:br/>"); }
        else buf += ch;
    }
    flush();
    return QStringLiteral("<w:r>") + (rpr.isEmpty() ? QString() : QStringLiteral("<w:rPr>") + rpr + QStringLiteral("</w:rPr>")) + body + QStringLiteral("</w:r>");
}

QString paraXml(const QTextBlock &b)
{
    const QTextBlockFormat bf = b.blockFormat();
    QString ppr;
    const int h = bf.headingLevel();
    if (h >= 1 && h <= 3) ppr += QStringLiteral("<w:pStyle w:val=\"Heading%1\"/>").arg(h);
    if (bf.pageBreakPolicy().testFlag(QTextFormat::PageBreak_AlwaysBefore)) ppr += QStringLiteral("<w:pageBreakBefore/>");
    QTextList *list = b.textList();
    if (list) {
        const auto st = list->format().style();
        const bool numbered = st == QTextListFormat::ListDecimal || st == QTextListFormat::ListLowerAlpha || st == QTextListFormat::ListUpperAlpha
                              || st == QTextListFormat::ListLowerRoman || st == QTextListFormat::ListUpperRoman;
        ppr += QStringLiteral("<w:numPr><w:ilvl w:val=\"%1\"/><w:numId w:val=\"%2\"/></w:numPr>")
                   .arg(qBound(0, list->format().indent() - 1, 8)).arg(numbered ? 2 : 1);
    }
    QString sp;
    if (bf.topMargin() > 0) sp += QStringLiteral(" w:before=\"%1\"").arg(tw(bf.topMargin()));
    if (bf.bottomMargin() > 0) sp += QStringLiteral(" w:after=\"%1\"").arg(tw(bf.bottomMargin()));
    if (bf.lineHeightType() == QTextBlockFormat::ProportionalHeight && bf.lineHeight() > 0)
        sp += QStringLiteral(" w:line=\"%1\" w:lineRule=\"auto\"").arg(qRound(bf.lineHeight() / 100.0 * 240));
    if (!sp.isEmpty()) ppr += QStringLiteral("<w:spacing%1/>").arg(sp);
    const qreal left = bf.leftMargin() + (list ? 0 : bf.indent() * 40);
    if (left > 0 || bf.textIndent() > 0) {
        QString ind;
        if (left > 0) ind += QStringLiteral(" w:left=\"%1\"").arg(tw(left));
        if (bf.textIndent() > 0) ind += QStringLiteral(" w:firstLine=\"%1\"").arg(tw(bf.textIndent()));
        ppr += QStringLiteral("<w:ind%1/>").arg(ind);
    }
    const Qt::Alignment a = bf.alignment();
    if (a & Qt::AlignHCenter) ppr += QStringLiteral("<w:jc w:val=\"center\"/>");
    else if (a & Qt::AlignRight) ppr += QStringLiteral("<w:jc w:val=\"right\"/>");
    else if (a & Qt::AlignJustify) ppr += QStringLiteral("<w:jc w:val=\"both\"/>");

    QString runs;
    for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it) {
        const QTextFragment fr = it.fragment();
        if (fr.isValid()) runs += runXml(fr.charFormat(), fr.text());
    }
    return QStringLiteral("<w:p>") + (ppr.isEmpty() ? QString() : QStringLiteral("<w:pPr>") + ppr + QStringLiteral("</w:pPr>")) + runs + QStringLiteral("</w:p>");
}

void writeFrame(QTextFrame *f, QString &out);

void writeTable(QTextTable *t, QString &out)
{
    out += QStringLiteral("<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>");
    for (const char *e : { "top", "left", "bottom", "right", "insideH", "insideV" })
        out += QStringLiteral("<w:%1 w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"808080\"/>").arg(QLatin1String(e));
    out += QStringLiteral("</w:tblBorders></w:tblPr><w:tblGrid>");
    for (int c = 0; c < t->columns(); ++c) out += QStringLiteral("<w:gridCol w:w=\"2000\"/>");
    out += QStringLiteral("</w:tblGrid>");
    for (int r = 0; r < t->rows(); ++r) {
        out += QStringLiteral("<w:tr>");
        for (int c = 0; c < t->columns(); ++c) {
            const QTextTableCell cell = t->cellAt(r, c);
            if (cell.column() != c) continue; // covered by a column span
            QString pr;
            if (cell.columnSpan() > 1) pr += QStringLiteral("<w:gridSpan w:val=\"%1\"/>").arg(cell.columnSpan());
            if (cell.row() != r) pr += QStringLiteral("<w:vMerge/>");
            else if (cell.rowSpan() > 1) pr += QStringLiteral("<w:vMerge w:val=\"restart\"/>");
            out += QStringLiteral("<w:tc><w:tcPr>") + pr + QStringLiteral("</w:tcPr>");
            QString inner;
            if (cell.row() == r)
                for (QTextFrame::iterator it = cell.begin(); !it.atEnd(); ++it)
                    if (it.currentBlock().isValid()) inner += paraXml(it.currentBlock());
            if (inner.isEmpty()) inner = QStringLiteral("<w:p/>");
            out += inner + QStringLiteral("</w:tc>");
        }
        out += QStringLiteral("</w:tr>");
    }
    out += QStringLiteral("</w:tbl>");
}

void writeFrame(QTextFrame *f, QString &out)
{
    for (QTextFrame::iterator it = f->begin(); !it.atEnd(); ++it) {
        if (QTextFrame *child = it.currentFrame()) {
            if (auto *t = qobject_cast<QTextTable *>(child)) writeTable(t, out);
        } else if (it.currentBlock().isValid()) {
            out += paraXml(it.currentBlock());
        }
    }
}

QString numberingXml()
{
    QString x = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:numbering xmlns:w=\"%1\">").arg(kW);
    for (int a = 0; a < 2; ++a) {
        x += QStringLiteral("<w:abstractNum w:abstractNumId=\"%1\"><w:multiLevelType w:val=\"hybridMultilevel\"/>").arg(a);
        for (int i = 0; i < 9; ++i) {
            const QString fmt = a == 0 ? QStringLiteral("bullet") : QStringLiteral("decimal");
            const QString txt = a == 0 ? QStringLiteral("\u2022") : QStringLiteral("%") + QString::number(i + 1) + QStringLiteral(".");
            x += QStringLiteral("<w:lvl w:ilvl=\"") + QString::number(i) + QStringLiteral("\"><w:start w:val=\"1\"/><w:numFmt w:val=\"") + fmt
                 + QStringLiteral("\"/><w:lvlText w:val=\"") + txt + QStringLiteral("\"/><w:lvlJc w:val=\"left\"/><w:pPr><w:ind w:left=\"")
                 + QString::number(720 + 360 * i) + QStringLiteral("\" w:hanging=\"360\"/></w:pPr></w:lvl>");
        }
        x += QStringLiteral("</w:abstractNum>");
    }
    x += QStringLiteral("<w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num><w:num w:numId=\"2\"><w:abstractNumId w:val=\"1\"/></w:num></w:numbering>");
    return x;
}

QString stylesXml()
{
    QString x = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:styles xmlns:w=\"%1\"><w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/></w:style>").arg(kW);
    const int sizes[3] = { 40, 32, 26 };
    for (int i = 1; i <= 3; ++i)
        x += QStringLiteral("<w:style w:type=\"paragraph\" w:styleId=\"Heading%1\"><w:name w:val=\"heading %1\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:keepNext/><w:outlineLvl w:val=\"%2\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"%3\"/></w:rPr></w:style>")
                 .arg(i).arg(i - 1).arg(sizes[i - 1]);
    return x + QStringLiteral("</w:styles>");
}

// ------------------------------------------------------------------ reader model
struct RunData { QString text; QTextCharFormat fmt; bool pageBreak = false; };
struct ParaData {
    QTextBlockFormat bf;
    std::vector<RunData> runs;
    int listKind = 0; // 0 none, 1 bullet, 2 numbered
    int level = 0, heading = 0;
    bool pageBreakBefore = false;
};
struct CellData { std::vector<ParaData> paras; int span = 1; int vmerge = 0; }; // vmerge: 1 restart, 2 continue
struct TableData { std::vector<std::vector<CellData>> rows; };
struct Item { bool isTable = false; ParaData para; TableData table; };

QString wval(QXmlStreamReader &x, const char *name = "val")
{
    return x.attributes().value(kW, QString::fromLatin1(name)).toString();
}
bool onOff(const QString &v) { return !(v == QLatin1String("0") || v == QLatin1String("false") || v == QLatin1String("off")); }

class Parser
{
public:
    QHash<QString, QString> links;
    QHash<int, int> numToAbs;
    QHash<int, bool> absBullet;
    std::vector<Item> items;
    PageSetup page;
    bool havePage = false;

    void parseBody(QXmlStreamReader &x)
    {
        while (x.readNextStartElement()) {
            const QStringView n = x.name();
            if (n == QLatin1String("p")) { Item it; it.para = parsePara(x); items.push_back(std::move(it)); }
            else if (n == QLatin1String("tbl")) { Item it; it.isTable = true; it.table = parseTable(x); items.push_back(std::move(it)); }
            else if (n == QLatin1String("sectPr")) parseSect(x);
            else x.skipCurrentElement();
        }
    }

private:
    void parseSect(QXmlStreamReader &x)
    {
        double w = 0, h = 0, t = 0, r = 0, b = 0, l = 0;
        bool land = false;
        while (x.readNextStartElement()) {
            if (x.name() == QLatin1String("pgSz")) { w = wval(x, "w").toDouble(); h = wval(x, "h").toDouble(); land = wval(x, "orient") == QLatin1String("landscape"); }
            else if (x.name() == QLatin1String("pgMar")) { t = wval(x, "top").toDouble(); r = wval(x, "right").toDouble(); b = wval(x, "bottom").toDouble(); l = wval(x, "left").toDouble(); }
            x.skipCurrentElement();
        }
        if (w > 0 && h > 0) {
            page.widthMm = qMin(w, h) / 56.6929;
            page.heightMm = qMax(w, h) / 56.6929;
            page.landscape = land || w > h;
            page.marginTopMm = t / 56.6929; page.marginRightMm = r / 56.6929;
            page.marginBottomMm = b / 56.6929; page.marginLeftMm = l / 56.6929;
            havePage = true;
        }
    }

    void parseRPr(QXmlStreamReader &x, QTextCharFormat &f)
    {
        while (x.readNextStartElement()) {
            const QStringView n = x.name();
            const QString v = wval(x);
            if (n == QLatin1String("b")) f.setFontWeight(onOff(v) ? QFont::Bold : QFont::Normal);
            else if (n == QLatin1String("i")) f.setFontItalic(onOff(v));
            else if (n == QLatin1String("strike")) f.setFontStrikeOut(onOff(v));
            else if (n == QLatin1String("u")) f.setFontUnderline(v != QLatin1String("none"));
            else if (n == QLatin1String("rFonts")) { QString fam = wval(x, "ascii"); if (fam.isEmpty()) fam = wval(x, "hAnsi"); if (!fam.isEmpty()) f.setFontFamilies({ fam }); }
            else if (n == QLatin1String("sz")) { const int hp = v.toInt(); if (hp > 0) f.setFontPointSize(hp / 2.0); }
            else if (n == QLatin1String("color")) { if (v != QLatin1String("auto") && v.size() == 6) f.setForeground(QColor(QLatin1Char('#') + v)); }
            else if (n == QLatin1String("highlight")) {
                static const QHash<QString, QString> map = { { "yellow", "#FFFF00" }, { "green", "#00FF00" }, { "cyan", "#00FFFF" }, { "magenta", "#FF00FF" },
                    { "red", "#FF0000" }, { "blue", "#0000FF" }, { "lightGray", "#D3D3D3" }, { "darkYellow", "#808000" } };
                if (map.contains(v)) f.setBackground(QColor(map.value(v)));
            } else if (n == QLatin1String("shd")) { const QString fill = wval(x, "fill"); if (fill.size() == 6) f.setBackground(QColor(QLatin1Char('#') + fill)); }
            x.skipCurrentElement();
        }
    }

    void parseRun(QXmlStreamReader &x, ParaData &p, const QString &href)
    {
        QTextCharFormat fmt;
        QString text;
        bool pb = false;
        while (x.readNextStartElement()) {
            const QStringView n = x.name();
            if (n == QLatin1String("rPr")) parseRPr(x, fmt);
            else if (n == QLatin1String("t")) text += x.readElementText();
            else if (n == QLatin1String("tab")) { text += QLatin1Char('\t'); x.skipCurrentElement(); }
            else if (n == QLatin1String("br")) { if (wval(x, "type") == QLatin1String("page")) pb = true; else text += QChar::LineSeparator; x.skipCurrentElement(); }
            else x.skipCurrentElement();
        }
        if (!href.isEmpty()) { fmt.setAnchor(true); fmt.setAnchorHref(href); fmt.setForeground(QColor("#0563C1")); fmt.setFontUnderline(true); }
        if (!text.isEmpty()) p.runs.push_back({ text, fmt, false });
        if (pb) p.runs.push_back({ QString(), QTextCharFormat(), true });
    }

    void parsePPr(QXmlStreamReader &x, ParaData &p)
    {
        while (x.readNextStartElement()) {
            const QStringView n = x.name();
            if (n == QLatin1String("pStyle")) {
                const QString v = wval(x);
                if (v.startsWith(QLatin1String("Heading"), Qt::CaseInsensitive)) p.heading = qBound(1, v.mid(7).toInt(), 6);
                x.skipCurrentElement();
            } else if (n == QLatin1String("jc")) {
                const QString v = wval(x);
                if (v == QLatin1String("center")) p.bf.setAlignment(Qt::AlignHCenter);
                else if (v == QLatin1String("right") || v == QLatin1String("end")) p.bf.setAlignment(Qt::AlignRight);
                else if (v == QLatin1String("both")) p.bf.setAlignment(Qt::AlignJustify);
                x.skipCurrentElement();
            } else if (n == QLatin1String("ind")) {
                const double left = wval(x, "left").toDouble(), first = wval(x, "firstLine").toDouble();
                if (left > 0) p.bf.setLeftMargin(left / 15.0);
                if (first > 0) p.bf.setTextIndent(first / 15.0);
                x.skipCurrentElement();
            } else if (n == QLatin1String("spacing")) {
                const double before = wval(x, "before").toDouble(), after = wval(x, "after").toDouble(), line = wval(x, "line").toDouble();
                const QString rule = wval(x, "lineRule");
                if (before > 0) p.bf.setTopMargin(before / 15.0);
                if (after > 0) p.bf.setBottomMargin(after / 15.0);
                if (line > 0 && (rule.isEmpty() || rule == QLatin1String("auto")) && qAbs(line - 240) > 6)
                    p.bf.setLineHeight(line / 240.0 * 100.0, QTextBlockFormat::ProportionalHeight);
                x.skipCurrentElement();
            } else if (n == QLatin1String("pageBreakBefore")) { p.pageBreakBefore = true; x.skipCurrentElement(); }
            else if (n == QLatin1String("numPr")) {
                int numId = 0;
                while (x.readNextStartElement()) {
                    if (x.name() == QLatin1String("ilvl")) p.level = wval(x).toInt();
                    else if (x.name() == QLatin1String("numId")) numId = wval(x).toInt();
                    x.skipCurrentElement();
                }
                if (numId > 0) p.listKind = absBullet.value(numToAbs.value(numId, -1), true) ? 1 : 2;
            } else x.skipCurrentElement();
        }
    }

    ParaData parsePara(QXmlStreamReader &x)
    {
        ParaData p;
        while (x.readNextStartElement()) {
            const QStringView n = x.name();
            if (n == QLatin1String("pPr")) parsePPr(x, p);
            else if (n == QLatin1String("r")) parseRun(x, p, QString());
            else if (n == QLatin1String("hyperlink")) {
                const QString id = x.attributes().value(kRel, QStringLiteral("id")).toString();
                const QString href = links.value(id);
                while (x.readNextStartElement()) { if (x.name() == QLatin1String("r")) parseRun(x, p, href); else x.skipCurrentElement(); }
            } else x.skipCurrentElement();
        }
        return p;
    }

    CellData parseCell(QXmlStreamReader &x)
    {
        CellData c;
        while (x.readNextStartElement()) {
            if (x.name() == QLatin1String("tcPr")) {
                while (x.readNextStartElement()) {
                    if (x.name() == QLatin1String("gridSpan")) c.span = qMax(1, wval(x).toInt());
                    else if (x.name() == QLatin1String("vMerge")) c.vmerge = wval(x) == QLatin1String("restart") ? 1 : 2;
                    x.skipCurrentElement();
                }
            } else if (x.name() == QLatin1String("p")) c.paras.push_back(parsePara(x));
            else x.skipCurrentElement();
        }
        return c;
    }

    TableData parseTable(QXmlStreamReader &x)
    {
        TableData t;
        while (x.readNextStartElement()) {
            if (x.name() == QLatin1String("tr")) {
                std::vector<CellData> row;
                while (x.readNextStartElement()) { if (x.name() == QLatin1String("tc")) row.push_back(parseCell(x)); else x.skipCurrentElement(); }
                t.rows.push_back(std::move(row));
            } else x.skipCurrentElement();
        }
        return t;
    }
};

qreal headingSize(int level) { return level == 1 ? 20 : level == 2 ? 16 : level == 3 ? 13 : 12; }

struct ListState { QTextList *lists[9] = { nullptr }; int kinds[9] = { 0 }; };

void emitParagraph(QTextCursor &c, const ParaData &p, bool reuseBlock, ListState &ls, bool &pendingBreak)
{
    QTextBlockFormat bf = p.bf;
    if (p.heading > 0) bf.setHeadingLevel(p.heading);
    if (p.pageBreakBefore || pendingBreak) { bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore); pendingBreak = false; }
    if (reuseBlock) c.setBlockFormat(bf); else c.insertBlock(bf, QTextCharFormat());
    if (p.listKind) {
        const int lvl = qBound(0, p.level, 8);
        if (ls.lists[lvl] && ls.kinds[lvl] == p.listKind) ls.lists[lvl]->add(c.block());
        else {
            QTextListFormat lf;
            lf.setStyle(p.listKind == 2 ? QTextListFormat::ListDecimal : QTextListFormat::ListDisc);
            lf.setIndent(lvl + 1);
            ls.lists[lvl] = c.createList(lf);
            ls.kinds[lvl] = p.listKind;
        }
        for (int i = lvl + 1; i < 9; ++i) ls.lists[i] = nullptr;
    } else {
        for (int i = 0; i < 9; ++i) ls.lists[i] = nullptr;
    }
    for (const RunData &r : p.runs) {
        if (r.pageBreak) { pendingBreak = true; continue; }
        QTextCharFormat f = r.fmt;
        if (p.heading > 0) {
            if (!f.hasProperty(QTextFormat::FontPointSize)) f.setFontPointSize(headingSize(p.heading));
            if (!f.hasProperty(QTextFormat::FontWeight)) f.setFontWeight(QFont::Bold);
        }
        c.insertText(r.text, f);
    }
}

void build(QTextDocument *doc, const std::vector<Item> &items)
{
    doc->setUndoRedoEnabled(false);
    doc->clear();
    QTextCursor cur(doc);
    bool first = true, reuse = true, pending = false;
    ListState ls;
    for (const Item &it : items) {
        if (!it.isTable) {
            emitParagraph(cur, it.para, reuse, ls, pending);
            reuse = false;
            first = false;
            continue;
        }
        if (!reuse) cur.insertBlock(QTextBlockFormat(), QTextCharFormat());
        const auto &rows = it.table.rows;
        int cols = 0;
        struct Placed { int r, c, span, vmerge; const CellData *cell; };
        std::vector<Placed> placed;
        for (size_t r = 0; r < rows.size(); ++r) {
            int col = 0;
            for (const CellData &cd : rows[r]) { placed.push_back({ int(r), col, cd.span, cd.vmerge, &cd }); col += cd.span; }
            cols = qMax(cols, col);
        }
        if (rows.empty() || cols == 0) { reuse = false; continue; }
        QTextTableFormat tf;
        tf.setBorder(1);
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        tf.setBorderBrush(QColor("#808080"));
        tf.setCellPadding(4);
        tf.setCellSpacing(0);
        tf.setBorderCollapse(true);
        tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
        QTextTable *table = cur.insertTable(int(rows.size()), cols, tf);
        for (const Placed &pl : placed) {
            if (pl.vmerge == 2) continue;
            QTextCursor cc = table->cellAt(pl.r, pl.c).firstCursorPosition();
            ListState local;
            bool pb = false, firstPara = true;
            for (const ParaData &pd : pl.cell->paras) { emitParagraph(cc, pd, firstPara, local, pb); firstPara = false; }
        }
        for (const Placed &pl : placed) {
            int rowSpan = 1;
            if (pl.vmerge == 1)
                for (const Placed &below : placed) if (below.c == pl.c && below.r > pl.r && below.r <= pl.r + rowSpan && below.vmerge == 2) rowSpan = below.r - pl.r + 1;
            if (pl.span > 1 || rowSpan > 1) table->mergeCells(pl.r, pl.c, rowSpan, pl.span);
        }
        cur.setPosition(table->lastPosition() + 1); // empty block that follows the table
        reuse = true;
        first = false;
    }
    Q_UNUSED(first)
    doc->setUndoRedoEnabled(true);
}

void parseNumbering(const QByteArray &xml, QHash<int, int> &numToAbs, QHash<int, bool> &absBullet)
{
    QXmlStreamReader x(xml);
    int curAbs = -1, curNum = -1;
    bool wantFmt = false;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isEndElement()) {
            if (x.name() == QLatin1String("abstractNum")) curAbs = -1;
            else if (x.name() == QLatin1String("num")) curNum = -1;
            continue;
        }
        if (!x.isStartElement()) continue;
        const QStringView n = x.name();
        if (n == QLatin1String("abstractNum")) { curAbs = wval(x, "abstractNumId").toInt(); wantFmt = false; }
        else if (n == QLatin1String("lvl") && curAbs >= 0) wantFmt = wval(x, "ilvl") == QLatin1String("0");
        else if (n == QLatin1String("numFmt") && curAbs >= 0 && wantFmt) { absBullet[curAbs] = wval(x) == QLatin1String("bullet"); wantFmt = false; }
        else if (n == QLatin1String("num")) curNum = wval(x, "numId").toInt();
        else if (n == QLatin1String("abstractNumId") && curNum >= 0) numToAbs[curNum] = wval(x).toInt();
    }
}

} // namespace

namespace DocxIO {

QByteArray write(const QTextDocument *doc, const PageSetup &ps)
{
    QString body;
    writeFrame(doc->rootFrame(), body);
    const double pw = mmToTw(ps.landscape ? ps.heightMm : ps.widthMm), ph = mmToTw(ps.landscape ? ps.widthMm : ps.heightMm);
    const QString sect = QStringLiteral("<w:sectPr><w:pgSz w:w=\"%1\" w:h=\"%2\"%3/><w:pgMar w:top=\"%4\" w:right=\"%5\" w:bottom=\"%6\" w:left=\"%7\" w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/></w:sectPr>")
                             .arg(qRound(pw)).arg(qRound(ph)).arg(ps.landscape ? QStringLiteral(" w:orient=\"landscape\"") : QString())
                             .arg(qRound(mmToTw(ps.marginTopMm))).arg(qRound(mmToTw(ps.marginRightMm)))
                             .arg(qRound(mmToTw(ps.marginBottomMm))).arg(qRound(mmToTw(ps.marginLeftMm)));
    const QString document = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:document xmlns:w=\"%1\" xmlns:r=\"%2\"><w:body>%3%4</w:body></w:document>")
                                 .arg(kW, kRel, body, sect);
    const QString contentTypes = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/><Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/><Override PartName=\"/word/numbering.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml\"/></Types>");
    const QString rels = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>");
    const QString docRels = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/><Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering\" Target=\"numbering.xml\"/></Relationships>");

    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&buf);
        zip.setCompressionPolicy(QZipWriter::AutoCompress);
        zip.addFile(QStringLiteral("[Content_Types].xml"), contentTypes.toUtf8());
        zip.addFile(QStringLiteral("_rels/.rels"), rels.toUtf8());
        zip.addFile(QStringLiteral("word/document.xml"), document.toUtf8());
        zip.addFile(QStringLiteral("word/styles.xml"), stylesXml().toUtf8());
        zip.addFile(QStringLiteral("word/numbering.xml"), numberingXml().toUtf8());
        zip.addFile(QStringLiteral("word/_rels/document.xml.rels"), docRels.toUtf8());
        zip.close();
        if (zip.status() != QZipWriter::NoError) return {};
    }
    return out;
}

bool read(const QByteArray &data, QTextDocument *doc, PageSetup *ps, QString *error)
{
    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    QZipReader zip(&buf);
    if (!zip.isReadable()) { if (error) *error = QObject::tr("Not a valid DOCX file (cannot read ZIP container)."); return false; }
    const QByteArray docXml = zip.fileData(QStringLiteral("word/document.xml"));
    if (docXml.isEmpty()) { if (error) *error = QObject::tr("The DOCX file has no document.xml."); return false; }
    if (docXml.size() > 256 * 1024 * 1024) { if (error) *error = QObject::tr("The document is too large."); return false; }

    Parser parser;
    const QByteArray relsXml = zip.fileData(QStringLiteral("word/_rels/document.xml.rels"));
    if (!relsXml.isEmpty()) {
        QXmlStreamReader r(relsXml);
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement() && r.name() == QLatin1String("Relationship"))
                parser.links.insert(r.attributes().value(QStringLiteral("Id")).toString(), r.attributes().value(QStringLiteral("Target")).toString());
        }
    }
    const QByteArray numXml = zip.fileData(QStringLiteral("word/numbering.xml"));
    if (!numXml.isEmpty()) parseNumbering(numXml, parser.numToAbs, parser.absBullet);

    QXmlStreamReader x(docXml);
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement() && x.name() == QLatin1String("body")) { parser.parseBody(x); break; }
    }
    if (x.hasError() && parser.items.empty()) { if (error) *error = QObject::tr("The document XML is malformed: %1").arg(x.errorString()); return false; }
    build(doc, parser.items);
    if (ps && parser.havePage) *ps = parser.page;
    return true;
}

} // namespace DocxIO

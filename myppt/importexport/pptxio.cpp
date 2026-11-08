#include "pptxio.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <functional>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

namespace PptxIO {
namespace {
const char kHeader[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
const char kNsA[] = "http://schemas.openxmlformats.org/drawingml/2006/main";
const char kNsR[] = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
const char kNsP[] = "http://schemas.openxmlformats.org/presentationml/2006/main";
const char kRelBase[] = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/";
const char kExtrasRel[] = "http://myoffice.local/relationships/extras";
const char kExtrasPath[] = "ppt/myoffice/extras.xml";
constexpr qreal kEmu = 9525.0; // EMU per slide unit

QString nsDecl()
{
    return QStringLiteral(" xmlns:a=\"%1\" xmlns:r=\"%2\" xmlns:p=\"%3\"").arg(QLatin1String(kNsA), QLatin1String(kNsR), QLatin1String(kNsP));
}
QString hex(const QColor &c) { return c.name().mid(1).toUpper(); }
QString emu(qreal v) { return QString::number(qRound64(v * kEmu)); }
QString esc(const QString &s) { return s.toHtmlEscaped(); }

// ================================================================== write
QString fillXml(const QColor &c)
{
    return c.isValid() ? QStringLiteral("<a:solidFill><a:srgbClr val=\"%1\"/></a:solidFill>").arg(hex(c)) : QStringLiteral("<a:noFill/>");
}

QString lineXml(const SlideObject &o)
{
    if (!o.line.isValid() || o.lineWidth <= 0) return QStringLiteral("<a:ln><a:noFill/></a:ln>");
    return QStringLiteral("<a:ln w=\"%1\">%2</a:ln>").arg(emu(o.lineWidth), fillXml(o.line));
}

QString xfrmXml(const SlideObject &o)
{
    QString attrs;
    if (!qFuzzyIsNull(o.rotation)) attrs += QStringLiteral(" rot=\"%1\"").arg(qRound(o.rotation * 60000.0));
    if (o.flipV) attrs += QStringLiteral(" flipV=\"1\"");
    return QStringLiteral("<a:xfrm%1><a:off x=\"%2\" y=\"%3\"/><a:ext cx=\"%4\" cy=\"%5\"/></a:xfrm>")
        .arg(attrs, emu(o.rect.x()), emu(o.rect.y()), emu(o.rect.width()), emu(o.rect.height()));
}

QString paragraphsXml(const SlideObject &o)
{
    const QString algn = o.align == 1 ? QStringLiteral("ctr") : o.align == 2 ? QStringLiteral("r") : QStringLiteral("l");
    const int sz = qRound(o.fontSize * 100);
    QString out;
    const QStringList lines = o.text.split(QLatin1Char('\n'));
    for (const QString &ln : lines) {
        out += QStringLiteral("<a:p><a:pPr algn=\"%1\"/>").arg(algn);
        if (ln.isEmpty()) {
            out += QStringLiteral("<a:endParaRPr lang=\"en-US\" sz=\"%1\"/>").arg(sz);
        } else {
            out += QStringLiteral("<a:r><a:rPr lang=\"en-US\" sz=\"%1\" b=\"%2\" i=\"%3\"%4 dirty=\"0\">%5</a:rPr><a:t>%6</a:t></a:r>")
                       .arg(sz).arg(o.bold ? 1 : 0).arg(o.italic ? 1 : 0)
                       .arg(o.underline ? QStringLiteral(" u=\"sng\"") : QString())
                       .arg(fillXml(o.textColor.isValid() ? o.textColor : QColor(Qt::black)), esc(ln));
        }
        out += QStringLiteral("</a:p>");
    }
    return out;
}

QString shapeXml(const SlideObject &o, int id, const QString &imageRid)
{
    const QString name = QStringLiteral("%1 %2").arg(esc(objectLabel(o)), QString::number(id));
    if (o.type == SlideObject::Image) {
        return QStringLiteral("<p:pic><p:nvPicPr><p:cNvPr id=\"%1\" name=\"%2\"/><p:cNvPicPr><a:picLocks noChangeAspect=\"1\"/></p:cNvPicPr><p:nvPr/></p:nvPicPr>"
                              "<p:blipFill><a:blip r:embed=\"%3\"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>"
                              "<p:spPr>%4<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr></p:pic>")
            .arg(QString::number(id), name, imageRid, xfrmXml(o));
    }
    if (o.type == SlideObject::Line) {
        return QStringLiteral("<p:cxnSp><p:nvCxnSpPr><p:cNvPr id=\"%1\" name=\"%2\"/><p:cNvCxnSpPr/><p:nvPr/></p:nvCxnSpPr>"
                              "<p:spPr>%3<a:prstGeom prst=\"line\"><a:avLst/></a:prstGeom>%4</p:spPr></p:cxnSp>")
            .arg(QString::number(id), name, xfrmXml(o), lineXml(o));
    }
    const char *prst = o.type == SlideObject::RoundRect ? "roundRect" : o.type == SlideObject::Ellipse ? "ellipse" : "rect";
    const bool txBox = o.type == SlideObject::Text;
    const QString anchor = o.valign == 1 ? QStringLiteral("ctr") : o.valign == 2 ? QStringLiteral("b") : QStringLiteral("t");
    return QStringLiteral("<p:sp><p:nvSpPr><p:cNvPr id=\"%1\" name=\"%2\"/><p:cNvSpPr%3/><p:nvPr/></p:nvSpPr>"
                          "<p:spPr>%4<a:prstGeom prst=\"%5\"><a:avLst/></a:prstGeom>%6%7</p:spPr>"
                          "<p:txBody><a:bodyPr wrap=\"square\" lIns=\"95250\" tIns=\"57150\" rIns=\"95250\" bIns=\"57150\" anchor=\"%8\"><a:noAutofit/></a:bodyPr><a:lstStyle/>%9</p:txBody></p:sp>")
        .arg(QString::number(id), name, txBox ? QStringLiteral(" txBox=\"1\"") : QString(), xfrmXml(o), QLatin1String(prst), fillXml(o.fill), lineXml(o), anchor, paragraphsXml(o));
}

QString transitionXml(const Slide &s)
{
    if (s.transition == Ppt::Transition::None && s.advanceAfterMs <= 0) return {};
    using T = Ppt::Transition;
    QString inner;
    switch (s.transition) {
    case T::None: break;
    case T::Fade: inner = QStringLiteral("<p:fade/>"); break;
    case T::PushLeft: inner = QStringLiteral("<p:push dir=\"l\"/>"); break;
    case T::PushUp: inner = QStringLiteral("<p:push dir=\"u\"/>"); break;
    case T::Wipe: inner = QStringLiteral("<p:wipe dir=\"r\"/>"); break;
    case T::Cover: inner = QStringLiteral("<p:cover dir=\"l\"/>"); break;
    case T::Zoom: inner = QStringLiteral("<p:zoom dir=\"in\"/>"); break;
    case T::Dissolve: inner = QStringLiteral("<p:dissolve/>"); break;
    }
    const QString spd = s.transitionMs >= 1000 ? QStringLiteral("slow") : s.transitionMs <= 450 ? QStringLiteral("fast") : QStringLiteral("med");
    QString attrs = QStringLiteral(" spd=\"%1\"").arg(spd);
    if (s.advanceAfterMs > 0) attrs += QStringLiteral(" advTm=\"%1\"").arg(s.advanceAfterMs);
    return QStringLiteral("<p:transition%1>%2</p:transition>").arg(attrs, inner);
}

const char kGroupHeader[] = "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/><a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>";

QString themeXml()
{
    const QString fill = QStringLiteral("<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>");
    const QString ln = QStringLiteral("<a:ln w=\"9525\"><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:ln>");
    const QString fx = QStringLiteral("<a:effectStyle><a:effectLst/></a:effectStyle>");
    const QString font = QStringLiteral("<a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/><a:cs typeface=\"\"/>");
    auto acc = [](const char *n, const char *c) { return QStringLiteral("<a:%1><a:srgbClr val=\"%2\"/></a:%1>").arg(QLatin1String(n), QLatin1String(c)); };
    return QString::fromLatin1(kHeader)
        + QStringLiteral("<a:theme xmlns:a=\"%1\" name=\"MyOffice\"><a:themeElements><a:clrScheme name=\"MyOffice\">").arg(QLatin1String(kNsA))
        + QStringLiteral("<a:dk1><a:sysClr val=\"windowText\" lastClr=\"000000\"/></a:dk1><a:lt1><a:sysClr val=\"window\" lastClr=\"FFFFFF\"/></a:lt1>")
        + acc("dk2", "1F2937") + acc("lt2", "F3F4F6") + acc("accent1", "3B82F6") + acc("accent2", "E8663A") + acc("accent3", "1E9E62")
        + acc("accent4", "F59E0B") + acc("accent5", "8B5CF6") + acc("accent6", "64748B") + acc("hlink", "2563EB") + acc("folHlink", "7C3AED")
        + QStringLiteral("</a:clrScheme><a:fontScheme name=\"MyOffice\"><a:majorFont>%1</a:majorFont><a:minorFont>%1</a:minorFont></a:fontScheme>").arg(font)
        + QStringLiteral("<a:fmtScheme name=\"MyOffice\"><a:fillStyleLst>%1%1%1</a:fillStyleLst><a:lnStyleLst>%2%2%2</a:lnStyleLst><a:effectStyleLst>%3%3%3</a:effectStyleLst><a:bgFillStyleLst>%1%1%1</a:bgFillStyleLst></a:fmtScheme>").arg(fill, ln, fx)
        + QStringLiteral("</a:themeElements></a:theme>");
}

QString rels(const QList<QStringList> &items) // each: id, type-suffix-or-full, target
{
    QString out = QString::fromLatin1(kHeader) + QStringLiteral("<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
    for (const QStringList &r : items) {
        const QString type = r[1].startsWith(QLatin1String("http")) ? r[1] : QLatin1String(kRelBase) + r[1];
        out += QStringLiteral("<Relationship Id=\"%1\" Type=\"%2\" Target=\"%3\"/>").arg(r[0], type, r[2]);
    }
    return out + QStringLiteral("</Relationships>");
}

QByteArray extrasXml(const Deck &deck)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("myoffice"));
    w.writeAttribute(QStringLiteral("version"), QStringLiteral("1"));
    for (int i = 0; i < deck.slides.size(); ++i) {
        const Slide &s = deck.slides[i];
        w.writeStartElement(QStringLiteral("slide"));
        w.writeAttribute(QStringLiteral("index"), QString::number(i));
        w.writeAttribute(QStringLiteral("transitionMs"), QString::number(s.transitionMs));
        if (!s.notes.isEmpty()) w.writeTextElement(QStringLiteral("notes"), s.notes);
        for (int k = 0; k < s.objects.size(); ++k) {
            const Animation &a = s.objects[k].anim;
            if (a.effect == Ppt::Effect::None) continue;
            w.writeStartElement(QStringLiteral("anim"));
            w.writeAttribute(QStringLiteral("id"), QString::number(k + 2));
            w.writeAttribute(QStringLiteral("effect"), QString::number(int(a.effect)));
            w.writeAttribute(QStringLiteral("trigger"), QString::number(int(a.trigger)));
            w.writeAttribute(QStringLiteral("duration"), QString::number(a.durationMs));
            w.writeAttribute(QStringLiteral("delay"), QString::number(a.delayMs));
            w.writeEndElement();
        }
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}
} // namespace

QByteArray write(const Deck &deckIn)
{
    Deck deck = deckIn;
    if (deck.slides.isEmpty()) deck.slides << Slide();
    const int n = deck.slides.size();
    const QString h = QString::fromLatin1(kHeader);
    const QString ct = QStringLiteral("application/vnd.openxmlformats-officedocument.");

    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    {
        QZipWriter zip(&buf);
        zip.setCompressionPolicy(QZipWriter::AutoCompress);

        QString content = h + QStringLiteral("<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                                            "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                                            "<Default Extension=\"xml\" ContentType=\"application/xml\"/><Default Extension=\"png\" ContentType=\"image/png\"/>");
        content += QStringLiteral("<Override PartName=\"/ppt/presentation.xml\" ContentType=\"%1presentationml.presentation.main+xml\"/>").arg(ct);
        content += QStringLiteral("<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" ContentType=\"%1presentationml.slideMaster+xml\"/>").arg(ct);
        content += QStringLiteral("<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" ContentType=\"%1presentationml.slideLayout+xml\"/>").arg(ct);
        content += QStringLiteral("<Override PartName=\"/ppt/theme/theme1.xml\" ContentType=\"%1theme+xml\"/>").arg(ct);
        content += QStringLiteral("<Override PartName=\"/%1\" ContentType=\"application/vnd.myoffice.extras+xml\"/>").arg(QLatin1String(kExtrasPath));
        for (int i = 1; i <= n; ++i)
            content += QStringLiteral("<Override PartName=\"/ppt/slides/slide%1.xml\" ContentType=\"%2presentationml.slide+xml\"/>").arg(i).arg(ct);
        content += QStringLiteral("</Types>");
        zip.addFile(QStringLiteral("[Content_Types].xml"), content.toUtf8());
        zip.addFile(QStringLiteral("_rels/.rels"), rels({ { QStringLiteral("rId1"), QStringLiteral("officeDocument"), QStringLiteral("ppt/presentation.xml") } }).toUtf8());

        // presentation
        QString sldIds;
        QList<QStringList> prels { { QStringLiteral("rId1"), QStringLiteral("slideMaster"), QStringLiteral("slideMasters/slideMaster1.xml") } };
        for (int i = 0; i < n; ++i) {
            sldIds += QStringLiteral("<p:sldId id=\"%1\" r:id=\"rId%2\"/>").arg(256 + i).arg(i + 2);
            prels.append({ QStringLiteral("rId%1").arg(i + 2), QStringLiteral("slide"), QStringLiteral("slides/slide%1.xml").arg(i + 1) });
        }
        prels.append({ QStringLiteral("rId%1").arg(n + 2), QStringLiteral("theme"), QStringLiteral("theme/theme1.xml") });
        prels.append({ QStringLiteral("rId%1").arg(n + 3), QLatin1String(kExtrasRel), QStringLiteral("myoffice/extras.xml") });
        zip.addFile(QStringLiteral("ppt/presentation.xml"),
                    (h + QStringLiteral("<p:presentation%1><p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst><p:sldIdLst>%2</p:sldIdLst>"
                                        "<p:sldSz cx=\"12192000\" cy=\"6858000\"/><p:notesSz cx=\"6858000\" cy=\"9144000\"/></p:presentation>").arg(nsDecl(), sldIds)).toUtf8());
        zip.addFile(QStringLiteral("ppt/_rels/presentation.xml.rels"), rels(prels).toUtf8());

        // master, layout, theme
        const QString tree = QStringLiteral("<p:cSld%1><p:spTree>%2</p:spTree></p:cSld>");
        zip.addFile(QStringLiteral("ppt/slideMasters/slideMaster1.xml"),
                    (h + QStringLiteral("<p:sldMaster%1><p:cSld><p:bg><p:bgRef idx=\"1001\"><a:schemeClr val=\"bg1\"/></p:bgRef></p:bg><p:spTree>%2</p:spTree></p:cSld>"
                                        "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" hlink=\"hlink\" folHlink=\"folHlink\"/>"
                                        "<p:sldLayoutIdLst><p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/></p:sldLayoutIdLst></p:sldMaster>").arg(nsDecl(), QLatin1String(kGroupHeader))).toUtf8());
        zip.addFile(QStringLiteral("ppt/slideMasters/_rels/slideMaster1.xml.rels"),
                    rels({ { QStringLiteral("rId1"), QStringLiteral("slideLayout"), QStringLiteral("../slideLayouts/slideLayout1.xml") },
                           { QStringLiteral("rId2"), QStringLiteral("theme"), QStringLiteral("../theme/theme1.xml") } }).toUtf8());
        zip.addFile(QStringLiteral("ppt/slideLayouts/slideLayout1.xml"),
                    (h + QStringLiteral("<p:sldLayout%1 type=\"blank\" preserve=\"1\">").arg(nsDecl()) + tree.arg(QStringLiteral(" name=\"Blank\""), QLatin1String(kGroupHeader))
                     + QStringLiteral("<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>")).toUtf8());
        zip.addFile(QStringLiteral("ppt/slideLayouts/_rels/slideLayout1.xml.rels"),
                    rels({ { QStringLiteral("rId1"), QStringLiteral("slideMaster"), QStringLiteral("../slideMasters/slideMaster1.xml") } }).toUtf8());
        zip.addFile(QStringLiteral("ppt/theme/theme1.xml"), themeXml().toUtf8());

        // slides
        int imageCounter = 0;
        for (int i = 0; i < n; ++i) {
            const Slide &s = deck.slides[i];
            QList<QStringList> srels { { QStringLiteral("rId1"), QStringLiteral("slideLayout"), QStringLiteral("../slideLayouts/slideLayout1.xml") } };
            QString shapes;
            for (int k = 0; k < s.objects.size(); ++k) {
                const SlideObject &o = s.objects[k];
                QString rid;
                if (o.type == SlideObject::Image) {
                    if (o.image.isNull()) continue;
                    const QString media = QStringLiteral("image%1.png").arg(++imageCounter);
                    QByteArray png;
                    QBuffer pb(&png);
                    pb.open(QIODevice::WriteOnly);
                    o.image.save(&pb, "PNG");
                    zip.addFile(QStringLiteral("ppt/media/") + media, png);
                    rid = QStringLiteral("rId%1").arg(srels.size() + 1);
                    srels.append({ rid, QStringLiteral("image"), QStringLiteral("../media/") + media });
                }
                shapes += shapeXml(o, k + 2, rid);
            }
            const QString bg = QStringLiteral("<p:bg><p:bgPr>%1<a:effectLst/></p:bgPr></p:bg>").arg(fillXml(s.background.isValid() ? s.background : QColor(Qt::white)));
            zip.addFile(QStringLiteral("ppt/slides/slide%1.xml").arg(i + 1),
                        (h + QStringLiteral("<p:sld%1><p:cSld>%2<p:spTree>%3%4</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>%5</p:sld>")
                                 .arg(nsDecl(), bg, QLatin1String(kGroupHeader), shapes, transitionXml(s))).toUtf8());
            zip.addFile(QStringLiteral("ppt/slides/_rels/slide%1.xml.rels").arg(i + 1), rels(srels).toUtf8());
        }
        zip.addFile(QLatin1String(kExtrasPath), extrasXml(deck));
        zip.close();
        if (zip.status() != QZipWriter::NoError) return {};
    }
    return out;
}

// ================================================================== read
namespace {
QString attrStr(const QXmlStreamAttributes &a, const char *name) { return a.value(QLatin1String(name)).toString(); }
qreal attrNum(const QXmlStreamAttributes &a, const char *name) { return a.value(QLatin1String(name)).toString().toDouble(); }

QColor schemeColor(const QString &v)
{
    static const QHash<QString, QString> map = {
        { "tx1", "#000000" }, { "dk1", "#000000" }, { "bg1", "#FFFFFF" }, { "lt1", "#FFFFFF" }, { "tx2", "#44546A" }, { "dk2", "#44546A" },
        { "bg2", "#E7E6E6" }, { "lt2", "#E7E6E6" }, { "accent1", "#4472C4" }, { "accent2", "#ED7D31" }, { "accent3", "#A5A5A5" },
        { "accent4", "#FFC000" }, { "accent5", "#5B9BD5" }, { "accent6", "#70AD47" }, { "hlink", "#0563C1" } };
    return QColor(map.value(v, QStringLiteral("#000000")));
}

// Positioned on <a:solidFill>; consumes up to and including its end tag.
QColor readFill(QXmlStreamReader &x)
{
    QColor c;
    int depth = 1;
    while (!x.atEnd() && depth > 0) {
        x.readNext();
        if (x.isStartElement()) {
            ++depth;
            if (c.isValid()) continue;
            const auto name = x.name();
            const auto a = x.attributes();
            if (name == QLatin1String("srgbClr")) c = QColor(QLatin1Char('#') + attrStr(a, "val"));
            else if (name == QLatin1String("schemeClr")) c = schemeColor(attrStr(a, "val"));
            else if (name == QLatin1String("sysClr")) c = QColor(QLatin1Char('#') + attrStr(a, "lastClr"));
            else if (name == QLatin1String("prstClr")) c = QColor(attrStr(a, "val"));
        } else if (x.isEndElement()) {
            --depth;
        }
    }
    return c;
}

// Positioned on <a:xfrm>; consumes up to and including its end tag.
void readXfrm(QXmlStreamReader &x, SlideObject &o, qreal sx, qreal sy)
{
    const auto at = x.attributes();
    o.rotation = attrNum(at, "rot") / 60000.0;
    o.flipV = attrStr(at, "flipV") == QLatin1String("1");
    qreal ox = 0, oy = 0, cx = 0, cy = 0;
    int depth = 1;
    while (!x.atEnd() && depth > 0) {
        x.readNext();
        if (x.isStartElement()) {
            ++depth;
            const auto a = x.attributes();
            if (x.name() == QLatin1String("off")) { ox = attrNum(a, "x"); oy = attrNum(a, "y"); }
            else if (x.name() == QLatin1String("ext")) { cx = attrNum(a, "cx"); cy = attrNum(a, "cy"); }
        } else if (x.isEndElement()) {
            --depth;
        }
    }
    o.rect = QRectF(ox * sx, oy * sy, cx * sx, cy * sy);
}

using ImageLookup = std::function<QImage(const QString &)>;

// Positioned on <p:sp>, <p:cxnSp> or <p:pic>; consumes the whole element. Returns false if it should be dropped.
bool parseShape(QXmlStreamReader &x, const QString &kind, qreal sx, qreal sy, const ImageLookup &imageFor, SlideObject &o)
{
    o = SlideObject();
    o.type = kind == QLatin1String("pic") ? SlideObject::Image : kind == QLatin1String("cxnSp") ? SlideObject::Line : SlideObject::Rect;
    o.fill = QColor();
    o.line = QColor();
    o.lineWidth = 0;
    bool hasXfrm = false, inSpPr = false, inLn = false, inTx = false, inRPr = false, hasStyle = false, txBox = false;
    bool fillSet = false, lineSet = false, colorSet = false, sizeSet = false, algnSet = false, anchorSet = false, isPh = false, firstRun = true;
    QString ph, prst, para;
    QStringList paras;
    int depth = 1;
    while (!x.atEnd() && depth > 0) {
        x.readNext();
        if (x.isStartElement()) {
            ++depth;
            const QString n = x.name().toString();
            const auto a = x.attributes();
            if (n == QLatin1String("cNvPr")) {
                o.id = attrStr(a, "id").toInt();
            } else if (n == QLatin1String("cNvSpPr")) {
                txBox = attrStr(a, "txBox") == QLatin1String("1");
            } else if (n == QLatin1String("ph")) {
                isPh = true;
                ph = attrStr(a, "type");
                if (ph.isEmpty()) ph = QStringLiteral("body");
            } else if (n == QLatin1String("style")) {
                hasStyle = true;
            } else if (n == QLatin1String("spPr")) {
                inSpPr = true;
            } else if (n == QLatin1String("xfrm") && inSpPr) {
                readXfrm(x, o, sx, sy);
                hasXfrm = true;
                --depth;
            } else if (n == QLatin1String("prstGeom")) {
                prst = attrStr(a, "prst");
            } else if (n == QLatin1String("ln") && inSpPr) {
                inLn = true;
                const QString w = attrStr(a, "w");
                o.lineWidth = w.isEmpty() ? 1.33 : w.toDouble() * sx;
            } else if (n == QLatin1String("noFill")) {
                if (inLn) { lineSet = true; o.line = QColor(); }
                else if (inSpPr) { fillSet = true; o.fill = QColor(); }
            } else if (n == QLatin1String("solidFill")) {
                const QColor c = readFill(x);
                --depth;
                if (inRPr) { if (!colorSet && c.isValid()) { o.textColor = c; colorSet = true; } }
                else if (inLn) { o.line = c; lineSet = true; }
                else if (inSpPr) { o.fill = c; fillSet = true; }
            } else if (n == QLatin1String("bodyPr")) {
                const QString an = attrStr(a, "anchor");
                if (!an.isEmpty()) { anchorSet = true; o.valign = an == QLatin1String("ctr") ? 1 : an == QLatin1String("b") ? 2 : 0; }
            } else if (n == QLatin1String("txBody")) {
                inTx = true;
            } else if (n == QLatin1String("p") && inTx) {
                para.clear();
            } else if (n == QLatin1String("pPr") && inTx && !algnSet) {
                const QString al = attrStr(a, "algn");
                if (!al.isEmpty()) { algnSet = true; o.align = al == QLatin1String("ctr") ? 1 : al == QLatin1String("r") ? 2 : 0; }
            } else if (n == QLatin1String("rPr") && inTx) {
                inRPr = true;
                if (firstRun) {
                    firstRun = false;
                    const QString sz = attrStr(a, "sz");
                    if (!sz.isEmpty()) { o.fontSize = sz.toDouble() / 100.0; sizeSet = true; }
                    o.bold = attrStr(a, "b") == QLatin1String("1");
                    o.italic = attrStr(a, "i") == QLatin1String("1");
                    const QString u = attrStr(a, "u");
                    o.underline = !u.isEmpty() && u != QLatin1String("none");
                }
            } else if (n == QLatin1String("t") && inTx) {
                para += x.readElementText();
                --depth;
            } else if (n == QLatin1String("br") && inTx) {
                para += QLatin1Char('\n');
            } else if (n == QLatin1String("blip")) {
                const QString rid = a.value(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"), QStringLiteral("embed")).toString();
                if (!rid.isEmpty()) o.image = imageFor(rid);
            }
        } else if (x.isEndElement()) {
            --depth;
            const QString n = x.name().toString();
            if (n == QLatin1String("spPr")) inSpPr = false;
            else if (n == QLatin1String("ln")) inLn = false;
            else if (n == QLatin1String("rPr")) inRPr = false;
            else if (n == QLatin1String("txBody")) inTx = false;
            else if (n == QLatin1String("p") && inTx) paras << para;
        }
    }
    o.text = paras.join(QLatin1Char('\n'));

    if (o.type == SlideObject::Image) return !o.image.isNull();

    if (isPh) {
        if (ph == QLatin1String("sldNum") || ph == QLatin1String("dt") || ph == QLatin1String("ftr") || ph == QLatin1String("hdr")) return false;
        const bool title = ph == QLatin1String("title") || ph == QLatin1String("ctrTitle");
        if (!hasXfrm) o.rect = title ? QRectF(80, 40, 1120, 110) : ph == QLatin1String("subTitle") ? QRectF(160, 330, 960, 140) : QRectF(80, 170, 1120, 480);
        o.type = SlideObject::Text;
        if (!sizeSet) o.fontSize = title ? 40 : 24;
        if (!algnSet && (ph == QLatin1String("ctrTitle") || ph == QLatin1String("subTitle"))) o.align = 1;
        if (!anchorSet) o.valign = title ? 1 : 0;
        o.lineWidth = lineSet ? o.lineWidth : 0;
        return !o.text.isEmpty(); // empty placeholders ("Click to add title") render nothing
    }
    if (!hasXfrm) o.rect = QRectF(100, 100, 400, 200);
    if (o.type != SlideObject::Line) {
        if (prst == QLatin1String("roundRect")) o.type = SlideObject::RoundRect;
        else if (prst == QLatin1String("ellipse") || prst == QLatin1String("oval")) o.type = SlideObject::Ellipse;
        else if (prst == QLatin1String("line") || prst.contains(QLatin1String("Connector"))) o.type = SlideObject::Line;
        else if (txBox) o.type = SlideObject::Text;
    }
    const bool shape = o.type == SlideObject::Rect || o.type == SlideObject::RoundRect || o.type == SlideObject::Ellipse;
    if (hasStyle && !fillSet && shape) o.fill = QColor("#4472C4");
    if (hasStyle && !lineSet && (shape || o.type == SlideObject::Line)) { o.line = o.type == SlideObject::Line ? QColor("#4472C4") : QColor("#2F5597"); if (o.lineWidth <= 0) o.lineWidth = 1.33; }
    if (o.type == SlideObject::Line && !o.line.isValid()) o.line = QColor(Qt::black);
    if (o.type == SlideObject::Line && o.lineWidth <= 0) o.lineWidth = 1.33;
    if (shape && hasStyle && !colorSet && o.fill.isValid()) o.textColor = QColor(Qt::white);
    if (shape) {
        if (!algnSet) o.align = 1;
        if (!anchorSet) o.valign = 1;
    }
    if (!sizeSet) o.fontSize = 18;
    return true;
}

QString resolve(const QString &baseDir, const QString &target)
{
    if (target.startsWith(QLatin1Char('/'))) return target.mid(1);
    return QDir::cleanPath(baseDir + QLatin1Char('/') + target);
}

QHash<QString, QString> readRels(QZipReader &zip, const QString &relsPath, const QString &baseDir)
{
    QHash<QString, QString> map;
    QXmlStreamReader r(zip.fileData(relsPath));
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement() && r.name() == QLatin1String("Relationship"))
            map.insert(attrStr(r.attributes(), "Id"), resolve(baseDir, attrStr(r.attributes(), "Target")));
    }
    return map;
}

void parseSlide(const QByteArray &xml, qreal sx, qreal sy, const ImageLookup &imageFor, Slide &slide, int &skipped)
{
    QXmlStreamReader x(xml);
    bool inBg = false, inTrans = false, transSet = false;
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement()) {
            const QString n = x.name().toString();
            if (n == QLatin1String("bg")) {
                inBg = true;
            } else if (inBg && n == QLatin1String("solidFill")) {
                const QColor c = readFill(x);
                if (c.isValid()) slide.background = c;
            } else if (n == QLatin1String("sp") || n == QLatin1String("cxnSp") || n == QLatin1String("pic")) {
                SlideObject o;
                if (parseShape(x, n, sx, sy, imageFor, o)) slide.objects << o;
            } else if (n == QLatin1String("graphicFrame") || n == QLatin1String("oleObj")) {
                ++skipped;
                x.skipCurrentElement();
            } else if (n == QLatin1String("transition")) {
                inTrans = true;
                if (!transSet) {
                    const auto a = x.attributes();
                    const QString spd = attrStr(a, "spd");
                    slide.transitionMs = spd == QLatin1String("slow") ? 1000 : spd == QLatin1String("fast") ? 400 : 700;
                    QString dur = a.value(QLatin1String("p14:dur")).toString();
                    if (!dur.isEmpty()) slide.transitionMs = qBound(100, dur.toInt(), 5000);
                    if (attrStr(a, "advTm").toInt() > 0) slide.advanceAfterMs = attrStr(a, "advTm").toInt();
                }
            } else if (inTrans && !transSet && n != QLatin1String("AlternateContent") && n != QLatin1String("Choice") && n != QLatin1String("Fallback")) {
                using T = Ppt::Transition;
                const QString dir = attrStr(x.attributes(), "dir");
                if (n == QLatin1String("cut")) slide.transition = T::None;
                else if (n == QLatin1String("fade")) slide.transition = T::Fade;
                else if (n == QLatin1String("push")) slide.transition = dir == QLatin1String("u") || dir == QLatin1String("d") ? T::PushUp : T::PushLeft;
                else if (n == QLatin1String("wipe")) slide.transition = T::Wipe;
                else if (n == QLatin1String("cover") || n == QLatin1String("pull")) slide.transition = T::Cover;
                else if (n == QLatin1String("zoom")) slide.transition = T::Zoom;
                else if (n == QLatin1String("dissolve")) slide.transition = T::Dissolve;
                else slide.transition = T::Fade; // any other effect: nearest match
                transSet = true;
            }
        } else if (x.isEndElement()) {
            const QString n = x.name().toString();
            if (n == QLatin1String("bg")) inBg = false;
            else if (n == QLatin1String("transition")) inTrans = false;
        }
    }
}
} // namespace

bool read(const QByteArray &data, Deck &out, QString *error, int *skippedOut)
{
    QBuffer buf;
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    QZipReader zip(&buf);
    const QByteArray pres = zip.fileData(QStringLiteral("ppt/presentation.xml"));
    if (!zip.isReadable() || pres.isEmpty()) {
        if (error) *error = QObject::tr("Not a valid PPTX file.");
        return false;
    }
    const QHash<QString, QString> presRels = readRels(zip, QStringLiteral("ppt/_rels/presentation.xml.rels"), QStringLiteral("ppt"));

    qreal cx = 12192000, cy = 6858000;
    QStringList slidePaths;
    {
        QXmlStreamReader x(pres);
        while (!x.atEnd()) {
            x.readNext();
            if (!x.isStartElement()) continue;
            if (x.name() == QLatin1String("sldSz")) {
                if (attrNum(x.attributes(), "cx") > 0) cx = attrNum(x.attributes(), "cx");
                if (attrNum(x.attributes(), "cy") > 0) cy = attrNum(x.attributes(), "cy");
            } else if (x.name() == QLatin1String("sldId")) {
                const QString rid = x.attributes().value(QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"), QStringLiteral("id")).toString();
                if (presRels.contains(rid)) slidePaths << presRels.value(rid);
            }
        }
    }
    const qreal sx = Ppt::kSlideW / cx, sy = Ppt::kSlideH / cy;

    int skipped = 0;
    Deck deck;
    for (const QString &path : slidePaths) {
        Slide slide;
        const QByteArray xml = zip.fileData(path);
        if (xml.isEmpty()) continue;
        const QString dir = QFileInfo(path).path();
        const QString relPath = dir + QStringLiteral("/_rels/") + QFileInfo(path).fileName() + QStringLiteral(".rels");
        const QHash<QString, QString> srels = readRels(zip, relPath, dir);
        auto imageFor = [&](const QString &rid) -> QImage { return QImage::fromData(zip.fileData(srels.value(rid))); };
        parseSlide(xml, sx, sy, imageFor, slide, skipped);
        deck.slides << slide;
    }

    // MyOffice extras: animations and notes (matched by slide index and shape id).
    const QByteArray extras = zip.fileData(QLatin1String(kExtrasPath));
    if (!extras.isEmpty()) {
        QXmlStreamReader x(extras);
        int si = -1;
        while (!x.atEnd()) {
            x.readNext();
            if (!x.isStartElement()) continue;
            if (x.name() == QLatin1String("slide")) {
                si = attrStr(x.attributes(), "index").toInt();
                if (si >= 0 && si < deck.slides.size() && !attrStr(x.attributes(), "transitionMs").isEmpty())
                    deck.slides[si].transitionMs = qBound(100, attrStr(x.attributes(), "transitionMs").toInt(), 5000);
            } else if (x.name() == QLatin1String("notes") && si >= 0 && si < deck.slides.size()) {
                deck.slides[si].notes = x.readElementText();
            } else if (x.name() == QLatin1String("anim") && si >= 0 && si < deck.slides.size()) {
                const int id = attrStr(x.attributes(), "id").toInt();
                for (SlideObject &o : deck.slides[si].objects) {
                    if (o.id != id) continue;
                    o.anim.effect = Ppt::Effect(qBound(0, attrStr(x.attributes(), "effect").toInt(), 9));
                    o.anim.trigger = Ppt::Trigger(qBound(0, attrStr(x.attributes(), "trigger").toInt(), 2));
                    o.anim.durationMs = qBound(50, attrStr(x.attributes(), "duration").toInt(), 10000);
                    o.anim.delayMs = qBound(0, attrStr(x.attributes(), "delay").toInt(), 10000);
                    break;
                }
            }
        }
    }
    if (deck.slides.isEmpty()) deck.slides << Slide();
    out = deck;
    if (skippedOut) *skippedOut = skipped;
    return true;
}
}

#include "htmlio.h"

#include <QBuffer>
#include <QPixmap>
#include <QImage>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrl>

namespace HtmlIO {

QByteArray write(const QTextDocument *doc)
{
    QString html = doc->toHtml();
    static const QRegularExpression img(QStringLiteral("<img src=\"([^\"]*)\""));
    QString out;
    qsizetype last = 0;
    auto it = img.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        out += html.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
        const QString name = m.captured(1);
        QVariant v = doc->resource(QTextDocument::ImageResource, QUrl(name));
        QImage image = v.canConvert<QImage>() ? qvariant_cast<QImage>(v) : QImage();
        if (image.isNull() && v.canConvert<QPixmap>()) image = qvariant_cast<QPixmap>(v).toImage();
        if (image.isNull()) { out += m.captured(0); continue; }
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        image.save(&buf, "PNG");
        out += QStringLiteral("<img src=\"data:image/png;base64,") + QString::fromLatin1(png.toBase64()) + QLatin1Char('"');
    }
    out += html.mid(last);
    return out.toUtf8();
}

void read(const QByteArray &data, QTextDocument *doc)
{
    QString html = QString::fromUtf8(data);
    static const QRegularExpression dataUri(QStringLiteral("src=\"data:image/[a-zA-Z+.-]+;base64,([^\"]+)\""));
    QString out;
    qsizetype last = 0;
    int n = 0;
    auto it = dataUri.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        out += html.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
        const QImage image = QImage::fromData(QByteArray::fromBase64(m.captured(1).toLatin1()));
        if (image.isNull()) { out += m.captured(0); continue; }
        const QString name = QStringLiteral("myword-image-%1").arg(n++);
        doc->addResource(QTextDocument::ImageResource, QUrl(name), image);
        out += QStringLiteral("src=\"") + name + QLatin1Char('"');
    }
    out += html.mid(last);
    doc->setHtml(out);
}

}

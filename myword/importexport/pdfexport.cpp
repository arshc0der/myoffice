#include "pdfexport.h"

#include <QAbstractTextDocumentLayout>
#include <QFile>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QTextDocument>
#include <memory>

namespace PdfExport {

bool write(const QTextDocument *src, const PageSetup &ps, const QString &path, QString *error)
{
    std::unique_ptr<QTextDocument> doc(src->clone());
    QPdfWriter pdf(path);
    pdf.setResolution(96); // device pixels == on-screen pixels, so layout matches the editor exactly
    pdf.setPageSize(QPageSize(QSizeF(ps.widthMm, ps.heightMm), QPageSize::Millimeter));
    pdf.setPageOrientation(ps.landscape ? QPageLayout::Landscape : QPageLayout::Portrait);
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    pdf.setCreator(QStringLiteral("MyOffice MyWord"));

    const QSizeF content = ps.contentSizePx();
    const QMarginsF m = ps.marginsPx();
    doc->documentLayout()->setPaintDevice(&pdf);
    doc->setPageSize(content);

    QPainter p;
    if (!p.begin(&pdf)) { if (error) *error = QObject::tr("Cannot write %1").arg(path); return false; }
    const int pages = qMax(1, doc->pageCount());
    for (int i = 0; i < pages; ++i) {
        if (i > 0) pdf.newPage();
        p.save();
        p.translate(m.left(), m.top() - i * content.height());
        const QRectF clip(0, i * content.height(), content.width(), content.height());
        p.setClipRect(clip);
        doc->drawContents(&p, clip);
        p.restore();
        if (ps.pageNumbers) {
            p.setPen(QColor(90, 90, 90));
            p.setFont(QFont(QStringLiteral("Arial"), 9));
            p.drawText(QRectF(m.left(), ps.pageSizePx().height() - m.bottom() * 0.7, content.width(), 20),
                       Qt::AlignHCenter | Qt::AlignTop, QString::number(i + 1));
        }
    }
    p.end();
    return QFile::exists(path);
}

}

#pragma once
#include <QByteArray>
#include <QString>

#include "myword/document/pagesetup.h"

class QTextDocument;

// Native DOCX (Office Open XML) reader/writer built on Qt's zip support.
// Supported: paragraphs, runs (font, size, bold/italic/underline/strike, colour, highlight),
// alignment, indents, spacing, headings, bullet/numbered lists, tables (with spans), hyperlinks (read),
// page breaks, page size/margins. Not yet: images, headers/footers, styles inheritance, comments.
namespace DocxIO {
QByteArray write(const QTextDocument *doc, const PageSetup &ps);
bool read(const QByteArray &zipData, QTextDocument *doc, PageSetup *ps, QString *error);
}

#pragma once
#include <QString>

#include "myword/document/pagesetup.h"

class QTextDocument;

namespace PdfExport {
// Paginates the document to the page setup and writes a vector PDF. Returns false with `error` set on failure.
bool write(const QTextDocument *doc, const PageSetup &ps, const QString &path, QString *error);
}

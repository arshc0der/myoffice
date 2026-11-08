#pragma once
#include <QByteArray>

class QTextDocument;

// HTML with images embedded as data: URIs so a saved page is self-contained.
namespace HtmlIO {
QByteArray write(const QTextDocument *doc);
void read(const QByteArray &data, QTextDocument *doc);
}

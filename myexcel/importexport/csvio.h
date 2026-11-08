#pragma once
#include <QByteArray>
#include <QString>
#include <vector>

class Sheet;

namespace CsvIO {
// RFC 4180 parser (quoted fields, embedded newlines/quotes); delimiter auto-detected (, ; tab).
std::vector<std::vector<QString>> parse(const QString &text, QChar delimiter);
QChar detectDelimiter(const QString &text);
bool read(const QByteArray &data, Sheet &sheet, QString *error);
QByteArray write(const Sheet &sheet); // writes displayed values
}

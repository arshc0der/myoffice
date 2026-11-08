#include "filemanager.h"

#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {
const char *kRecentKey = "files/recent";
bool isZipBased(const QString &ext) { return ext == "docx" || ext == "xlsx" || ext == "pptx"; }
}

FileManager &FileManager::instance()
{
    static FileManager manager;
    return manager;
}

QStringList FileManager::recentFiles() const { return QSettings().value(kRecentKey).toStringList(); }

void FileManager::addRecent(const QString &path)
{
    const QString canonical = QFileInfo(path).absoluteFilePath();
    QStringList list = recentFiles();
    list.removeAll(canonical);
    list.prepend(canonical);
    while (list.size() > kMaxRecent) list.removeLast();
    QSettings().setValue(kRecentKey, list);
    emit recentFilesChanged();
}

void FileManager::removeRecent(const QString &path)
{
    QStringList list = recentFiles();
    if (list.removeAll(QFileInfo(path).absoluteFilePath()) > 0) {
        QSettings().setValue(kRecentKey, list);
        emit recentFilesChanged();
    }
}

void FileManager::clearRecent()
{
    QSettings().remove(kRecentKey);
    emit recentFilesChanged();
}

bool FileManager::looksLikeText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    return !f.read(8192).contains('\0'); // binary files almost always contain NUL bytes
}

FileManager::Route FileManager::route(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) return { false, {}, tr("The file does not exist.") };
    if (!fi.isReadable()) return { false, {}, tr("You do not have permission to read this file.") };
    if (fi.size() > kMaxFileBytes) return { false, {}, tr("The file is larger than the 2 GB limit.") };

    const QString ext = fi.suffix().toLower();
    std::optional<ApplicationManager::AppId> app;
    for (const auto &info : ApplicationManager::apps())
        if (info.extensions.contains(ext)) { app = info.id; break; }

    if (!app) {
        if (ext == "rtf" || ext == "doc" || ext == "xls" || ext == "ppt" || ext == "odt" || ext == "pdf")
            return { false, {}, tr("\"%1\" files are not supported yet.").arg(ext) };
        if (!looksLikeText(path)) return { false, {}, tr("This file type is not supported.") };
        app = ApplicationManager::AppId::Word; // plain-text fallback
    }
    if (isZipBased(ext)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return { false, {}, f.errorString() };
        if (f.read(4) != QByteArray("PK\x03\x04", 4))
            return { false, {}, tr("The file is corrupted or is not a valid Office document.") };
    }
    return { true, *app, {} };
}

QString FileManager::allSupportedFilter()
{
    QStringList patterns;
    for (const auto &info : ApplicationManager::apps())
        for (const QString &e : info.extensions) patterns << "*." + e;
    return tr("All supported files (%1);;All files (*)").arg(patterns.join(' '));
}

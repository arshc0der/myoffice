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

QStringList FileManager::recentFiles() const
{
    return QSettings().value(kRecentKey).toStringList();
}

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

FileManager::ValidationResult FileManager::validateForOpen(const QString &path,
                                                           ApplicationManager::AppId app)
{
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) return { false, tr("The file does not exist.") };
    if (!fi.isReadable()) return { false, tr("You do not have permission to read this file.") };
    if (fi.size() > kMaxFileBytes) return { false, tr("The file is larger than the 2 GB limit.") };

    const QString ext = fi.suffix().toLower();
    if (!ApplicationManager::info(app).extensions.contains(ext))
        return { false, tr("\"%1\" files are not supported by %2.")
                            .arg(ext, ApplicationManager::info(app).name) };

    if (isZipBased(ext)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return { false, f.errorString() };
        if (f.read(4) != QByteArray("PK\x03\x04", 4))
            return { false, tr("The file is corrupted or is not a valid Office document.") };
    }
    return { true, {} };
}

QString FileManager::openFilter(ApplicationManager::AppId app)
{
    QStringList patterns;
    for (const QString &e : ApplicationManager::info(app).extensions) patterns << "*." + e;
    return tr("Supported files (%1);;All files (*)").arg(patterns.join(' '));
}

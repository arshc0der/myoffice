#pragma once
#include <QObject>
#include <QStringList>

#include "applicationmanager.h"

// Shared file layer: recent files, open-time validation and file dialog filters.
class FileManager : public QObject
{
    Q_OBJECT
public:
    struct ValidationResult {
        bool ok = false;
        QString error;
    };

    static constexpr qint64 kMaxFileBytes = 2LL * 1024 * 1024 * 1024; // 2 GiB hard limit
    static constexpr int kMaxRecent = 20;

    static FileManager &instance();

    QStringList recentFiles() const;
    void addRecent(const QString &path);
    void removeRecent(const QString &path);
    void clearRecent();

    // Checks existence, permissions, size, extension and (for Office Open XML) the ZIP signature.
    static ValidationResult validateForOpen(const QString &path, ApplicationManager::AppId app);
    static QString openFilter(ApplicationManager::AppId app);

signals:
    void recentFilesChanged();

private:
    FileManager() = default;
};

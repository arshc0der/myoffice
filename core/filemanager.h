#pragma once
#include <QObject>
#include <QStringList>
#include <optional>

#include "applicationmanager.h"

// Shared file layer: recent files, routing/validation of files to the right application.
class FileManager : public QObject
{
    Q_OBJECT
public:
    struct Route {
        bool ok = false;
        ApplicationManager::AppId app = ApplicationManager::AppId::Word;
        QString error;
    };

    static constexpr qint64 kMaxFileBytes = 2LL * 1024 * 1024 * 1024; // 2 GiB hard limit
    static constexpr int kMaxRecent = 20;

    static FileManager &instance();

    QStringList recentFiles() const;
    void addRecent(const QString &path);
    void removeRecent(const QString &path);
    void clearRecent();

    // Works out which app should open `path` and validates it (exists, readable, size,
    // ZIP signature for docx/xlsx/pptx). Unknown extensions open in MyWord if they look like text.
    static Route route(const QString &path);
    static QString allSupportedFilter();
    static bool looksLikeText(const QString &path);

signals:
    void recentFilesChanged();

private:
    FileManager() = default;
};

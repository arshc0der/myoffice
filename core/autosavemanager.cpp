#include "autosavemanager.h"

#include <QDir>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent>

AutosaveManager &AutosaveManager::instance()
{
    static AutosaveManager manager;
    return manager;
}

AutosaveManager::AutosaveManager()
{
    m_timer.setInterval(60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, &AutosaveManager::tick);
}

void AutosaveManager::setIntervalSeconds(int seconds) { m_timer.setInterval(qMax(5, seconds) * 1000); }

QString AutosaveManager::recoveryDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/recovery");
}

QString AutosaveManager::pathFor(const QString &id) const
{
    QString safe = id;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    return recoveryDir() + '/' + safe + QStringLiteral(".autosave");
}

void AutosaveManager::registerDocument(const QString &id, SnapshotProvider provider, DirtyCheck isDirty)
{
    m_docs.insert(id, { std::move(provider), std::move(isDirty) });
    if (!m_timer.isActive()) m_timer.start();
}

void AutosaveManager::unregisterDocument(const QString &id, bool discard)
{
    m_docs.remove(id);
    if (discard) discardRecovery(id);
    if (m_docs.isEmpty()) m_timer.stop();
}

void AutosaveManager::tick()
{
    QDir().mkpath(recoveryDir());
    for (auto it = m_docs.cbegin(); it != m_docs.cend(); ++it) {
        if (!it->isDirty()) continue;
        const QString id = it.key();
        const QString path = pathFor(id);
        const QByteArray data = it->provider(); // UI thread: reads document state
        (void)QtConcurrent::run([this, id, path, data] {
            QSaveFile file(path); // atomic: never leaves a half-written recovery file
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
                const QString err = file.errorString();
                QMetaObject::invokeMethod(this, [this, id, err] { emit autosaveFailed(id, err); },
                                          Qt::QueuedConnection);
            }
        });
    }
}

QStringList AutosaveManager::recoverableIds() const
{
    QStringList ids;
    const auto files = QDir(recoveryDir()).entryInfoList({ "*.autosave" }, QDir::Files);
    for (const QFileInfo &fi : files) ids << fi.completeBaseName();
    return ids;
}

QByteArray AutosaveManager::readRecovery(const QString &id) const
{
    QFile f(pathFor(id));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void AutosaveManager::discardRecovery(const QString &id) { QFile::remove(pathFor(id)); }

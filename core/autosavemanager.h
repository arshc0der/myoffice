#pragma once
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <functional>

// Periodic crash-recovery snapshots. Documents register a snapshot provider and a dirty check;
// snapshots are serialised on the UI thread but written atomically on a worker thread.
class AutosaveManager : public QObject
{
    Q_OBJECT
public:
    using SnapshotProvider = std::function<QByteArray()>;
    using DirtyCheck = std::function<bool()>;

    static AutosaveManager &instance();

    void registerDocument(const QString &id, SnapshotProvider provider, DirtyCheck isDirty);
    void unregisterDocument(const QString &id, bool discardRecovery);
    void setIntervalSeconds(int seconds);

    QString recoveryDir() const;
    QStringList recoverableIds() const;
    QByteArray readRecovery(const QString &id) const;
    void discardRecovery(const QString &id);

signals:
    void autosaveFailed(const QString &id, const QString &error);

private:
    AutosaveManager();
    void tick();
    QString pathFor(const QString &id) const;

    struct Entry { SnapshotProvider provider; DirtyCheck isDirty; };
    QHash<QString, Entry> m_docs;
    QTimer m_timer;
};

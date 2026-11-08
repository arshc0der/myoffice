#include "fileio.h"

#include <QFile>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <utility>

namespace FileIO {
void readAsync(const QString &path, QObject *context, ReadCallback callback)
{
    using Result = std::pair<QByteArray, QString>;
    auto *watcher = new QFutureWatcher<Result>(context);
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, context, [watcher, callback] {
        const Result r = watcher->result();
        watcher->deleteLater();
        callback(r.first, r.second);
    });
    watcher->setFuture(QtConcurrent::run([path]() -> Result {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return Result{ QByteArray(), f.errorString() };
        return Result{ f.readAll(), QString() };
    }));
}
}

#pragma once
#include <QByteArray>
#include <QObject>
#include <functional>

// Asynchronous file reading (worker thread); callbacks run on the UI thread and are dropped
// automatically if `context` is destroyed first.
namespace FileIO {
using ReadCallback = std::function<void(const QByteArray &data, const QString &error)>;
void readAsync(const QString &path, QObject *context, ReadCallback callback);
}

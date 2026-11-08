#include "iconprovider.h"

#include <QFile>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>
#include <QDebug>

namespace Icons {
QPixmap pixmap(const QString &name, const QColor &color, int size, qreal dpr)
{
    static QHash<QString, QPixmap> cache;
    const QString key = name + QLatin1Char('|') + color.name(QColor::HexArgb) + QLatin1Char('|')
                        + QString::number(size) + QLatin1Char('@') + QString::number(dpr);
    if (auto it = cache.constFind(key); it != cache.constEnd()) return *it;

    QPixmap pm;
    // The .qrc lives in resources/ and lists "icons/<name>.svg" under prefix "/", so the resource path is
    // ":/icons/<name>.svg". The older ":/resources/icons/" form is kept as a fallback.
    QFile f(QStringLiteral(":/icons/") + name + QStringLiteral(".svg"));
    if (!f.exists()) f.setFileName(QStringLiteral(":/resources/icons/") + name + QStringLiteral(".svg"));
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray svg = f.readAll();
        svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
        QSvgRenderer renderer(svg);
        const int px = qMax(1, qRound(size * dpr));
        pm = QPixmap(px, px);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setOpacity(color.alphaF());
        renderer.render(&p, QRectF(0, 0, px, px));
        p.end();
        pm.setDevicePixelRatio(dpr);
    } else {
        qWarning() << "Missing icon resource:" << name << "(check resources/resources.qrc is listed in RESOURCES and rebuild with qmake)";
    }
    cache.insert(key, pm);
    return pm;
}
}

#include "recentlist.h"

#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPainter>

#include "core/applicationmanager.h"
#include "core/designtokens.h"
#include "core/filemanager.h"
#include "core/iconprovider.h"
#include "core/thememanager.h"

RecentList::RecentList(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setAccessibleName(tr("Recent files"));
    connect(&FileManager::instance(), &FileManager::recentFilesChanged, this, &RecentList::refresh);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
    refresh();
}

void RecentList::refresh()
{
    m_files = FileManager::instance().recentFiles().mid(0, kMaxRows);
    setFixedHeight(sizeHint().height());
    update();
}

QSize RecentList::sizeHint() const { return { 600, qMax(1, int(m_files.size())) * kRowHeight + 8 }; }
int RecentList::rowAt(const QPoint &p) const { const int r = (p.y() - 4) / kRowHeight; return (p.y() >= 4 && r >= 0 && r < m_files.size()) ? r : -1; }

void RecentList::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_files.isEmpty()) {
        p.setPen(pal.textSecondary);
        p.setFont(theme.uiFont(Design::Type::body));
        p.drawText(rect().adjusted(14, 0, 0, 0), Qt::AlignVCenter, tr("No recent files yet. Files you open appear here."));
        return;
    }
    for (int i = 0; i < m_files.size(); ++i) {
        const QRectF r(0, 4 + i * kRowHeight, width(), kRowHeight - 4);
        if (i == m_hover) { p.setPen(Qt::NoPen); p.setBrush(pal.surfaceHover); p.drawRoundedRect(r, 10, 10); }
        const QFileInfo fi(m_files[i]);
        QColor tint = pal.accent;
        for (const auto &info : ApplicationManager::apps())
            if (info.extensions.contains(fi.suffix().toLower())) tint = info.accent;
        p.drawPixmap(QPointF(r.left() + 14, r.center().y() - 11), Icons::pixmap(QStringLiteral("file-text"), tint, 22, devicePixelRatioF()));
        p.setPen(pal.text);
        p.setFont(theme.uiFont(Design::Type::body, QFont::DemiBold));
        p.drawText(QRectF(r.left() + 48, r.top() + 3, r.width() - 60, 18), Qt::AlignVCenter, fi.fileName());
        p.setPen(pal.textSecondary);
        p.setFont(theme.uiFont(Design::Type::caption));
        p.drawText(QRectF(r.left() + 48, r.top() + 21, r.width() - 60, 16), Qt::AlignVCenter,
                   p.fontMetrics().elidedText(QDir::toNativeSeparators(fi.absolutePath()), Qt::ElideMiddle, int(r.width() - 60)));
    }
}

void RecentList::mouseMoveEvent(QMouseEvent *e) { m_hover = rowAt(e->position().toPoint()); setCursor(m_hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
void RecentList::mouseReleaseEvent(QMouseEvent *e)
{
    const int r = rowAt(e->position().toPoint());
    if (e->button() == Qt::LeftButton && r >= 0) emit openRequested(m_files[r]);
}

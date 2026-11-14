#include "recentlist.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
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

QSize RecentList::sizeHint() const { return { 600, kHeaderHeight + qMax(1, int(m_files.size())) * kRowHeight + 8 }; }

int RecentList::rowAt(const QPoint &p) const
{
    const int r = (p.y() - kHeaderHeight) / kRowHeight;
    return (p.y() >= kHeaderHeight && r >= 0 && r < m_files.size()) ? r : -1;
}

void RecentList::paintEvent(QPaintEvent *)
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Column headers, like the Office "Recent" list.
    p.setPen(pal.textSecondary);
    p.setFont(theme.uiFont(Design::Type::caption, QFont::DemiBold));
    p.drawText(QRectF(52, 0, width() - 260, kHeaderHeight), Qt::AlignVCenter | Qt::AlignLeft, tr("Name"));
    p.drawText(QRectF(width() - 200, 0, 190, kHeaderHeight), Qt::AlignVCenter | Qt::AlignLeft, tr("Date modified"));
    p.setPen(pal.border);
    p.drawLine(0, kHeaderHeight - 1, width(), kHeaderHeight - 1);

    if (m_files.isEmpty()) {
        p.setPen(pal.textSecondary);
        p.setFont(theme.uiFont(Design::Type::body));
        p.drawText(QRectF(0, kHeaderHeight, width(), kRowHeight), Qt::AlignVCenter | Qt::AlignLeft, tr("  No recent files yet. Files you open appear here."));
        return;
    }
    for (int i = 0; i < m_files.size(); ++i) {
        const QRectF r(0, kHeaderHeight + i * kRowHeight, width(), kRowHeight);
        if (i == m_hover) { p.setPen(Qt::NoPen); p.setBrush(pal.surfaceHover); p.drawRect(r); }
        const QFileInfo fi(m_files[i]);
        QColor tint = pal.accent;
        for (const auto &info : ApplicationManager::apps())
            if (info.extensions.contains(fi.suffix().toLower())) tint = info.accent;
        p.drawPixmap(QPointF(14, r.center().y() - 11), Icons::pixmap(QStringLiteral("file-text"), tint, 22, devicePixelRatioF()));
        p.setPen(pal.text);
        p.setFont(theme.uiFont(Design::Type::body, QFont::DemiBold));
        p.drawText(QRectF(52, r.top() + 6, r.width() - 270, 20), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(fi.fileName(), Qt::ElideRight, int(r.width() - 270)));
        p.setPen(pal.textSecondary);
        p.setFont(theme.uiFont(Design::Type::caption));
        p.drawText(QRectF(52, r.top() + 27, r.width() - 270, 18), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(QDir::toNativeSeparators(fi.absolutePath()), Qt::ElideMiddle, int(r.width() - 270)));
        p.setFont(theme.uiFont(Design::Type::body));
        p.drawText(QRectF(width() - 200, r.top(), 190, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
                   fi.exists() ? QLocale().toString(fi.lastModified(), QStringLiteral("d MMM yyyy, h:mm")) : tr("File not found"));
        p.setPen(pal.border);
        p.drawLine(52, int(r.bottom()) - 1, width(), int(r.bottom()) - 1);
    }
}

void RecentList::mouseMoveEvent(QMouseEvent *e) { m_hover = rowAt(e->position().toPoint()); setCursor(m_hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
void RecentList::mouseReleaseEvent(QMouseEvent *e)
{
    const int r = rowAt(e->position().toPoint());
    if (e->button() == Qt::LeftButton && r >= 0) emit openRequested(m_files[r]);
}

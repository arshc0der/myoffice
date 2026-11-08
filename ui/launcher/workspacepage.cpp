#include "workspacepage.h"

#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "ui/widgets/appglyph.h"
#include "ui/widgets/themeutil.h"

WorkspacePage::WorkspacePage(QWidget *parent) : QWidget(parent)
{
    auto &theme = ThemeManager::instance();
    m_title = new QLabel(this);
    m_title->setFont(theme.uiFont(Design::Type::display, QFont::DemiBold));
    m_title->setAlignment(Qt::AlignHCenter);
    m_status = new QLabel(this);
    m_status->setFont(theme.uiFont(Design::Type::subtitle));
    m_status->setAlignment(Qt::AlignHCenter);
    m_status->setWordWrap(true);
    m_back = new QPushButton(tr("Back to launcher"), this);
    m_back->setCursor(Qt::PointingHandCursor);
    m_back->setFont(theme.uiFont(Design::Type::body, QFont::DemiBold));
    connect(m_back, &QPushButton::clicked, &ApplicationManager::instance(), &ApplicationManager::goHome);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(Design::Space::xxl, Design::Space::xxl, Design::Space::xxl, Design::Space::xxl);
    col->addStretch(1);
    col->addSpacing(Design::IconSize::xl + Design::Space::xl); // room for the painted glyph
    col->addWidget(m_title);
    col->addWidget(m_status);
    col->addSpacing(Design::Space::lg);
    col->addWidget(m_back, 0, Qt::AlignHCenter);
    col->addStretch(1);

    connect(&theme, &ThemeManager::themeChanged, this, &WorkspacePage::refreshTheme);
    refreshTheme();
}

void WorkspacePage::showApp(ApplicationManager::AppId id)
{
    m_id = id;
    const auto &info = ApplicationManager::info(id);
    static const char *phases[] = { "Phase 2", "Phase 3", "Phase 4" };
    m_title->setText(info.name);
    m_status->setText(tr("The %1 workspace is not implemented yet. It is scheduled for %2 of the build plan.")
                          .arg(info.name, tr(phases[static_cast<int>(id)])));
    refreshTheme();
    update();
}

void WorkspacePage::refreshTheme()
{
    const Palette &pal = ThemeManager::instance().palette();
    setTextColor(m_title, pal.text);
    setTextColor(m_status, pal.textSecondary);
    const QColor accent = ApplicationManager::info(m_id).accent;
    m_back->setStyleSheet(QStringLiteral(
        "QPushButton{background:%1;color:white;border:none;border-radius:%2px;padding:9px 20px;}"
        "QPushButton:hover{background:%3;}QPushButton:focus{outline:2px solid %4;}")
        .arg(accent.name()).arg(Design::Radius::md).arg(accent.darker(112).name(), pal.accent.name()));
}

void WorkspacePage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int s = Design::IconSize::xl;
    const QPoint anchor = m_title->geometry().topLeft();
    paintAppGlyph(&p, QRectF((width() - s) / 2.0, anchor.y() - s - Design::Space::lg, s, s), m_id);
}

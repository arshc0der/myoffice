#include "launcherpage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QTime>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "navrail.h"
#include "ui/widgets/appcard.h"
#include "ui/widgets/pillbutton.h"
#include "ui/widgets/recentlist.h"
#include "ui/widgets/themeutil.h"
#include "ui/widgets/toggleswitch.h"

// Office-style start screen: navigation rail on the left; greeting, "new blank" tiles and the recent
// list on the right.
LauncherPage::LauncherPage(QWidget *parent) : QWidget(parent)
{
    const int hour = QTime::currentTime().hour();
    const QString greeting = hour < 12 ? tr("Good morning") : hour < 18 ? tr("Good afternoon") : tr("Good evening");
    auto &theme = ThemeManager::instance();

    auto *rail = new NavRail(this);
    connect(rail, &NavRail::openRequested, this, &LauncherPage::openFileRequested);
    connect(rail, &NavRail::settingsRequested, this, &LauncherPage::settingsRequested);
    connect(rail, &NavRail::aboutRequested, this, &LauncherPage::aboutRequested);

    m_heading = new QLabel(greeting, this);
    m_heading->setFont(theme.uiFont(22, QFont::DemiBold));
    m_sub = new QLabel(tr("Create a new file"), this);
    m_sub->setFont(theme.uiFont(Design::Type::subtitle, QFont::DemiBold));

    auto *tiles = new QHBoxLayout;
    tiles->setSpacing(Design::Space::md);
    for (const auto &info : ApplicationManager::apps()) {
        auto *card = new AppCard(info.id, this);
        connect(card, &AppCard::activated, this, &LauncherPage::newRequested);
        tiles->addWidget(card);
    }

    m_recentTitle = new QLabel(tr("Recent"), this);
    m_recentTitle->setFont(theme.uiFont(Design::Type::subtitle, QFont::DemiBold));
    m_viewLabel = new QLabel(tr("Open files in view mode first"), this);
    m_viewLabel->setFont(theme.uiFont(Design::Type::body));
    auto *toggle = new ToggleSwitch(tr("Open files in view mode first"), this);
    toggle->setChecked(QSettings().value(QStringLiteral("files/openInViewMode"), true).toBool(), false);
    connect(toggle, &ToggleSwitch::toggled, this, [](bool on) { QSettings().setValue(QStringLiteral("files/openInViewMode"), on); });
    auto *open = new PillButton(tr("Open file…"), QStringLiteral("folder"), false, this);
    connect(open, &PillButton::clicked, this, &LauncherPage::openFileRequested);
    auto *recentHeader = new QHBoxLayout;
    recentHeader->addWidget(m_recentTitle);
    recentHeader->addStretch(1);
    recentHeader->addWidget(m_viewLabel);
    recentHeader->addWidget(toggle);
    recentHeader->addSpacing(Design::Space::lg);
    recentHeader->addWidget(open);

    auto *recent = new RecentList(this);
    connect(recent, &RecentList::openRequested, this, &LauncherPage::openPathRequested);

    m_footer = new QLabel(tr("MyOffice 0.5.0  ·  github.com/arshc0der/myoffice"), this);
    m_footer->setFont(theme.uiFont(Design::Type::caption));

    auto *content = new QWidget;
    content->setAutoFillBackground(false);
    auto *col = new QVBoxLayout(content);
    col->setContentsMargins(44, 32, 44, 16);
    col->setSpacing(Design::Space::md);
    col->addWidget(m_heading);
    col->addSpacing(Design::Space::sm);
    col->addWidget(m_sub);
    col->addLayout(tiles);
    col->addSpacing(Design::Space::lg);
    col->addLayout(recentHeader);
    col->addWidget(recent);
    col->addStretch(1);
    col->addWidget(m_footer);

    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(content);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(rail);
    root->addWidget(scroll, 1);

    connect(&theme, &ThemeManager::themeChanged, this, &LauncherPage::refreshTheme);
    refreshTheme();
}

void LauncherPage::refreshTheme()
{
    const Palette &pal = ThemeManager::instance().palette();
    setTextColor(m_heading, pal.text);
    setTextColor(m_sub, pal.text);
    setTextColor(m_recentTitle, pal.text);
    setTextColor(m_viewLabel, pal.textSecondary);
    setTextColor(m_footer, pal.textSecondary);
}

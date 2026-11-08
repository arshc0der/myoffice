#include "launcherpage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QTime>
#include <QVBoxLayout>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "ui/widgets/appcard.h"
#include "ui/widgets/pillbutton.h"
#include "ui/widgets/recentlist.h"
#include "ui/widgets/themeutil.h"
#include "ui/widgets/toggleswitch.h"

LauncherPage::LauncherPage(QWidget *parent) : QWidget(parent)
{
    const int hour = QTime::currentTime().hour();
    const QString greeting = hour < 12 ? tr("Good morning") : hour < 18 ? tr("Good afternoon") : tr("Good evening");
    auto &theme = ThemeManager::instance();

    m_heading = new QLabel(greeting, this);
    m_heading->setFont(theme.uiFont(Design::Type::display, QFont::DemiBold));
    m_heading->setAlignment(Qt::AlignHCenter);
    m_sub = new QLabel(tr("Create something new, or open any file."), this);
    m_sub->setFont(theme.uiFont(Design::Type::subtitle));
    m_sub->setAlignment(Qt::AlignHCenter);

    auto *cards = new QHBoxLayout;
    cards->setSpacing(Design::Space::sm);
    for (const auto &info : ApplicationManager::apps()) {
        auto *card = new AppCard(info.id, this);
        connect(card, &AppCard::activated, this, &LauncherPage::newRequested);
        cards->addWidget(card);
    }

    auto *open = new PillButton(tr("Open file…"), QStringLiteral("folder"), false, this);
    connect(open, &PillButton::clicked, this, &LauncherPage::openFileRequested);
    auto *toggle = new ToggleSwitch(tr("Open files in view mode first"), this);
    toggle->setChecked(QSettings().value(QStringLiteral("files/openInViewMode"), true).toBool(), false);
    connect(toggle, &ToggleSwitch::toggled, this, [](bool on) { QSettings().setValue(QStringLiteral("files/openInViewMode"), on); });
    m_viewLabel = new QLabel(tr("Open files in view mode first"), this);
    m_viewLabel->setFont(theme.uiFont(Design::Type::body));
    auto *tools = new QHBoxLayout;
    tools->addWidget(open);
    tools->addStretch(1);
    tools->addWidget(m_viewLabel);
    tools->addWidget(toggle);

    m_recentTitle = new QLabel(tr("Recent"), this);
    m_recentTitle->setFont(theme.uiFont(Design::Type::body, QFont::DemiBold));
    auto *recent = new RecentList(this);
    connect(recent, &RecentList::openRequested, this, &LauncherPage::openPathRequested);

    auto *host = new QWidget(this);
    host->setMaximumWidth(1000);
    auto *hostCol = new QVBoxLayout(host);
    hostCol->setContentsMargins(0, 0, 0, 0);
    hostCol->setSpacing(Design::Space::md);
    hostCol->addLayout(cards);
    hostCol->addLayout(tools);
    hostCol->addWidget(m_recentTitle);
    hostCol->addWidget(recent);

    m_footer = new QLabel(tr("MyOffice 0.4.0  ·  MyWord, MyExcel and MyPowerPoint  ·  github.com/arshc0der/myoffice"), this);
    m_footer->setFont(theme.uiFont(Design::Type::caption));
    m_footer->setAlignment(Qt::AlignHCenter);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(Design::Space::xxl, Design::Space::lg, Design::Space::xxl, Design::Space::md);
    col->addStretch(1);
    col->addWidget(m_heading);
    col->addWidget(m_sub);
    col->addSpacing(Design::Space::md);
    col->addWidget(host, 0, Qt::AlignHCenter);
    col->addStretch(2);
    col->addWidget(m_footer);

    connect(&theme, &ThemeManager::themeChanged, this, &LauncherPage::refreshTheme);
    refreshTheme();
}

void LauncherPage::refreshTheme()
{
    const Palette &pal = ThemeManager::instance().palette();
    setTextColor(m_heading, pal.text);
    setTextColor(m_sub, pal.textSecondary);
    setTextColor(m_recentTitle, pal.text);
    setTextColor(m_viewLabel, pal.textSecondary);
    setTextColor(m_footer, pal.textSecondary);
}

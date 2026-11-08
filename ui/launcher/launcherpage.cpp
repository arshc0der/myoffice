#include "launcherpage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTime>
#include <QVBoxLayout>

#include "core/applicationmanager.h"
#include "core/designtokens.h"
#include "core/thememanager.h"
#include "ui/widgets/appcard.h"
#include "ui/widgets/themeutil.h"

LauncherPage::LauncherPage(QWidget *parent) : QWidget(parent)
{
    const int hour = QTime::currentTime().hour();
    const QString greeting = hour < 12 ? tr("Good morning") : hour < 18 ? tr("Good afternoon") : tr("Good evening");
    auto &theme = ThemeManager::instance();

    m_heading = new QLabel(greeting, this);
    m_heading->setFont(theme.uiFont(Design::Type::display, QFont::DemiBold));
    m_heading->setAlignment(Qt::AlignHCenter);
    m_sub = new QLabel(tr("What would you like to create today?"), this);
    m_sub->setFont(theme.uiFont(Design::Type::subtitle));
    m_sub->setAlignment(Qt::AlignHCenter);
    m_footer = new QLabel(tr("MyOffice 0.1.0  ·  Milestone 1: application shell. Editors arrive in the next phases."), this);
    m_footer->setFont(theme.uiFont(Design::Type::caption));
    m_footer->setAlignment(Qt::AlignHCenter);

    auto *cards = new QHBoxLayout;
    cards->setSpacing(Design::Space::sm);
    for (const auto &info : ApplicationManager::apps()) {
        auto *card = new AppCard(info.id, this);
        connect(card, &AppCard::activated, &ApplicationManager::instance(), &ApplicationManager::openApp);
        cards->addWidget(card);
    }
    auto *cardHost = new QWidget(this);
    cardHost->setLayout(cards);
    cardHost->setMaximumWidth(1000);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(Design::Space::xxl, Design::Space::xl, Design::Space::xxl, Design::Space::lg);
    col->addStretch(2);
    col->addWidget(m_heading);
    col->addSpacing(Design::Space::xs);
    col->addWidget(m_sub);
    col->addSpacing(Design::Space::lg);
    col->addWidget(cardHost, 0, Qt::AlignHCenter);
    col->addStretch(3);
    col->addWidget(m_footer);

    connect(&theme, &ThemeManager::themeChanged, this, &LauncherPage::refreshTheme);
    refreshTheme();
}

void LauncherPage::refreshTheme()
{
    const Palette &pal = ThemeManager::instance().palette();
    setTextColor(m_heading, pal.text);
    setTextColor(m_sub, pal.textSecondary);
    setTextColor(m_footer, pal.textSecondary);
}

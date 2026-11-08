#include "statusbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "themeutil.h"

OfficeStatusBar::OfficeStatusBar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(28);
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(14, 0, 10, 0);
    row->setSpacing(16);
    m_left = new QHBoxLayout;
    m_left->setSpacing(18);
    m_right = new QHBoxLayout;
    m_right->setSpacing(6);
    row->addLayout(m_left);
    row->addStretch(1);
    row->addLayout(m_right);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { recolor(); update(); });
}

QLabel *OfficeStatusBar::addLeftText()
{
    auto *l = new QLabel(this);
    l->setFont(ThemeManager::instance().uiFont(Design::Type::caption));
    m_left->addWidget(l);
    m_labels << l;
    recolor();
    return l;
}

void OfficeStatusBar::addRight(QWidget *w) { m_right->addWidget(w); }

void OfficeStatusBar::recolor()
{
    for (QLabel *l : m_labels) setTextColor(l, ThemeManager::instance().palette().chromeText);
}

void OfficeStatusBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), ThemeManager::instance().palette().chrome);
}

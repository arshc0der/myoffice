#include "titlebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>

#include "core/designtokens.h"
#include "core/thememanager.h"
#include "ui/widgets/iconbutton.h"
#include "ui/widgets/themeutil.h"

TitleBar::TitleBar(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(Design::Window::titleBarHeight);
    auto &theme = ThemeManager::instance();

    m_back = new IconButton(IconButton::Glyph::Back, tr("Back to launcher"), this);
    m_logo = new QLabel(this);
    m_brand = new QLabel(QStringLiteral("MyOffice"), this);
    m_context = new QLabel(this);
    m_theme = new IconButton(IconButton::Glyph::Moon, tr("Toggle light/dark theme"), this);
    m_min = new IconButton(IconButton::Glyph::Minimize, tr("Minimize"), this);
    m_max = new IconButton(IconButton::Glyph::Maximize, tr("Maximize"), this);
    m_close = new IconButton(IconButton::Glyph::Close, tr("Close"), this);
    for (QLabel *l : { m_logo, m_brand, m_context }) l->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_back->hide();
    m_context->hide();
    m_brand->setFont(theme.uiFont(Design::Type::body, QFont::DemiBold));
    m_context->setFont(theme.uiFont(Design::Type::body));

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(Design::Space::md, 0, Design::Space::sm, 0);
    row->setSpacing(Design::Space::sm);
    row->addWidget(m_back);
    row->addWidget(m_logo);
    row->addWidget(m_brand);
    row->addWidget(m_context);
    row->addStretch(1);
    row->addWidget(m_theme);
    row->addSpacing(Design::Space::sm);
    row->addWidget(m_min);
    row->addWidget(m_max);
    row->addWidget(m_close);

    connect(m_back, &IconButton::clicked, this, &TitleBar::backRequested);
    connect(m_theme, &IconButton::clicked, &theme, &ThemeManager::toggleLightDark);
    connect(m_min, &IconButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_max, &IconButton::clicked, this, &TitleBar::maximizeToggleRequested);
    connect(m_close, &IconButton::clicked, this, &TitleBar::closeRequested);
    connect(&theme, &ThemeManager::themeChanged, this, &TitleBar::refreshTheme);
    refreshTheme();
}

void TitleBar::refreshTheme()
{
    const auto &theme = ThemeManager::instance();
    const Palette &pal = theme.palette();
    setTextColor(m_brand, pal.text);
    setTextColor(m_context, pal.textSecondary);
    m_theme->setGlyph(theme.isDark() ? IconButton::Glyph::Sun : IconButton::Glyph::Moon);

    // Logo: accent rounded square with an "M", rendered at device resolution.
    const qreal dpr = devicePixelRatioF();
    const int size = 22;
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(pal.accent);
    p.drawRoundedRect(QRectF(0, 0, size, size), 6, 6);
    p.setPen(Qt::white);
    QFont f = theme.uiFont(11, QFont::Bold);
    p.setFont(f);
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("M"));
    p.end();
    m_logo->setPixmap(pm);
}

void TitleBar::setContext(const QString &appName)
{
    m_back->setVisible(!appName.isEmpty());
    m_context->setVisible(!appName.isEmpty());
    m_context->setText(QStringLiteral("›  ") + appName);
}

void TitleBar::setMaximizedState(bool maximized)
{
    m_max->setGlyph(maximized ? IconButton::Glyph::Restore : IconButton::Glyph::Maximize);
    m_max->setAccessibleName(maximized ? tr("Restore") : tr("Maximize"));
}

void TitleBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && window()->windowHandle())
        window()->windowHandle()->startSystemMove(); // native drag: snapping, edge tiling, etc.
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) emit maximizeToggleRequested();
}

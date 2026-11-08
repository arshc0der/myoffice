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

    m_back = new IconButton(QStringLiteral("back"), tr("Back to launcher"), this);
    m_logo = new QLabel(this);
    m_brand = new QLabel(QStringLiteral("MyOffice"), this);
    m_context = new QLabel(this);
    m_theme = new IconButton(QStringLiteral("moon"), tr("Toggle light/dark theme"), this);
    m_min = new IconButton(QStringLiteral("minimize"), tr("Minimize"), this);
    m_max = new IconButton(QStringLiteral("maximize"), tr("Maximize"), this);
    m_close = new IconButton(QStringLiteral("close"), tr("Close"), this);
    for (IconButton *b : { m_back, m_theme, m_min, m_max, m_close }) b->setOnChrome(true);
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
    setTextColor(m_brand, pal.chromeText);
    QColor sec = pal.chromeText;
    sec.setAlphaF(0.75);
    setTextColor(m_context, sec);
    m_theme->setIconName(theme.isDark() ? QStringLiteral("sun") : QStringLiteral("moon"));

    const qreal dpr = devicePixelRatioF();
    const int size = 22;
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(theme.inApp() ? QColor(Qt::white) : pal.accent);
    p.drawRoundedRect(QRectF(0, 0, size, size), 6, 6);
    p.setPen(theme.inApp() ? pal.chrome : QColor(Qt::white));
    p.setFont(theme.uiFont(11, QFont::Bold));
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("M"));
    p.end();
    m_logo->setPixmap(pm);
    update();
}

void TitleBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ThemeManager::instance().palette().chrome);
    p.setClipRect(rect());
    p.drawRoundedRect(QRectF(0, 0, width(), height() + m_radius), m_radius, m_radius);
}

void TitleBar::setContext(const QString &appName)
{
    m_back->setVisible(!appName.isEmpty());
    m_context->setVisible(!appName.isEmpty());
    m_context->setText(QStringLiteral("›  ") + appName);
}

void TitleBar::setMaximizedState(bool maximized)
{
    m_max->setIconName(maximized ? QStringLiteral("restore") : QStringLiteral("maximize"));
}

void TitleBar::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && window()->windowHandle()) window()->windowHandle()->startSystemMove();
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) emit maximizeToggleRequested();
}

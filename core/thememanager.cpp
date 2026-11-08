#include "thememanager.h"

#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ThemeManager &ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager()
{
    m_mode = static_cast<Mode>(QSettings().value(QStringLiteral("appearance/mode"), 0).toInt());
    m_reducedMotion = detectReducedMotion();
    apply();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_mode == Mode::System) apply();
    });
}

bool ThemeManager::detectReducedMotion() const
{
    if (qEnvironmentVariableIsSet("MYOFFICE_REDUCE_MOTION")) return true;
    if (QSettings().value(QStringLiteral("appearance/reduceMotion"), false).toBool()) return true;
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    if (SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0) && !enabled) return true;
#endif
    return false;
}

void ThemeManager::setMode(Mode mode)
{
    m_mode = mode;
    QSettings().setValue(QStringLiteral("appearance/mode"), static_cast<int>(mode));
    apply();
}

void ThemeManager::setReducedMotion(bool on)
{
    m_reducedMotion = on;
    QSettings().setValue(QStringLiteral("appearance/reduceMotion"), on);
}

void ThemeManager::toggleLightDark() { setMode(m_dark ? Mode::Light : Mode::Dark); }

void ThemeManager::setActiveApp(std::optional<ApplicationManager::AppId> app)
{
    if (m_app == app) return;
    m_app = app;
    apply();
}

QFont ThemeManager::uiFont(qreal pointSize, QFont::Weight weight) const
{
    QFont f = QGuiApplication::font();
    f.setPointSizeF(pointSize);
    f.setWeight(weight);
    return f;
}

void ThemeManager::apply()
{
    m_dark = m_mode == Mode::Dark
          || (m_mode == Mode::System && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    Palette &p = m_palette;
    p.window = m_dark ? QColor("#16181D") : QColor("#F3F4F7");
    p.surface = m_dark ? QColor("#20232A") : QColor("#FFFFFF");
    p.surfaceHover = m_dark ? QColor("#2B303A") : QColor("#EBEEF4");
    p.border = m_dark ? QColor("#343A47") : QColor("#DDE1E9");
    p.text = m_dark ? QColor("#ECEEF3") : QColor("#1B1E27");
    p.textSecondary = m_dark ? QColor("#9AA1B0") : QColor("#667085");
    p.danger = QColor("#E5484D");
    p.canvas = m_dark ? QColor("#101216") : QColor("#E4E7EE");
    QColor accent = m_dark ? QColor("#7C93FF") : QColor("#4F6BED");
    if (m_app) {
        accent = ApplicationManager::info(*m_app).accent;
        if (m_dark) accent = accent.lighter(125);
        p.chrome = m_dark ? ApplicationManager::info(*m_app).accent.darker(170) : ApplicationManager::info(*m_app).accent;
        p.chromeText = Qt::white;
    } else {
        p.chrome = p.window;
        p.chromeText = p.text;
    }
    p.accent = accent;
    p.accentDeep = accent.darker(125);

    // Make stock widgets (combo boxes, menus, dialogs, message boxes) follow the theme too.
    QPalette qp;
    qp.setColor(QPalette::Window, p.window);
    qp.setColor(QPalette::WindowText, p.text);
    qp.setColor(QPalette::Base, p.surface);
    qp.setColor(QPalette::AlternateBase, p.surfaceHover);
    qp.setColor(QPalette::Text, p.text);
    qp.setColor(QPalette::Button, p.surfaceHover);
    qp.setColor(QPalette::ButtonText, p.text);
    qp.setColor(QPalette::Highlight, p.accent);
    qp.setColor(QPalette::HighlightedText, Qt::white);
    qp.setColor(QPalette::ToolTipBase, p.surface);
    qp.setColor(QPalette::ToolTipText, p.text);
    qp.setColor(QPalette::PlaceholderText, p.textSecondary);
    qp.setColor(QPalette::Link, p.accent);
    for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        qp.setColor(QPalette::Disabled, role, p.textSecondary);
    QGuiApplication::setPalette(qp);
    emit themeChanged();
}

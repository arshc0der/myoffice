#include "thememanager.h"

#include <QGuiApplication>
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
    QSettings s;
    m_mode = static_cast<Mode>(s.value(QStringLiteral("appearance/mode"), 0).toInt());
    m_reducedMotion = detectReducedMotion();
    apply();
    // Follow the OS when the user chose "System".
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_mode == Mode::System) apply();
    });
}

bool ThemeManager::detectReducedMotion() const
{
    if (qEnvironmentVariableIsSet("MYOFFICE_REDUCE_MOTION")) return true;
    if (QSettings().value(QStringLiteral("appearance/reduceMotion"), false).toBool()) return true;
#ifdef Q_OS_WIN
    BOOL enabled = TRUE; // SPI_GETCLIENTAREAANIMATION is FALSE when the user disabled animations
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

void ThemeManager::toggleLightDark() { setMode(m_dark ? Mode::Light : Mode::Dark); }

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
          || (m_mode == Mode::System
              && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    if (m_dark) {
        m_palette = { QColor("#16181D"), QColor("#20232A"), QColor("#292D36"), QColor("#323744"),
                      QColor("#ECEEF3"), QColor("#9AA1B0"), QColor("#7C93FF"), QColor("#E5484D") };
    } else {
        m_palette = { QColor("#F3F4F7"), QColor("#FFFFFF"), QColor("#EEF0F5"), QColor("#E2E5EC"),
                      QColor("#1B1E27"), QColor("#6B7280"), QColor("#4F6BED"), QColor("#E5484D") };
    }
    emit themeChanged();
}

#pragma once
#include <QColor>
#include <QFont>
#include <QObject>
#include <optional>

#include "applicationmanager.h"

struct Palette {
    QColor window, surface, surfaceHover, border, text, textSecondary, accent, accentDeep, danger,
        canvas, chrome, chromeText;
};

// Light/dark/system mode, per-application colour identity and the reduced-motion preference.
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum class Mode { System, Light, Dark };
    static ThemeManager &instance();

    const Palette &palette() const { return m_palette; }
    bool isDark() const { return m_dark; }
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    void toggleLightDark();

    // While an app is active its accent colours the title bar, ribbon, status bar and controls.
    void setActiveApp(std::optional<ApplicationManager::AppId> app);
    bool inApp() const { return m_app.has_value(); }

    bool reducedMotion() const { return m_reducedMotion; }
    int duration(int ms) const { return m_reducedMotion ? 0 : ms; }
    QFont uiFont(qreal pointSize, QFont::Weight weight = QFont::Normal) const;

signals:
    void themeChanged();

private:
    ThemeManager();
    void apply();
    bool detectReducedMotion() const;

    Mode m_mode = Mode::System;
    std::optional<ApplicationManager::AppId> m_app;
    bool m_dark = false;
    bool m_reducedMotion = false;
    Palette m_palette;
};

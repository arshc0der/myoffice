#pragma once
#include <QColor>
#include <QFont>
#include <QObject>

struct Palette {
    QColor window, surface, surfaceHover, border, text, textSecondary, accent, danger;
};

// Owns light/dark/system mode, the active palette and the reduced-motion preference.
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

    bool reducedMotion() const { return m_reducedMotion; }
    // Returns 0 when reduced motion is on, so animations complete instantly.
    int duration(int ms) const { return m_reducedMotion ? 0 : ms; }
    QFont uiFont(qreal pointSize, QFont::Weight weight = QFont::Normal) const;

signals:
    void themeChanged();

private:
    ThemeManager();
    void apply();
    bool detectReducedMotion() const;

    Mode m_mode = Mode::System;
    bool m_dark = false;
    bool m_reducedMotion = false;
    Palette m_palette;
};

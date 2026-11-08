#pragma once
#include <QColor>
#include <QObject>
#include <QStringList>

// Registry of the suite's applications and the current navigation state.
class ApplicationManager : public QObject
{
    Q_OBJECT
public:
    enum class AppId { Word, Excel, PowerPoint };

    struct AppInfo {
        AppId id;
        QString name;
        QString tagline;
        QString detail;
        QColor accent;
        QStringList extensions; // lower-case, no dot
        QString noun;           // "Document", "Workbook", ...
    };

    static ApplicationManager &instance();
    static const QList<AppInfo> &apps();
    static const AppInfo &info(AppId id);
    static bool isImplemented(AppId id);

    bool atHome() const { return m_atHome; }
    AppId currentApp() const { return m_current; }

    void openApp(AppId id);
    void goHome();

signals:
    void navigationChanged();

private:
    ApplicationManager() = default;
    bool m_atHome = true;
    AppId m_current = AppId::Word;
};

#include "applicationmanager.h"

ApplicationManager &ApplicationManager::instance()
{
    static ApplicationManager manager;
    return manager;
}

const QList<ApplicationManager::AppInfo> &ApplicationManager::apps()
{
    static const QList<AppInfo> list = {
        { AppId::Word, QStringLiteral("MyWord"), QStringLiteral("Documents"),
          QStringLiteral("Write reports, letters and notes with rich formatting, tables and images."),
          QColor("#2B6CE6"),
          { "docx", "html", "htm", "txt", "md", "markdown", "log", "json", "xml", "ini", "cfg" },
          QStringLiteral("Document") },
        { AppId::Excel, QStringLiteral("MyExcel"), QStringLiteral("Spreadsheets"),
          QStringLiteral("Organise data, build formulas and analyse numbers across sheets."),
          QColor("#1E9E62"), { "csv", "xlsx" }, QStringLiteral("Workbook") },
        { AppId::PowerPoint, QStringLiteral("MyPowerPoint"), QStringLiteral("Presentations"),
          QStringLiteral("Design slides, add shapes and media, and present full screen."),
          QColor("#E8663A"), { "pptx" }, QStringLiteral("Presentation") },
    };
    return list;
}

const ApplicationManager::AppInfo &ApplicationManager::info(AppId id)
{
    return apps().at(static_cast<int>(id));
}

bool ApplicationManager::isImplemented(AppId) { return true; }

void ApplicationManager::openApp(AppId id)
{
    if (!m_atHome && m_current == id) return;
    m_current = id;
    m_atHome = false;
    emit navigationChanged();
}

void ApplicationManager::goHome()
{
    if (m_atHome) return;
    m_atHome = true;
    emit navigationChanged();
}

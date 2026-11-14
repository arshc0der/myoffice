#pragma once
#include <QWidget>

#include "core/applicationmanager.h"

class QLabel;
class RecentList;

class LauncherPage : public QWidget
{
    Q_OBJECT
public:
    explicit LauncherPage(QWidget *parent = nullptr);

signals:
    void newRequested(ApplicationManager::AppId id);
    void openFileRequested();
    void openPathRequested(const QString &path);
    void aboutRequested();
    void settingsRequested();

private:
    void refreshTheme();
    QLabel *m_heading, *m_sub, *m_recentTitle, *m_viewLabel, *m_footer;
};

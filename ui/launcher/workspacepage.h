#pragma once
#include <QWidget>

#include "core/applicationmanager.h"

class QLabel;
class QPushButton;

// Navigation target for each app. Clearly marked as not implemented until its phase lands.
class WorkspacePage : public QWidget
{
    Q_OBJECT
public:
    explicit WorkspacePage(QWidget *parent = nullptr);
    void showApp(ApplicationManager::AppId id);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void refreshTheme();
    ApplicationManager::AppId m_id = ApplicationManager::AppId::Word;
    QLabel *m_title, *m_status;
    QPushButton *m_back;
};

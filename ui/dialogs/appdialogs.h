#pragma once
#include <QString>

class QWidget;

// Small modal dialogs shared by the whole suite.
namespace AppDialogs {
inline const QString kRepoUrl = QStringLiteral("https://github.com/arshc0der/myoffice");
void showAbout(QWidget *parent);
void showSettings(QWidget *parent);
void showShortcuts(QWidget *parent);
}

#include <QApplication>
#include <QFontDatabase>

#include "core/thememanager.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MyOffice"));
    QApplication::setOrganizationName(QStringLiteral("MyOffice"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // Fusion gives identical behaviour on every platform; all branded visuals are custom painted.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSizeF(10.5);
    QApplication::setFont(font);

    ThemeManager::instance(); // construct after QApplication
    MainWindow window;
    window.show();
    return app.exec();
}

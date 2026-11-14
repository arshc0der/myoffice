#include <QApplication>
#include <QFontDatabase>
#include <QTimer>

#include "core/thememanager.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MyOffice"));
    QApplication::setOrganizationName(QStringLiteral("MyOffice"));
    QApplication::setApplicationVersion(QStringLiteral("0.5.0"));

    QApplication::setStyle(QStringLiteral("Fusion"));
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSizeF(10.5);
    QApplication::setFont(font);

    ThemeManager::instance();
    MainWindow window;
    window.show();
    const QStringList args = QApplication::arguments().mid(1); // files passed on the command line open in view mode
    QTimer::singleShot(0, &window, [&window, args] {
        window.checkRecovery();
        if (!args.isEmpty()) window.openFiles(args);
    });
    return app.exec();
}

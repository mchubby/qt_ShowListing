#include <QApplication>
#include <QSettings>
#include <QDir>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow mainWin(app);

    mainWin.show();
    if (app.arguments().size() > 1) {
        mainWin.openPath(QDir::fromNativeSeparators(app.arguments().at(1)));
    }

    return app.exec();
}

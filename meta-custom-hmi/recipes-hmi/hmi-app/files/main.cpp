#include <QApplication>
#include <QCursor>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Configure default embedded environment if not already set
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "linuxfb:fb=/dev/fb0");
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_GENERIC_PLUGINS")) {
        qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtouch:/dev/input/touchscreen0");
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS")) {
        qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "/dev/input/touchscreen0");
    }

    QApplication app(argc, argv);

    MainWindow w;
    w.setWindowFlags(Qt::FramelessWindowHint);
    w.resize(1024, 600);
    w.showFullScreen();

    return app.exec();
}

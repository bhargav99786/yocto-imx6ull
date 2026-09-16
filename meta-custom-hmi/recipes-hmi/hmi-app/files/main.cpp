#include <QApplication>
#include <QCursor>
#include <QStackedWidget>
#include "loginwindow.h"
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

    // ── Stacked widget: page 0 = Login, page 1 = HMI Dashboard ──
    QStackedWidget stack;
    stack.setWindowFlags(Qt::FramelessWindowHint);
    stack.resize(1024, 600);

    LoginWindow *login = new LoginWindow(&stack);
    MainWindow  *hmi   = new MainWindow(&stack);

    stack.addWidget(login);   // index 0
    stack.addWidget(hmi);     // index 1
    stack.setCurrentIndex(0); // show login first

    // On successful login: switch to HMI dashboard
    QObject::connect(login, &LoginWindow::loginSuccess, [&stack]() {
        stack.setCurrentIndex(1);
    });

    stack.showFullScreen();

    return app.exec();
}

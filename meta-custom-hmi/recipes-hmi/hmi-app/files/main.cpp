#include <QApplication>
#include <QCursor>
#include <QStackedWidget>
#include <QFontDatabase>
#include <QFont>
#include <QFile>
#include <QDir>
#include "loginwindow.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // 1. Configure LinuxFB platform if not set
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "linuxfb:fb=/dev/fb0");
    }

    // 2. Configure evdevtouch with rotate=90:invertx for 1024x600 Goodix digitizer
    if (!qEnvironmentVariableIsSet("QT_QPA_GENERIC_PLUGINS")) {
        qputenv("QT_QPA_GENERIC_PLUGINS", "evdevtouch:/dev/input/touchscreen0:rotate=90:invertx");
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS")) {
        qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "/dev/input/touchscreen0:rotate=90:invertx");
    }

    // 3. Configure font directory for Qt QBasicFontDatabase
    if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR")) {
        if (QDir("/usr/share/fonts/truetype/dejavu").exists()) {
            qputenv("QT_QPA_FONTDIR", "/usr/share/fonts/truetype/dejavu");
        } else {
            qputenv("QT_QPA_FONTDIR", "/usr/share/fonts/truetype");
        }
    }

    QApplication app(argc, argv);

    // Explicitly register DejaVu Sans fonts from system font paths
    const QStringList fontFiles = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/DejaVuSans-Bold.ttf",
        "/usr/lib/fonts/DejaVuSans.ttf",
        "/usr/lib/fonts/DejaVuSans-Bold.ttf"
    };
    for (const QString &f : fontFiles) {
        if (QFile::exists(f)) {
            QFontDatabase::addApplicationFont(f);
        }
    }
    app.setFont(QFont("DejaVu Sans", 11));

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

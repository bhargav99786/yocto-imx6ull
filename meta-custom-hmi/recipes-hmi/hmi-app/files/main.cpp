#include <QApplication>
#include <QCursor>
#include <QStackedWidget>
#include <QFontDatabase>
#include <QFont>
#include <QFile>
#include <QDir>
#include "loginwindow.h"
#include "mainwindow.h"
#include "touchcalibration.h"
#include "calibrationdialog.h"

int main(int argc, char *argv[])
{
    // 1. Configure LinuxFB platform if not set
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "linuxfb:fb=/dev/fb0");
    }

    // 2. Configure evdevtouch for 1024x600 Goodix digitizer (native 1:1 mapping)
    const char *touchDev = "/dev/input/touchscreen0";
    if (!QFile::exists(touchDev) && QFile::exists("/dev/input/event0")) {
        touchDev = "/dev/input/event0";
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_GENERIC_PLUGINS")) {
        QByteArray plugin = QByteArray("evdevtouch:") + touchDev;
        qputenv("QT_QPA_GENERIC_PLUGINS", plugin);
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS")) {
        QByteArray param = QByteArray(touchDev);
        qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", param);
    }

    // 3. Configure font directory for Qt QBasicFontDatabase
    if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR")) {
        if (QDir("/usr/share/fonts/truetype").exists()) {
            qputenv("QT_QPA_FONTDIR", "/usr/share/fonts/truetype");
        } else if (QDir("/usr/share/fonts/truetype/dejavu").exists()) {
            qputenv("QT_QPA_FONTDIR", "/usr/share/fonts/truetype/dejavu");
        }
    }

    QApplication app(argc, argv);

    // Install global touch calibration filter to auto-correct all taps
    TouchCalibrationFilter *calibFilter = new TouchCalibrationFilter(&app);
    app.installEventFilter(calibFilter);

    // Auto-launch calibration on first boot (if uncalibrated) or if --calibrate is passed
    bool needCalibrate = !TouchCalibration::instance().isCalibrated() || !QFile::exists("/etc/touch-calibration.json");
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--calibrate") {
            needCalibrate = true;
            break;
        }
    }

    if (needCalibrate) {
        CalibrationDialog dlg;
        dlg.exec();
        if (!QFile::exists("/etc/touch-calibration.json")) {
            TouchCalibration::instance().save();
        }
    }

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

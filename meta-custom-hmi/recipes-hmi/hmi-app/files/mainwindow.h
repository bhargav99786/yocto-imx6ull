#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QSlider>
#include <QPushButton>
#include <QProgressBar>
#include <QTabWidget>
#include <QLineEdit>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>

class TouchCanvas;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateClockAndStats();
    void onTouchCoordinates(int x, int y, bool isDown);
    void onBrightnessChanged(int value);
    void onColorButtonClicked(const QColor &color);
    void onRebootClicked();
    void onShutdownClicked();

    // Network & OTA slots
    void onRenewDhcp();
    void onRunPing();
    void onPingProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onPingReadyRead();
    void onEditPingTarget();
    void onEditServerClicked();
    void onCheckOtaUpdate();
    void onOtaVersionReply(QNetworkReply *reply);
    void onInstallOtaUpdate();
    void updateOtaProgress();

    // Touch Calibration slots
    void onCalibrateTouchClicked();
    void onResetCalibrationClicked();
    void updateCalibrationStatus();

    // GPIO Test slots
    void onToggleLed1();
    void onToggleBlinkTest();
    void onBlinkTimeout();
    void onHeartbeatModeChanged(int idx);
    void onReadGpioPin();
    void onSetGpioHigh();
    void onSetGpioLow();
    void onGpioAutoPollToggled(bool checked);
    void onGpioPollTimeout();

private:
    void setupUi();
    QWidget *createDashboardTab();
    QWidget *createTouchTestTab();
    QWidget *createHardwareControlTab();
    QWidget *createGpioTestTab();
    QWidget *createNetworkOtaTab();
    QWidget *createSystemTab();

    QString getIpAddress();
    float getCpuTemperature();
    QString getSystemUptime();
    void getMemoryUsage(int &totalMb, int &usedMb);
    void updateNetworkTabStats();
    QString getActiveBootBank();
    QString getInstalledOtaVersion();
    void loadOtaServerConfig();
    void saveOtaServerConfig(const QString &url);

    // Top status bar
    QLabel *m_clockLabel;
    QLabel *m_ipLabel;
    QLabel *m_tempLabel;
    QLabel *m_boardLabel;

    // Tabs
    QTabWidget *m_tabWidget;

    // Touch Test Tab widgets
    TouchCanvas *m_canvas;
    QLabel *m_touchCoordLabel;
    QLabel *m_calibrationStatusLabel;
    QPushButton *m_calibrateBtn;
    QPushButton *m_resetCalibrationBtn;

    // Dashboard widgets
    QLabel *m_uptimeLabel;
    QLabel *m_kernelLabel;
    QLabel *m_cpuModelLabel;
    QProgressBar *m_ramBar;
    QLabel *m_ramTextLabel;

    // Hardware tab widgets
    QSlider *m_brightnessSlider;
    QLabel *m_brightnessValueLabel;
    QPushButton *m_ledToggleBtn;
    bool m_ledState;

    // GPIO Test tab widgets
    QPushButton *m_led1ToggleBtn;
    QLabel *m_led1Lamp;
    bool m_led1State;
    QPushButton *m_blinkTestBtn;
    QTimer *m_blinkTimer;
    bool m_blinkState;
    QComboBox *m_heartbeatCombo;
    QComboBox *m_gpioChipCombo;
    QSpinBox *m_gpioLineSpin;
    QLabel *m_gpioPinLamp;
    QLabel *m_gpioPinStateLabel;
    QCheckBox *m_gpioAutoPollCheck;
    QTimer *m_gpioPollTimer;

    // Network & OTA widgets
    QLabel *m_eth0StatusLabel;
    QLabel *m_eth1StatusLabel;
    QPushButton *m_renewDhcpBtn;
    QLabel *m_dhcpStatusLabel;

    QLineEdit *m_pingTargetEdit;
    QPushButton *m_runPingBtn;
    QLabel *m_pingResultLabel;
    QProcess *m_pingProcess;

    QLabel *m_otaVersionLabel;
    QLabel *m_otaBankLabel;
    QLabel *m_otaServerLabel;
    QPushButton *m_checkUpdateBtn;
    QPushButton *m_installUpdateBtn;
    QProgressBar *m_otaProgressBar;
    QLabel *m_otaStatusLabel;
    QTimer *m_otaProgressTimer;
    QNetworkAccessManager *m_netManager;

    QString m_otaServerUrl;
    QString m_remoteVersion;
    QString m_remoteUpdateUrl;

    QTimer *m_timer;
};

#endif // MAINWINDOW_H

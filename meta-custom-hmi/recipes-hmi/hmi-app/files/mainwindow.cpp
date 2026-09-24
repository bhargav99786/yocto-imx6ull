#include "mainwindow.h"
#include "touchcanvas.h"
#include "numpaddialog.h"
#include "touchcalibration.h"
#include "calibrationdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTabBar>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>
#include <QTableWidget>
#include <QHeaderView>
#include <QScrollBar>

#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_ledState(false)
    , m_led1State(false)
    , m_blinkState(false)
    , m_blinkTimer(nullptr)
    , m_gpioPollTimer(nullptr)
    , m_otaProgressBar(nullptr)
    , m_otaProgressTimer(nullptr)
    , m_pingProcess(nullptr)
    , m_netManager(nullptr)
    , m_sleepStatusLabel(nullptr)
    , m_sleepTimeoutSpin(nullptr)
    , m_uartFd(-1)
    , m_uartNotifier(nullptr)
    , m_uartPortCombo(nullptr)
    , m_uartBaudCombo(nullptr)
    , m_uartOpenCloseBtn(nullptr)
    , m_uartStatusLabel(nullptr)
    , m_uartPinRefLabel(nullptr)
    , m_uartLogEdit(nullptr)
    , m_uartSendEdit(nullptr)
    , m_uartEndingCombo(nullptr)
    , m_uartAutoScrollCheck(nullptr)
    , m_spiDevCombo(nullptr)
    , m_spiModeCombo(nullptr)
    , m_spiSpeedCombo(nullptr)
    , m_spiPinRefLabel(nullptr)
    , m_spiCustomTxEdit(nullptr)
    , m_spiFormatCombo(nullptr)
    , m_spiPresetPatternCombo(nullptr)
    , m_spiLogEdit(nullptr)
    , m_spiResultBadge(nullptr)
{
    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, &MainWindow::onBlinkTimeout);

    m_gpioPollTimer = new QTimer(this);
    connect(m_gpioPollTimer, &QTimer::timeout, this, &MainWindow::onGpioPollTimeout);

    m_otaProgressTimer = new QTimer(this);
    connect(m_otaProgressTimer, &QTimer::timeout, this, &MainWindow::updateOtaProgress);
    m_otaProgressTimer->start(800);

    m_pingProcess = new QProcess(this);
    connect(m_pingProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onPingReadyRead);
    connect(m_pingProcess, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &MainWindow::onPingProcessFinished);

    m_netManager = new QNetworkAccessManager(this);
    connect(m_netManager, &QNetworkAccessManager::finished, this, &MainWindow::onOtaVersionReply);

    loadOtaServerConfig();
    loadSleepConfig();

    setupUi();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::updateClockAndStats);
    m_timer->start(1000);

    updateClockAndStats();
}

MainWindow::~MainWindow()
{
    if (m_uartFd >= 0) {
        if (m_uartNotifier) {
            m_uartNotifier->setEnabled(false);
            delete m_uartNotifier;
            m_uartNotifier = nullptr;
        }
        ::close(m_uartFd);
        m_uartFd = -1;
    }
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    centralWidget->setStyleSheet("background-color: #0b0f14; color: #e6edf3; font-family: sans-serif;");

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(8);

    // ================= TOP STATUS BAR =================
    QFrame *topBar = new QFrame(this);
    topBar->setStyleSheet("background-color: #161f28; border: 1px solid #233242; border-radius: 8px; padding: 4px;");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 4, 12, 4);

    m_boardLabel = new QLabel("i.MX6ULL HMI (1024x600)", this);
    m_boardLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #00d2ff;");

    m_tempLabel = new QLabel("SoC: -- °C", this);
    m_tempLabel->setStyleSheet("font-size: 14px; color: #ff9800; font-weight: bold;");

    m_ipLabel = new QLabel("IP: Scanning...", this);
    m_ipLabel->setStyleSheet("font-size: 14px; color: #4caf50; font-weight: bold;");

    m_clockLabel = new QLabel("00:00:00", this);
    m_clockLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #ffffff;");

    topLayout->addWidget(m_boardLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_tempLabel);
    topLayout->addSpacing(20);
    topLayout->addWidget(m_ipLabel);
    topLayout->addSpacing(20);
    topLayout->addWidget(m_clockLabel);

    mainLayout->addWidget(topBar);

    // ================= MAIN TABS =================
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #233242; background: #111822; border-radius: 8px; }"
        "QTabBar::tab { background: #16202c; color: #8b949e; padding: 6px 9px; font-size: 11px; font-weight: bold; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #1f2d3d; color: #00d2ff; border-bottom: 3px solid #00d2ff; }"
        "QTabBar::tab:hover { background: #1b2636; color: #e6edf3; }"
    );
    m_tabWidget->tabBar()->setExpanding(true);

    m_tabWidget->addTab(createDashboardTab(), "Dashboard");
    m_tabWidget->addTab(createTouchTestTab(), "Touch Test");
    m_tabWidget->addTab(createHardwareControlTab(), "Display && Sleep");
    m_tabWidget->addTab(createGpioTestTab(), "GPIO Test");
    m_tabWidget->addTab(createUartTab(), "UART Console");
    m_tabWidget->addTab(createSpiTestTab(), "SPI Test");
    m_tabWidget->addTab(createNetworkOtaTab(), "Network && OTA");
    m_tabWidget->addTab(createSystemInfoTab(), "Paths && Pinout");
    m_tabWidget->addTab(createSystemTab(), "Power");

    mainLayout->addWidget(m_tabWidget);
}

QWidget *MainWindow::createDashboardTab()
{
    QWidget *tab = new QWidget(this);
    QGridLayout *grid = new QGridLayout(tab);
    grid->setContentsMargins(12, 10, 12, 10);
    grid->setSpacing(10);

    auto makeCard = [](const QString &title, const QString &accentCol) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(title);
        box->setStyleSheet(QString(
            "QGroupBox { font-size: 13px; font-weight: bold; color: %1; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 12px 10px 10px 10px; background-color: #141c26; }"
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
        ).arg(accentCol));
        QVBoxLayout *l = new QVBoxLayout(box);
        l->setContentsMargins(10, 10, 10, 8);
        l->setSpacing(6);
        l->setAlignment(Qt::AlignTop);
        return box;
    };

    m_cpuModelLabel = new QLabel("ARM Cortex-A7 @ 528 MHz\nNXP i.MX6ULL Processor\nSingle Core 32-bit Architecture", tab);
    m_cpuModelLabel->setStyleSheet("font-size: 13px; color: #e6edf3; line-height: 1.4;");

    m_kernelLabel = new QLabel("Linux 6.1.57-fslc (Preempt)\nYocto Kirkstone 4.0 LTS\nRootFS: systemd 250", tab);
    m_kernelLabel->setStyleSheet("font-size: 13px; color: #e6edf3; line-height: 1.4;");

    m_uptimeLabel = new QLabel("Uptime: --", tab);
    m_uptimeLabel->setStyleSheet("font-size: 16px; color: #00d2ff; font-weight: bold;");

    QGroupBox *boxCpu = makeCard("Processor && Architecture", "#00d2ff");
    boxCpu->layout()->addWidget(m_cpuModelLabel);

    QGroupBox *boxOs = makeCard("Operating System && Kernel", "#4caf50");
    boxOs->layout()->addWidget(m_kernelLabel);

    QGroupBox *boxUptime = makeCard("System Uptime", "#ff9800");
    boxUptime->layout()->addWidget(m_uptimeLabel);

    // RAM Card
    QGroupBox *boxRam = makeCard("Memory (RAM) Utilization", "#e040fb");
    QVBoxLayout *ramLayout = static_cast<QVBoxLayout*>(boxRam->layout());
    m_ramBar = new QProgressBar(boxRam);
    m_ramBar->setRange(0, 100);
    m_ramBar->setValue(25);
    m_ramBar->setFixedHeight(22);
    m_ramBar->setTextVisible(false);
    m_ramBar->setStyleSheet(
        "QProgressBar { background-color: #1c2633; border: 1px solid #2a3b4c; border-radius: 4px; }"
        "QProgressBar::chunk { background-color: #e040fb; border-radius: 4px; }"
    );
    m_ramTextLabel = new QLabel("RAM: -- / -- MB", boxRam);
    m_ramTextLabel->setStyleSheet("font-size: 13px; color: #c9d1d9; font-weight: bold;");
    ramLayout->addWidget(m_ramBar);
    ramLayout->addWidget(m_ramTextLabel);

    grid->addWidget(boxCpu, 0, 0);
    grid->addWidget(boxOs, 0, 1);
    grid->addWidget(boxUptime, 1, 0);
    grid->addWidget(boxRam, 1, 1);

    return tab;
}

QWidget *MainWindow::createTouchTestTab()
{
    QWidget *tab = new QWidget(this);
    QHBoxLayout *hLayout = new QHBoxLayout(tab);
    hLayout->setContentsMargins(12, 12, 12, 12);
    hLayout->setSpacing(12);

    // Left sidebar with touch tools
    QFrame *toolsPanel = new QFrame(tab);
    toolsPanel->setFixedWidth(190);
    toolsPanel->setStyleSheet("background-color: #141c26; border: 1px solid #233242; border-radius: 8px; padding: 6px;");
    QVBoxLayout *vTools = new QVBoxLayout(toolsPanel);
    vTools->setContentsMargins(8, 8, 8, 8);
    vTools->setSpacing(8);

    QLabel *toolTitle = new QLabel("Color Palette", toolsPanel);
    toolTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #00d2ff;");
    vTools->addWidget(toolTitle);

    QHBoxLayout *colorRow = new QHBoxLayout();
    colorRow->setSpacing(6);
    colorRow->setContentsMargins(0, 0, 0, 0);

    auto makeColorBtn = [this, colorRow, toolsPanel](const QString &tip, const QColor &c) {
        QPushButton *btn = new QPushButton(toolsPanel);
        btn->setFixedSize(30, 30);
        btn->setToolTip(tip);
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; border: 2px solid #233242; border-radius: 15px; }"
            "QPushButton:hover { border-color: #ffffff; }"
            "QPushButton:pressed { border-color: #00d2ff; }"
        ).arg(c.name()));
        connect(btn, &QPushButton::clicked, this, [this, c]() { onColorButtonClicked(c); });
        colorRow->addWidget(btn);
    };

    makeColorBtn("Cyan", QColor(0, 220, 255));
    makeColorBtn("Yellow", QColor(255, 235, 59));
    makeColorBtn("Green", QColor(76, 175, 80));
    makeColorBtn("Red", QColor(244, 67, 54));
    makeColorBtn("White", QColor(240, 244, 248));
    vTools->addLayout(colorRow);

    QPushButton *clearBtn = new QPushButton("Clear Canvas", toolsPanel);
    clearBtn->setFixedHeight(32);
    clearBtn->setStyleSheet("background-color: #2e3846; color: #ffffff; font-size: 12px; font-weight: bold; border-radius: 6px; border: 1px solid #455a64;");
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_canvas) m_canvas->clearCanvas();
    });
    vTools->addWidget(clearBtn);

    vTools->addSpacing(4);

    QLabel *calibTitle = new QLabel("Touch Calibration", toolsPanel);
    calibTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #ff9800;");
    vTools->addWidget(calibTitle);

    m_calibrateBtn = new QPushButton("Calibrate Screen", toolsPanel);
    m_calibrateBtn->setFixedHeight(34);
    m_calibrateBtn->setStyleSheet("background-color: #e65100; color: #ffffff; font-size: 12px; font-weight: bold; border-radius: 6px;");
    connect(m_calibrateBtn, &QPushButton::clicked, this, &MainWindow::onCalibrateTouchClicked);
    vTools->addWidget(m_calibrateBtn);

    m_resetCalibrationBtn = new QPushButton("Reset to 1:1", toolsPanel);
    m_resetCalibrationBtn->setFixedHeight(28);
    m_resetCalibrationBtn->setStyleSheet("background-color: #21262d; color: #8b949e; font-size: 11px; border-radius: 4px; border: 1px solid #30363d;");
    connect(m_resetCalibrationBtn, &QPushButton::clicked, this, &MainWindow::onResetCalibrationClicked);
    vTools->addWidget(m_resetCalibrationBtn);

    m_calibrationStatusLabel = new QLabel(TouchCalibration::instance().statusString(), toolsPanel);
    m_calibrationStatusLabel->setStyleSheet("font-size: 11px; color: #81c784;");
    m_calibrationStatusLabel->setWordWrap(true);
    vTools->addWidget(m_calibrationStatusLabel);

    connect(&TouchCalibration::instance(), &TouchCalibration::calibrationChanged, this, &MainWindow::updateCalibrationStatus);

    vTools->addStretch();

    m_touchCoordLabel = new QLabel("Touch: Ready\nX: --  Y: --", toolsPanel);
    m_touchCoordLabel->setFixedHeight(44);
    m_touchCoordLabel->setStyleSheet("background: #0d1117; color: #00d2ff; font-family: monospace; font-size: 12px; font-weight: bold; padding: 4px 8px; border-radius: 6px; border: 1px solid #233242;");
    vTools->addWidget(m_touchCoordLabel);

    // Right Canvas
    m_canvas = new TouchCanvas(tab);
    connect(m_canvas, &TouchCanvas::touchCoordinatesChanged, this, &MainWindow::onTouchCoordinates);

    hLayout->addWidget(toolsPanel);
    hLayout->addWidget(m_canvas, 1);

    return tab;
}

QWidget *MainWindow::createHardwareControlTab()
{
    QWidget *tab = new QWidget(this);
    QGridLayout *grid = new QGridLayout(tab);
    grid->setContentsMargins(16, 16, 16, 16);
    grid->setSpacing(16);

    // Backlight Box
    QGroupBox *blBox = new QGroupBox("LCD Backlight Intensity", tab);
    blBox->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #00d2ff; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 14px 12px 12px 12px; background-color: #141c26; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
    );
    QVBoxLayout *blLayout = new QVBoxLayout(blBox);
    blLayout->setAlignment(Qt::AlignTop);
    blLayout->setSpacing(12);

    m_brightnessValueLabel = new QLabel("Brightness: 7 / 7 (100%)", blBox);
    m_brightnessValueLabel->setStyleSheet("font-size: 15px; color: #ffffff; font-weight: bold;");
    blLayout->addWidget(m_brightnessValueLabel);

    m_brightnessSlider = new QSlider(Qt::Horizontal, blBox);
    m_brightnessSlider->setRange(1, 7);
    m_brightnessSlider->setValue(7);
    m_brightnessSlider->setFixedHeight(36);
    m_brightnessSlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 10px; background: #233242; border-radius: 5px; }"
        "QSlider::sub-page:horizontal { background: #00d2ff; border-radius: 5px; }"
        "QSlider::handle:horizontal { background: #ffffff; width: 28px; margin-top: -9px; margin-bottom: -9px; border-radius: 14px; border: 2px solid #00d2ff; }"
    );
    connect(m_brightnessSlider, &QSlider::valueChanged, this, &MainWindow::onBrightnessChanged);
    blLayout->addWidget(m_brightnessSlider);

    // Quick presets
    QHBoxLayout *presetRow = new QHBoxLayout();
    presetRow->setSpacing(8);
    auto makePreset = [this, presetRow, blBox](const QString &label, int val) {
        QPushButton *btn = new QPushButton(label, blBox);
        btn->setFixedHeight(34);
        btn->setStyleSheet("background-color: #1f2d3d; color: #00d2ff; font-weight: bold; font-size: 12px; border: 1px solid #2a3b4c; border-radius: 6px;");
        connect(btn, &QPushButton::clicked, [this, val]() {
            m_brightnessSlider->setValue(val);
        });
        presetRow->addWidget(btn);
    };
    makePreset("25%", 2);
    makePreset("50%", 4);
    makePreset("75%", 5);
    makePreset("Max (100%)", 7);
    blLayout->addLayout(presetRow);
    blLayout->addStretch();

    // Sleep Inactivity & Display Blanking Box
    QGroupBox *sleepBox = new QGroupBox("Display Inactivity Sleep & Touch Wake", tab);
    sleepBox->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #ff9800; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 14px 12px 12px 12px; background-color: #141c26; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
    );
    QVBoxLayout *sleepLayout = new QVBoxLayout(sleepBox);
    sleepLayout->setAlignment(Qt::AlignTop);
    sleepLayout->setSpacing(10);

    m_sleepStatusLabel = new QLabel("Timeout: Loading /etc/hmi-sleep.conf...", sleepBox);
    m_sleepStatusLabel->setStyleSheet("font-size: 14px; color: #ffffff; font-weight: bold;");
    sleepLayout->addWidget(m_sleepStatusLabel);

    // Quick presets
    QHBoxLayout *presetSleepRow = new QHBoxLayout();
    presetSleepRow->setSpacing(6);
    auto makeSleepPreset = [this, presetSleepRow, sleepBox](const QString &label, int secs) {
        QPushButton *btn = new QPushButton(label, sleepBox);
        btn->setFixedHeight(32);
        btn->setStyleSheet("background-color: #1f2d3d; color: #ffb74d; font-weight: bold; font-size: 11px; border: 1px solid #2a3b4c; border-radius: 6px;");
        connect(btn, &QPushButton::clicked, [this, secs]() {
            onSleepPresetClicked(secs);
        });
        presetSleepRow->addWidget(btn);
    };
    makeSleepPreset("Always On", 0);
    makeSleepPreset("30s", 30);
    makeSleepPreset("1 min", 60);
    makeSleepPreset("2 min", 120);
    makeSleepPreset("5 min", 300);
    sleepLayout->addLayout(presetSleepRow);

    // Custom spinbox & apply
    QHBoxLayout *customSleepRow = new QHBoxLayout();
    customSleepRow->setSpacing(8);
    QLabel *customLbl = new QLabel("Custom Timeout:", sleepBox);
    customLbl->setStyleSheet("font-size: 12px; color: #8b949e; font-weight: bold;");
    m_sleepTimeoutSpin = new QSpinBox(sleepBox);
    m_sleepTimeoutSpin->setRange(0, 3600);
    m_sleepTimeoutSpin->setValue(120);
    m_sleepTimeoutSpin->setFixedHeight(32);
    m_sleepTimeoutSpin->setSuffix(" sec");
    m_sleepTimeoutSpin->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 8px; font-size: 12px; font-weight: bold;");

    QPushButton *applySleepBtn = new QPushButton("Save Config", sleepBox);
    applySleepBtn->setFixedHeight(32);
    applySleepBtn->setStyleSheet("background-color: #e65100; color: #ffffff; font-weight: bold; font-size: 12px; border-radius: 6px;");
    connect(applySleepBtn, &QPushButton::clicked, this, &MainWindow::onApplyCustomSleep);

    customSleepRow->addWidget(customLbl);
    customSleepRow->addWidget(m_sleepTimeoutSpin, 1);
    customSleepRow->addWidget(applySleepBtn);
    sleepLayout->addLayout(customSleepRow);

    // Sleep Now test button
    QPushButton *sleepNowBtn = new QPushButton("🌙 Sleep Screen Now (Test Touch Wake)", sleepBox);
    sleepNowBtn->setFixedHeight(34);
    sleepNowBtn->setStyleSheet("background-color: #283593; color: #80d8ff; font-weight: bold; font-size: 12px; border: 1px solid #3949ab; border-radius: 6px;");
    connect(sleepNowBtn, &QPushButton::clicked, this, &MainWindow::onSleepNowClicked);
    sleepLayout->addWidget(sleepNowBtn);

    QLabel *sleepInfo = new QLabel("Display powers off after inactivity. Any touch on screen instantly wakes it up.\nWrites to /etc/hmi-sleep.conf (hmi-sleep-daemon reloads live without restart).", sleepBox);
    sleepInfo->setStyleSheet("font-size: 11px; color: #78909c;");
    sleepInfo->setWordWrap(true);
    sleepLayout->addWidget(sleepInfo);
    sleepLayout->addStretch();

    // GPIO & LED Box
    QGroupBox *gpioBox = new QGroupBox("Industrial I/O && Relay Simulation", tab);
    gpioBox->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #4caf50; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 14px 12px 12px 12px; background-color: #141c26; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
    );
    QVBoxLayout *gpioLayout = new QVBoxLayout(gpioBox);
    gpioLayout->setAlignment(Qt::AlignTop);
    gpioLayout->setSpacing(14);

    QLabel *gpioDesc = new QLabel("Simulated Digital Output (Relay / Onboard LED)", gpioBox);
    gpioDesc->setStyleSheet("font-size: 13px; color: #8b949e;");
    gpioLayout->addWidget(gpioDesc);

    m_ledToggleBtn = new QPushButton("Carrier Board LED / Relay: OFF", gpioBox);
    m_ledToggleBtn->setFixedHeight(46);
    m_ledToggleBtn->setStyleSheet("background-color: #21262d; color: #8b949e; font-size: 14px; font-weight: bold; border-radius: 8px; border: 2px solid #30363d;");
    connect(m_ledToggleBtn, &QPushButton::clicked, this, [this]() {
        m_ledState = !m_ledState;
        if (m_ledState) {
            m_ledToggleBtn->setText("Carrier Board LED / Relay: ON");
            m_ledToggleBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 8px; border: 2px solid #4caf50;");
        } else {
            m_ledToggleBtn->setText("Carrier Board LED / Relay: OFF");
            m_ledToggleBtn->setStyleSheet("background-color: #21262d; color: #8b949e; font-size: 14px; font-weight: bold; border-radius: 8px; border: 2px solid #30363d;");
        }
    });
    gpioLayout->addWidget(m_ledToggleBtn);
    gpioLayout->addStretch();

    grid->addWidget(blBox, 0, 0);
    grid->addWidget(sleepBox, 0, 1);
    grid->addWidget(gpioBox, 1, 0, 1, 2);

    return tab;
}

QWidget *MainWindow::createGpioTestTab()
{
    QWidget *tab = new QWidget(this);
    QHBoxLayout *hLayout = new QHBoxLayout(tab);
    hLayout->setContentsMargins(12, 10, 12, 10);
    hLayout->setSpacing(12);

    auto makeCard = [](const QString &title, const QString &accentCol) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(title);
        box->setStyleSheet(QString(
            "QGroupBox { font-size: 13px; font-weight: bold; color: %1; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 12px 10px 10px 10px; background-color: #141c26; }"
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
        ).arg(accentCol));
        return box;
    };

    // ================= LEFT COLUMN: ONBOARD HARDWARE LEDS =================
    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->setSpacing(10);

    QGroupBox *ledBox = makeCard("Carrier Board Hardware LEDs", "#00d2ff");
    QVBoxLayout *ledLayout = new QVBoxLayout(ledBox);
    ledLayout->setSpacing(10);
    ledLayout->setContentsMargins(8, 8, 8, 8);

    // LED1 section
    QLabel *led1Title = new QLabel("User LED1 (/sys/class/leds/led1 -> gpio1 10):", ledBox);
    led1Title->setStyleSheet("font-size: 12px; color: #8b949e; font-weight: bold;");
    ledLayout->addWidget(led1Title);

    QHBoxLayout *led1Row = new QHBoxLayout();
    m_led1Lamp = new QLabel(ledBox);
    m_led1Lamp->setFixedSize(28, 28);
    m_led1Lamp->setStyleSheet("background-color: #21262d; border: 2px solid #30363d; border-radius: 14px;");

    m_led1ToggleBtn = new QPushButton("Turn LED1 ON", ledBox);
    m_led1ToggleBtn->setFixedHeight(36);
    m_led1ToggleBtn->setStyleSheet("background-color: #1f2d3d; color: #00d2ff; font-weight: bold; font-size: 12px; border: 1px solid #2a3b4c; border-radius: 6px;");
    connect(m_led1ToggleBtn, &QPushButton::clicked, this, &MainWindow::onToggleLed1);

    m_blinkTestBtn = new QPushButton("Start Blink (1Hz)", ledBox);
    m_blinkTestBtn->setFixedHeight(36);
    m_blinkTestBtn->setStyleSheet("background-color: #263238; color: #ffb74d; font-weight: bold; font-size: 12px; border: 1px solid #37474f; border-radius: 6px;");
    connect(m_blinkTestBtn, &QPushButton::clicked, this, &MainWindow::onToggleBlinkTest);

    led1Row->addWidget(m_led1Lamp);
    led1Row->addWidget(m_led1ToggleBtn, 1);
    led1Row->addWidget(m_blinkTestBtn, 1);
    ledLayout->addLayout(led1Row);

    ledLayout->addSpacing(6);

    // Heartbeat LED section
    QLabel *hbTitle = new QLabel("System Heartbeat LED (/sys/class/leds/heartbeat -> gpio5 3):", ledBox);
    hbTitle->setStyleSheet("font-size: 12px; color: #8b949e; font-weight: bold;");
    ledLayout->addWidget(hbTitle);

    QHBoxLayout *hbRow = new QHBoxLayout();
    m_heartbeatCombo = new QComboBox(ledBox);
    m_heartbeatCombo->setFixedHeight(34);
    m_heartbeatCombo->addItem("Kernel Trigger (Heartbeat CPU Pulse)");
    m_heartbeatCombo->addItem("Manual Trigger (Disabled / Off)");
    m_heartbeatCombo->setStyleSheet("QComboBox { background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 4px 8px; font-size: 11px; font-weight: bold; } QComboBox QAbstractItemView { background-color: #141c26; color: #ffffff; selection-background-color: #0288d1; }");
    connect(m_heartbeatCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onHeartbeatModeChanged);

    hbRow->addWidget(m_heartbeatCombo);
    ledLayout->addLayout(hbRow);

    leftCol->addWidget(ledBox);

    // Quick Presets Box
    QGroupBox *presetBox = makeCard("Carrier Board Pin Presets", "#ffd54f");
    QVBoxLayout *presetLayout = new QVBoxLayout(presetBox);
    presetLayout->setSpacing(6);
    presetLayout->setContentsMargins(8, 8, 8, 8);

    QGridLayout *btnGrid = new QGridLayout();
    btnGrid->setSpacing(6);

    auto addPresetBtn = [this, btnGrid](int r, int c, const QString &name, int chip, int line) {
        QPushButton *btn = new QPushButton(name);
        btn->setFixedHeight(30);
        btn->setStyleSheet("background-color: #1a2332; color: #90caf9; font-size: 11px; border: 1px solid #233242; border-radius: 4px;");
        connect(btn, &QPushButton::clicked, this, [this, chip, line]() {
            if (m_gpioChipCombo) m_gpioChipCombo->setCurrentIndex(chip);
            if (m_gpioLineSpin) m_gpioLineSpin->setValue(line);
            onReadGpioPin();
        });
        btnGrid->addWidget(btn, r, c);
    };

    addPresetBtn(0, 0, "LED1 (Chip 0, Line 10)", 0, 10);
    addPresetBtn(0, 1, "CAN 3V3 (Chip 0, Line 18)", 0, 18);
    addPresetBtn(1, 0, "Heartbeat (Chip 4, Line 3)", 4, 3);
    addPresetBtn(1, 1, "Backlight PWM (Chip 0, Line 8)", 0, 8);
    addPresetBtn(2, 0, "UART3 TX (Chip 0, Line 24)", 0, 24);
    addPresetBtn(2, 1, "UART3 RX (Chip 0, Line 25)", 0, 25);
    addPresetBtn(3, 0, "P17-8 GPIO (Chip 3, Line 20)", 3, 20);
    addPresetBtn(3, 1, "P17-9 GPIO (Chip 3, Line 17)", 3, 17);
    presetLayout->addLayout(btnGrid);

    leftCol->addWidget(presetBox);
    leftCol->addStretch();
    hLayout->addLayout(leftCol, 1);

    // ================= RIGHT COLUMN: UNIVERSAL GPIO PIN TEST BENCH =================
    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->setSpacing(10);

    QGroupBox *testBenchBox = makeCard("Universal GPIO Pin Test Bench (libgpiod)", "#4caf50");
    QVBoxLayout *benchLayout = new QVBoxLayout(testBenchBox);
    benchLayout->setSpacing(10);
    benchLayout->setContentsMargins(10, 10, 10, 10);

    // Chip & Line Selector Row
    QHBoxLayout *selRow = new QHBoxLayout();
    QLabel *chipLbl = new QLabel("Chip:", testBenchBox);
    chipLbl->setStyleSheet("color: #b0bec5; font-size: 12px; font-weight: bold;");
    m_gpioChipCombo = new QComboBox(testBenchBox);
    m_gpioChipCombo->setFixedHeight(32);
    m_gpioChipCombo->addItem("gpiochip0 (Bank 1)");
    m_gpioChipCombo->addItem("gpiochip1 (Bank 2)");
    m_gpioChipCombo->addItem("gpiochip2 (Bank 3)");
    m_gpioChipCombo->addItem("gpiochip3 (Bank 4)");
    m_gpioChipCombo->addItem("gpiochip4 (Bank 5)");
    m_gpioChipCombo->setStyleSheet("QComboBox { background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 6px; font-size: 11px; } QComboBox QAbstractItemView { background-color: #141c26; color: #ffffff; }");

    QLabel *lineLbl = new QLabel("Line:", testBenchBox);
    lineLbl->setStyleSheet("color: #b0bec5; font-size: 12px; font-weight: bold;");
    m_gpioLineSpin = new QSpinBox(testBenchBox);
    m_gpioLineSpin->setFixedHeight(32);
    m_gpioLineSpin->setRange(0, 31);
    m_gpioLineSpin->setValue(10);
    m_gpioLineSpin->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 8px; font-size: 13px; font-weight: bold;");

    selRow->addWidget(chipLbl);
    selRow->addWidget(m_gpioChipCombo, 1);
    selRow->addWidget(lineLbl);
    selRow->addWidget(m_gpioLineSpin);
    benchLayout->addLayout(selRow);

    // Visual Pin Status Display Box
    QFrame *statusFrame = new QFrame(testBenchBox);
    statusFrame->setStyleSheet("background-color: #0d131a; border: 1px solid #1e2c3c; border-radius: 8px; padding: 10px;");
    QHBoxLayout *statusRow = new QHBoxLayout(statusFrame);
    statusRow->setContentsMargins(10, 8, 10, 8);

    m_gpioPinLamp = new QLabel(statusFrame);
    m_gpioPinLamp->setFixedSize(36, 36);
    m_gpioPinLamp->setStyleSheet("background-color: #21262d; border: 2px solid #30363d; border-radius: 18px;");

    QVBoxLayout *stateTextLayout = new QVBoxLayout();
    m_gpioPinStateLabel = new QLabel("State: Unknown / Ready", statusFrame);
    m_gpioPinStateLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    QLabel *pinDetail = new QLabel("libgpiod character device direct status", statusFrame);
    pinDetail->setStyleSheet("font-size: 11px; color: #78909c;");
    stateTextLayout->addWidget(m_gpioPinStateLabel);
    stateTextLayout->addWidget(pinDetail);

    statusRow->addWidget(m_gpioPinLamp);
    statusRow->addSpacing(10);
    statusRow->addLayout(stateTextLayout, 1);
    benchLayout->addWidget(statusFrame);

    // Output Controls
    QLabel *outTitle = new QLabel("Digital Output Mode (Write to Pin):", testBenchBox);
    outTitle->setStyleSheet("font-size: 12px; color: #8b949e; font-weight: bold;");
    benchLayout->addWidget(outTitle);

    QHBoxLayout *outRow = new QHBoxLayout();
    QPushButton *setHighBtn = new QPushButton("Set HIGH (1 / 3.3V)", testBenchBox);
    setHighBtn->setFixedHeight(36);
    setHighBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-size: 12px; font-weight: bold; border-radius: 6px;");
    connect(setHighBtn, &QPushButton::clicked, this, &MainWindow::onSetGpioHigh);

    QPushButton *setLowBtn = new QPushButton("Set LOW (0 / 0V)", testBenchBox);
    setLowBtn->setFixedHeight(36);
    setLowBtn->setStyleSheet("background-color: #37474f; color: #cfd8dc; font-size: 12px; font-weight: bold; border-radius: 6px;");
    connect(setLowBtn, &QPushButton::clicked, this, &MainWindow::onSetGpioLow);

    outRow->addWidget(setHighBtn);
    outRow->addWidget(setLowBtn);
    benchLayout->addLayout(outRow);

    // Input Controls
    QLabel *inTitle = new QLabel("Digital Input Mode (Read from Pin):", testBenchBox);
    inTitle->setStyleSheet("font-size: 12px; color: #8b949e; font-weight: bold;");
    benchLayout->addWidget(inTitle);

    QHBoxLayout *inRow = new QHBoxLayout();
    QPushButton *readBtn = new QPushButton("Read Pin (gpioget)", testBenchBox);
    readBtn->setFixedHeight(36);
    readBtn->setStyleSheet("background-color: #0288d1; color: #ffffff; font-size: 12px; font-weight: bold; border-radius: 6px;");
    connect(readBtn, &QPushButton::clicked, this, &MainWindow::onReadGpioPin);

    m_gpioAutoPollCheck = new QCheckBox("Live Auto-Poll (500ms)", testBenchBox);
    m_gpioAutoPollCheck->setStyleSheet("QCheckBox { color: #80cbc4; font-size: 12px; font-weight: bold; } QCheckBox::indicator { width: 18px; height: 18px; }");
    connect(m_gpioAutoPollCheck, &QCheckBox::toggled, this, &MainWindow::onGpioAutoPollToggled);

    inRow->addWidget(readBtn, 1);
    inRow->addWidget(m_gpioAutoPollCheck);
    benchLayout->addLayout(inRow);

    rightCol->addWidget(testBenchBox);
    rightCol->addStretch();
    hLayout->addLayout(rightCol, 1);

    return tab;
}

QWidget *MainWindow::createSystemTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(tab);
    vLayout->setContentsMargins(16, 16, 16, 16);
    vLayout->setSpacing(16);

    QGroupBox *powerBox = new QGroupBox("System Power && Maintenance", tab);
    powerBox->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #00d2ff; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 16px; background-color: #141c26; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
    );
    QVBoxLayout *boxLayout = new QVBoxLayout(powerBox);
    boxLayout->setSpacing(16);
    boxLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *desc = new QLabel("Select an industrial system control action below:", powerBox);
    desc->setStyleSheet("font-size: 13px; color: #8b949e;");
    boxLayout->addWidget(desc);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(20);

    QPushButton *rebootBtn = new QPushButton("Restart System (Reboot)", powerBox);
    rebootBtn->setFixedHeight(48);
    rebootBtn->setStyleSheet("background-color: #f57c00; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px;");
    connect(rebootBtn, &QPushButton::clicked, this, &MainWindow::onRebootClicked);

    QPushButton *shutdownBtn = new QPushButton("Power Off (Shutdown)", powerBox);
    shutdownBtn->setFixedHeight(48);
    shutdownBtn->setStyleSheet("background-color: #d32f2f; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px;");
    connect(shutdownBtn, &QPushButton::clicked, this, &MainWindow::onShutdownClicked);

    btnLayout->addWidget(rebootBtn);
    btnLayout->addWidget(shutdownBtn);
    boxLayout->addLayout(btnLayout);

    vLayout->addWidget(powerBox);
    vLayout->addStretch();

    return tab;
}

QWidget *MainWindow::createUartTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(tab);
    vLayout->setContentsMargins(12, 10, 12, 10);
    vLayout->setSpacing(8);

    // ================= TOP CONFIGURATION PANEL =================
    QFrame *topFrame = new QFrame(tab);
    topFrame->setStyleSheet("background-color: #141c26; border: 1px solid #233242; border-radius: 8px; padding: 6px;");
    QHBoxLayout *topLayout = new QHBoxLayout(topFrame);
    topLayout->setContentsMargins(8, 4, 8, 4);
    topLayout->setSpacing(10);

    QLabel *portLbl = new QLabel("Serial Port:", topFrame);
    portLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #00d2ff;");
    m_uartPortCombo = new QComboBox(topFrame);
    m_uartPortCombo->addItem("UART2 (/dev/ttymxc1) - P17 Pins 32 RX, 34 TX", "/dev/ttymxc1");
    m_uartPortCombo->addItem("UART3 (/dev/ttymxc2) - P17 Pins 31 RX, 33 TX", "/dev/ttymxc2");
    m_uartPortCombo->addItem("UART4 (/dev/ttymxc3) - P17 Pins 26 RX, 28 TX", "/dev/ttymxc3");
    m_uartPortCombo->addItem("UART5 (/dev/ttymxc4) - P17 Pins 25 RX, 27 TX", "/dev/ttymxc4");
    m_uartPortCombo->addItem("UART1 (/dev/ttymxc0) - Debug Console", "/dev/ttymxc0");
    m_uartPortCombo->setFixedHeight(32);
    m_uartPortCombo->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 8px; font-size: 11px; font-weight: bold;");

    QLabel *baudLbl = new QLabel("Baud:", topFrame);
    baudLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #00d2ff;");
    m_uartBaudCombo = new QComboBox(topFrame);
    m_uartBaudCombo->addItems(QStringList() << "9600" << "19200" << "38400" << "57600" << "115200" << "230400" << "460800" << "921600");
    m_uartBaudCombo->setCurrentText("115200");
    m_uartBaudCombo->setFixedWidth(90);
    m_uartBaudCombo->setFixedHeight(32);
    m_uartBaudCombo->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 6px; font-size: 11px; font-weight: bold;");

    m_uartOpenCloseBtn = new QPushButton("Open Port", topFrame);
    m_uartOpenCloseBtn->setFixedHeight(32);
    m_uartOpenCloseBtn->setFixedWidth(100);
    m_uartOpenCloseBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-weight: bold; font-size: 12px; border-radius: 6px;");
    connect(m_uartOpenCloseBtn, &QPushButton::clicked, this, &MainWindow::onOpenCloseUart);

    m_uartStatusLabel = new QLabel("Status: Closed", topFrame);
    m_uartStatusLabel->setStyleSheet("color: #8b949e; font-size: 12px; font-weight: bold;");

    topLayout->addWidget(portLbl);
    topLayout->addWidget(m_uartPortCombo, 2);
    topLayout->addWidget(baudLbl);
    topLayout->addWidget(m_uartBaudCombo);
    topLayout->addWidget(m_uartOpenCloseBtn);
    topLayout->addWidget(m_uartStatusLabel, 1);
    vLayout->addWidget(topFrame);

    // ================= HARDWARE PIN REFERENCE CALLOUT =================
    m_uartPinRefLabel = new QLabel(tab);
    m_uartPinRefLabel->setStyleSheet("background-color: #0d131a; color: #ffb74d; border: 1px solid #233242; border-radius: 6px; padding: 6px 10px; font-size: 11px; font-weight: bold;");
    connect(m_uartPortCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onUartPortChanged);
    onUartPortChanged(0);
    vLayout->addWidget(m_uartPinRefLabel);

    // ================= CONSOLE LOG CONTROLS =================
    QHBoxLayout *ctrlRow = new QHBoxLayout();
    QLabel *logLbl = new QLabel("UART Console (Raw Serial Stream):", tab);
    logLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #e6edf3;");

    QPushButton *pingBtn = new QPushButton("Send Loopback Ping", tab);
    pingBtn->setFixedHeight(28);
    pingBtn->setStyleSheet("background-color: #e65100; color: #ffffff; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 0 10px;");
    connect(pingBtn, &QPushButton::clicked, this, &MainWindow::onSendLoopbackPing);

    QPushButton *clearBtn = new QPushButton("Clear Console", tab);
    clearBtn->setFixedHeight(28);
    clearBtn->setStyleSheet("background-color: #37474f; color: #cfd8dc; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 0 10px;");
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearUartLog);

    m_uartAutoScrollCheck = new QCheckBox("Auto-scroll", tab);
    m_uartAutoScrollCheck->setChecked(true);
    m_uartAutoScrollCheck->setStyleSheet("QCheckBox { color: #80cbc4; font-size: 11px; font-weight: bold; }");

    ctrlRow->addWidget(logLbl);
    ctrlRow->addStretch();
    ctrlRow->addWidget(pingBtn);
    ctrlRow->addWidget(clearBtn);
    ctrlRow->addWidget(m_uartAutoScrollCheck);
    vLayout->addLayout(ctrlRow);

    // ================= TERMINAL TEXT EDIT =================
    m_uartLogEdit = new QTextEdit(tab);
    m_uartLogEdit->setReadOnly(true);
    m_uartLogEdit->setStyleSheet(
        "QTextEdit { background-color: #0b0f14; color: #00d2ff; font-family: monospace; font-size: 12px; border: 1px solid #233242; border-radius: 6px; padding: 6px; }"
    );
    vLayout->addWidget(m_uartLogEdit, 1);

    // ================= TRANSMIT / SEND ROW =================
    QHBoxLayout *sendRow = new QHBoxLayout();
    sendRow->setSpacing(8);

    QLabel *sendLbl = new QLabel("TX:", tab);
    sendLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #69f0ae;");

    m_uartSendEdit = new QLineEdit(tab);
    m_uartSendEdit->setFixedHeight(34);
    m_uartSendEdit->setPlaceholderText("Type data to transmit over UART (Press Enter to Send)...");
    m_uartSendEdit->setStyleSheet("background-color: #16202c; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 8px; font-size: 12px;");
    connect(m_uartSendEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendUartData);

    m_uartEndingCombo = new QComboBox(tab);
    m_uartEndingCombo->addItem("CR+LF (\\r\\n)");
    m_uartEndingCombo->addItem("LF (\\n)");
    m_uartEndingCombo->addItem("None");
    m_uartEndingCombo->setFixedHeight(34);
    m_uartEndingCombo->setFixedWidth(110);
    m_uartEndingCombo->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 6px; font-size: 11px; font-weight: bold;");

    QPushButton *sendBtn = new QPushButton("Send", tab);
    sendBtn->setFixedHeight(34);
    sendBtn->setFixedWidth(80);
    sendBtn->setStyleSheet("background-color: #00897b; color: #ffffff; font-weight: bold; font-size: 12px; border-radius: 6px;");
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSendUartData);

    sendRow->addWidget(sendLbl);
    sendRow->addWidget(m_uartSendEdit, 1);
    sendRow->addWidget(m_uartEndingCombo);
    sendRow->addWidget(sendBtn);
    vLayout->addLayout(sendRow);

    return tab;
}

QWidget *MainWindow::createSpiTestTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(tab);
    vLayout->setContentsMargins(12, 10, 12, 10);
    vLayout->setSpacing(8);

    // ================= TOP CONFIGURATION PANEL =================
    QFrame *topFrame = new QFrame(tab);
    topFrame->setStyleSheet("background-color: #141c26; border: 1px solid #233242; border-radius: 8px; padding: 6px;");
    QHBoxLayout *topLayout = new QHBoxLayout(topFrame);
    topLayout->setContentsMargins(8, 4, 8, 4);
    topLayout->setSpacing(10);

    QLabel *devLbl = new QLabel("SPI Device:", topFrame);
    devLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #00d2ff;");
    m_spiDevCombo = new QComboBox(topFrame);
    m_spiDevCombo->addItem("ECSPI1: /dev/spidev0.0 (P17 Pins 13-16)", "/dev/spidev0.0");
    m_spiDevCombo->addItem("ECSPI2: /dev/spidev1.0 (P17 Pins 19-22)", "/dev/spidev1.0");
    m_spiDevCombo->setFixedHeight(32);
    m_spiDevCombo->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    connect(m_spiDevCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MainWindow::onSpiDeviceChanged);

    QLabel *modeLbl = new QLabel("Mode:", topFrame);
    modeLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #00d2ff;");
    m_spiModeCombo = new QComboBox(topFrame);
    m_spiModeCombo->addItem("Mode 0 (CPOL=0, CPHA=0)", 0);
    m_spiModeCombo->addItem("Mode 1 (CPOL=0, CPHA=1)", 1);
    m_spiModeCombo->addItem("Mode 2 (CPOL=1, CPHA=0)", 2);
    m_spiModeCombo->addItem("Mode 3 (CPOL=1, CPHA=1)", 3);
    m_spiModeCombo->setFixedHeight(32);
    m_spiModeCombo->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 6px; font-size: 11px; font-weight: bold;");

    QLabel *speedLbl = new QLabel("Speed:", topFrame);
    speedLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #00d2ff;");
    m_spiSpeedCombo = new QComboBox(topFrame);
    m_spiSpeedCombo->addItem("100 kHz", 100000);
    m_spiSpeedCombo->addItem("500 kHz", 500000);
    m_spiSpeedCombo->addItem("1 MHz", 1000000);
    m_spiSpeedCombo->addItem("5 MHz", 5000000);
    m_spiSpeedCombo->addItem("10 MHz", 10000000);
    m_spiSpeedCombo->addItem("20 MHz", 20000000);
    m_spiSpeedCombo->setCurrentIndex(2); // 1 MHz default
    m_spiSpeedCombo->setFixedHeight(32);
    m_spiSpeedCombo->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 6px; font-size: 11px; font-weight: bold;");

    topLayout->addWidget(devLbl);
    topLayout->addWidget(m_spiDevCombo, 2);
    topLayout->addWidget(modeLbl);
    topLayout->addWidget(m_spiModeCombo);
    topLayout->addWidget(speedLbl);
    topLayout->addWidget(m_spiSpeedCombo);
    vLayout->addWidget(topFrame);

    // ================= HARDWARE PIN REFERENCE CALLOUT =================
    m_spiPinRefLabel = new QLabel(tab);
    m_spiPinRefLabel->setStyleSheet("background-color: #0d131a; color: #81c784; border: 1px solid #233242; border-radius: 6px; padding: 6px 10px; font-size: 11px; font-weight: bold;");
    onSpiDeviceChanged(0);
    vLayout->addWidget(m_spiPinRefLabel);

    // ================= TEST PRESETS PANEL & RESULT BADGE =================
    QFrame *testFrame = new QFrame(tab);
    testFrame->setStyleSheet("background-color: #141c26; border: 1px solid #233242; border-radius: 8px; padding: 8px;");
    QVBoxLayout *testLayout = new QVBoxLayout(testFrame);
    testLayout->setContentsMargins(6, 4, 6, 4);
    testLayout->setSpacing(8);

    QHBoxLayout *actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);

    QPushButton *loopbackBtn = new QPushButton("⚡ Run Loopback Test (MOSI -> MISO)", testFrame);
    loopbackBtn->setFixedHeight(34);
    loopbackBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-weight: bold; font-size: 11px; border-radius: 6px;");
    connect(loopbackBtn, &QPushButton::clicked, this, &MainWindow::onRunSpiLoopbackTest);

    QPushButton *probeBtn = new QPushButton("🔍 Probe Chip / JEDEC ID (0x9F)", testFrame);
    probeBtn->setFixedHeight(34);
    probeBtn->setStyleSheet("background-color: #0277bd; color: #ffffff; font-weight: bold; font-size: 11px; border-radius: 6px;");
    connect(probeBtn, &QPushButton::clicked, this, &MainWindow::onRunSpiProbeId);

    QPushButton *sweepBtn = new QPushButton("📶 Walking Bit Sweep", testFrame);
    sweepBtn->setFixedHeight(34);
    sweepBtn->setStyleSheet("background-color: #ef6c00; color: #ffffff; font-weight: bold; font-size: 11px; border-radius: 6px;");
    connect(sweepBtn, &QPushButton::clicked, this, &MainWindow::onRunSpiPatternTest);

    QPushButton *clearSpiBtn = new QPushButton("Clear Log", testFrame);
    clearSpiBtn->setFixedHeight(34);
    clearSpiBtn->setStyleSheet("background-color: #37474f; color: #cfd8dc; font-weight: bold; font-size: 11px; border-radius: 6px;");
    connect(clearSpiBtn, &QPushButton::clicked, this, &MainWindow::onClearSpiLog);

    actionRow->addWidget(loopbackBtn, 2);
    actionRow->addWidget(probeBtn, 2);
    actionRow->addWidget(sweepBtn, 2);
    actionRow->addWidget(clearSpiBtn, 1);
    testLayout->addLayout(actionRow);

    m_spiResultBadge = new QLabel("Status: Ready to test SPI bus. Use loopback (MOSI-to-MISO) or attach an SPI device.", testFrame);
    m_spiResultBadge->setFixedHeight(28);
    m_spiResultBadge->setStyleSheet("background-color: #0b0f14; color: #b0bec5; border: 1px solid #1e2c3c; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
    testLayout->addWidget(m_spiResultBadge);

    vLayout->addWidget(testFrame);

    // ================= CUSTOM TRANSFER ROW =================
    QHBoxLayout *customRow = new QHBoxLayout();
    customRow->setSpacing(6);

    QLabel *txLbl = new QLabel("Custom TX:", tab);
    txLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #69f0ae;");

    m_spiFormatCombo = new QComboBox(tab);
    m_spiFormatCombo->addItem("Hex Bytes", "hex");
    m_spiFormatCombo->addItem("ASCII Text", "ascii");
    m_spiFormatCombo->setFixedHeight(32);
    m_spiFormatCombo->setFixedWidth(100);
    m_spiFormatCombo->setStyleSheet("background-color: #1a2432; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 6px; font-size: 11px; font-weight: bold;");

    m_spiCustomTxEdit = new QLineEdit(tab);
    m_spiCustomTxEdit->setFixedHeight(32);
    m_spiCustomTxEdit->setText("55 AA 01 02 FF 00");
    m_spiCustomTxEdit->setStyleSheet("background-color: #16202c; color: #ffffff; border: 1px solid #334d66; border-radius: 6px; padding: 2px 8px; font-size: 12px; font-family: monospace;");
    connect(m_spiCustomTxEdit, &QLineEdit::returnPressed, this, &MainWindow::onTransferCustomSpi);

    QPushButton *customSendBtn = new QPushButton("Send / Exchange", tab);
    customSendBtn->setFixedHeight(32);
    customSendBtn->setFixedWidth(120);
    customSendBtn->setStyleSheet("background-color: #00897b; color: #ffffff; font-weight: bold; font-size: 12px; border-radius: 6px;");
    connect(customSendBtn, &QPushButton::clicked, this, &MainWindow::onTransferCustomSpi);

    customRow->addWidget(txLbl);
    customRow->addWidget(m_spiFormatCombo);
    customRow->addWidget(m_spiCustomTxEdit, 1);
    customRow->addWidget(customSendBtn);
    vLayout->addLayout(customRow);

    // ================= TERMINAL TEXT EDIT =================
    m_spiLogEdit = new QTextEdit(tab);
    m_spiLogEdit->setReadOnly(true);
    m_spiLogEdit->setStyleSheet(
        "QTextEdit { background-color: #0b0f14; color: #00d2ff; font-family: monospace; font-size: 11px; border: 1px solid #233242; border-radius: 6px; padding: 6px; }"
    );
    vLayout->addWidget(m_spiLogEdit, 1);

    return tab;
}

QWidget *MainWindow::createSystemInfoTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(8, 8, 8, 8);

    QScrollArea *scrollArea = new QScrollArea(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(14);

    auto makeCard = [](const QString &title, const QString &accentCol) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(title);
        box->setStyleSheet(QString(
            "QGroupBox { font-size: 13px; font-weight: bold; color: %1; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 12px 10px 10px 10px; background-color: #141c26; }"
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
        ).arg(accentCol));
        return box;
    };

    auto addPathRow = [](QGridLayout *g, int row, const QString &item, const QString &path, const QString &desc) {
        QLabel *lblItem = new QLabel(item);
        lblItem->setStyleSheet("font-size: 12px; font-weight: bold; color: #ffffff;");
        QLabel *lblPath = new QLabel(path);
        lblPath->setStyleSheet("font-size: 11px; font-family: monospace; color: #00d2ff; background: #0d1117; padding: 3px 6px; border-radius: 4px; border: 1px solid #21262d;");
        lblPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QLabel *lblDesc = new QLabel(desc);
        lblDesc->setStyleSheet("font-size: 11px; color: #8b949e;");
        lblDesc->setWordWrap(true);

        g->addWidget(lblItem, row, 0);
        g->addWidget(lblPath, row, 1);
        g->addWidget(lblDesc, row, 2);
    };

    // ================= CARD 1: SYSTEM ARCHITECTURE & FILE PATHS =================
    QGroupBox *pathsBox = makeCard("System Architecture & Configuration Paths", "#00d2ff");
    QVBoxLayout *pathsLayout = new QVBoxLayout(pathsBox);
    pathsLayout->setSpacing(10);

    // Section A: App & Services
    QLabel *appSecLbl = new QLabel("1. Qt Application & Service Launcher");
    appSecLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #81c784;");
    pathsLayout->addWidget(appSecLbl);

    QGridLayout *appGrid = new QGridLayout();
    appGrid->setSpacing(6);
    addPathRow(appGrid, 0, "App Executable", "/opt/hmi/bin/app", "Main running Qt HMI binary (executed by launcher)");
    addPathRow(appGrid, 1, "App Switcher", "/usr/bin/set-hmi-app", "Helper CLI to deploy/swap Qt apps: set-hmi-app <binary>");
    addPathRow(appGrid, 2, "Session Launcher", "/usr/bin/hmi-session-launcher", "Configures environment & executes /opt/hmi/bin/app");
    addPathRow(appGrid, 3, "Session Config", "/etc/hmi-session.conf", "Sets APP_EXEC, QT_QPA_PLATFORM, QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS");
    addPathRow(appGrid, 4, "Systemd Service", "/lib/systemd/system/hmi-app.service", "Lifecycle service: systemctl restart hmi-app");
    pathsLayout->addLayout(appGrid);

    pathsLayout->addSpacing(6);

    // Section B: Sleep & Touch
    QLabel *sleepSecLbl = new QLabel("2. Touch-Wake Sleep Mode & Display Control");
    sleepSecLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #ffb74d;");
    pathsLayout->addWidget(sleepSecLbl);

    QGridLayout *sleepGrid = new QGridLayout();
    sleepGrid->setSpacing(6);
    addPathRow(sleepGrid, 0, "Sleep Daemon", "/usr/bin/hmi-sleep-daemon", "Background C daemon monitoring touch activity & blanking display");
    addPathRow(sleepGrid, 1, "Sleep Config", "/etc/hmi-sleep.conf", "Inactivity timeout in seconds (SLEEP_TIMEOUT_SEC=120, 0=off)");
    addPathRow(sleepGrid, 2, "Sleep Service", "/lib/systemd/system/hmi-sleep.service", "Daemon service: systemctl restart hmi-sleep");
    addPathRow(sleepGrid, 3, "Touch Device", "/dev/input/touchscreen0", "Goodix capacitive touch evdev coordinate input node");
    addPathRow(sleepGrid, 4, "Touch Calibration", "/etc/pointercal", "Touchscreen calibration transformation matrix coefficients");
    addPathRow(sleepGrid, 5, "FB Blank Sysfs", "/sys/class/graphics/fb0/blank", "1 = blank screen, 0 = unblank screen");
    addPathRow(sleepGrid, 6, "Backlight Sysfs", "/sys/class/backlight/backlight/brightness", "LCD backlight intensity level (1 to 7)");
    pathsLayout->addLayout(sleepGrid);

    pathsLayout->addSpacing(6);

    // Section C: Networking
    QLabel *netSecLbl = new QLabel("3. Dual Ethernet Network Configuration");
    netSecLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #64b5f6;");
    pathsLayout->addWidget(netSecLbl);

    QGridLayout *netGrid = new QGridLayout();
    netGrid->setSpacing(6);
    addPathRow(netGrid, 0, "ENET1 (eth0)", "/etc/systemd/network/10-eth0.network", "Static IP: 192.168.1.100/24 (Yocto: meta-custom-hmi/recipes-core/systemd/)");
    addPathRow(netGrid, 1, "ENET2 (eth1)", "/etc/systemd/network/11-eth1.network", "Static IP: 192.168.2.100/24 (Yocto: meta-custom-hmi/recipes-core/systemd/)");
    addPathRow(netGrid, 2, "Networkd Service", "systemctl restart systemd-networkd", "Reload networking configs on runtime target");
    pathsLayout->addLayout(netGrid);

    pathsLayout->addSpacing(6);

    // Section D: Dual-Boot OTA
    QLabel *otaSecLbl = new QLabel("4. Dual-Boot OTA & Partition Architecture");
    otaSecLbl->setStyleSheet("font-size: 12px; font-weight: bold; color: #ba68c8;");
    pathsLayout->addWidget(otaSecLbl);

    QGridLayout *otaGrid = new QGridLayout();
    otaGrid->setSpacing(6);
    addPathRow(otaGrid, 0, "OTA Agent", "/usr/bin/ota-update-agent", "Automated background update polling agent");
    addPathRow(otaGrid, 1, "OTA Server URL", "/etc/ota-server.conf", "Target endpoint configuration (e.g., http://<ip>:8000)");
    addPathRow(otaGrid, 2, "OTA Progress", "/tmp/ota_progress.json", "JSON state file tracking download and flashing percentage");
    addPathRow(otaGrid, 3, "SWUpdate Tool", "/usr/bin/swupdate", "Industrial A/B partition installer (-m -M flag integration)");
    addPathRow(otaGrid, 4, "Hardware Rev", "/etc/hwrevision", "Hardware board revision string (board 1.0)");
    addPathRow(otaGrid, 5, "U-Boot Initial Env", "/etc/u-boot-initial-env", "Default U-Boot environment definition for fw_printenv");
    addPathRow(otaGrid, 6, "U-Boot Env Tools", "/usr/bin/fw_printenv, /usr/bin/fw_setenv", "Read and write U-Boot environment variables (mmcblk0 Bank A/B)");
    pathsLayout->addLayout(otaGrid);

    layout->addWidget(pathsBox);

    // ================= CARD 2: P17 HARDWARE PINOUT TABLE =================
    QGroupBox *pinoutBox = makeCard("P17 Expansion Header (40-Pin 2x20 2.54mm Pinout Reference)", "#4caf50");
    QVBoxLayout *pinoutLayout = new QVBoxLayout(pinoutBox);

    QLabel *pinSub = new QLabel("Cross-reference of all P17 pins to SoC pads, Linux device drivers, and GPIO numbers:");
    pinSub->setStyleSheet("font-size: 11px; color: #8b949e;");
    pinoutLayout->addWidget(pinSub);

    QLabel *gpioNote = new QLabel(
        "<b>GPIO Configuration Note for P17 Pin 8 &amp; Pin 9:</b><br>"
        "• <b>Pin 8 (CSI_HSYNC):</b> <code>gpiochip3</code> Line 20 (Linux GPIO 116 / <code>GPIO4_IO20</code>). Default: I2C2_SCL. Safe to use as User GPIO if I2C2 audio codec is not used.<br>"
        "• <b>Pin 9 (CSI_MCLK):</b> <code>gpiochip3</code> Line 17 (Linux GPIO 113 / <code>GPIO4_IO17</code>). Default: I2C1_SDA. <i>Caution:</i> Shared with Goodix CTP Touchscreen &amp; RTC."
    );
    gpioNote->setStyleSheet("background-color: #16222f; border-left: 3px solid #00d2ff; padding: 6px 10px; font-size: 11px; color: #b0bec5; border-radius: 4px; margin-bottom: 4px;");
    pinoutLayout->addWidget(gpioNote);

    QTableWidget *table = new QTableWidget(pinoutBox);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(QStringList() << "P17 Pin" << "Board Label" << "SoC Pad" << "Function / Linux Device" << "Sysfs GPIO");
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->setStyleSheet(
        "QTableWidget { background-color: #0b0f14; gridline-color: #1e2c3c; border: 1px solid #233242; font-size: 11px; color: #e6edf3; }"
        "QHeaderView::section { background-color: #16202c; color: #00d2ff; font-weight: bold; border: 1px solid #233242; padding: 4px; }"
    );
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(380);

    struct PinDef {
        int pin;
        const char *label;
        const char *soc;
        const char *func;
        const char *gpio;
        const char *color;
    };

    static const PinDef p17Pins[] = {
        { 1,  "GEN_5V",      "—",            "5V Power Output",                  "—",                    "#ef5350" },
        { 2,  "GEN_5V",      "—",            "5V Power Output",                  "—",                    "#ef5350" },
        { 3,  "GEN_3V3",     "—",            "3.3V Power Output",                "—",                    "#ffb74d" },
        { 4,  "GEN_3V3",     "—",            "3.3V Power Output",                "—",                    "#ffb74d" },
        { 5,  "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 6,  "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 7,  "I2C1_SCL",    "CSI_PIXCLK",   "I2C1 Clock (/dev/i2c-0)",          "GPIO 114 (GPIO4_IO18)","#ce93d8" },
        { 8,  "I2C2_SCL / GPIO", "CSI_HSYNC", "I2C2 Clock (/dev/i2c-1) / User GPIO", "GPIO 116 (GPIO4_IO20 / Chip 3 Line 20)","#00e676" },
        { 9,  "I2C1_SDA / GPIO", "CSI_MCLK",  "I2C1 Data (/dev/i2c-0) / User GPIO",  "GPIO 113 (GPIO4_IO17 / Chip 3 Line 17)","#00e676" },
        { 10, "I2C2_SDA",    "CSI_VSYNC",    "I2C2 Data (/dev/i2c-1)",           "GPIO 115 (GPIO4_IO19)","#ce93d8" },
        { 11, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 12, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 13, "SPI1_MISO",   "CSI_DATA07",   "ECSPI1 MISO (/dev/spidev0.0)",     "GPIO 124 (GPIO4_IO28)","#81c784" },
        { 14, "SPI1_MOSI",   "CSI_DATA06",   "ECSPI1 MOSI (/dev/spidev0.0)",     "GPIO 123 (GPIO4_IO27)","#81c784" },
        { 15, "SPI1_CS",     "CSI_DATA05",   "ECSPI1 CS (Active GPIO CS)",       "GPIO 122 (GPIO4_IO26)","#81c784" },
        { 16, "SPI1_SCLK",   "CSI_DATA04",   "ECSPI1 Clock (/dev/spidev0.0)",    "GPIO 121 (GPIO4_IO25)","#81c784" },
        { 17, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 18, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 19, "SPI2_MISO",   "CSI_DATA03",   "ECSPI2 MISO (/dev/spidev1.0)",     "GPIO 120 (GPIO4_IO24)","#81c784" },
        { 20, "SPI2_MOSI",   "CSI_DATA02",   "ECSPI2 MOSI (/dev/spidev1.0)",     "GPIO 119 (GPIO4_IO23)","#81c784" },
        { 21, "SPI2_CS",     "CSI_DATA01",   "ECSPI2 CS (Active GPIO CS)",       "GPIO 118 (GPIO4_IO22)","#81c784" },
        { 22, "SPI2_SCLK",   "CSI_DATA00",   "ECSPI2 Clock (/dev/spidev1.0)",    "GPIO 117 (GPIO4_IO21)","#81c784" },
        { 23, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 24, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 25, "UART5_RXD",   "UART5_RX_DATA","UART5 RX (/dev/ttymxc4)",          "GPIO 31 (GPIO1_IO31)", "#4fc3f7" },
        { 26, "UART4_RXD",   "UART4_RX_DATA","UART4 RX (/dev/ttymxc3)",          "GPIO 29 (GPIO1_IO29)", "#4fc3f7" },
        { 27, "UART5_TXD",   "UART5_TX_DATA","UART5 TX (/dev/ttymxc4)",          "GPIO 30 (GPIO1_IO30)", "#4fc3f7" },
        { 28, "UART4_TXD",   "UART4_TX_DATA","UART4 TX (/dev/ttymxc3)",          "GPIO 28 (GPIO1_IO28)", "#4fc3f7" },
        { 29, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 30, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 31, "UART3_RXD",   "UART3_RX_DATA","UART3 RX (/dev/ttymxc2)",          "GPIO 25 (GPIO1_IO25)", "#4fc3f7" },
        { 32, "UART2_RXD",   "UART2_RX_DATA","UART2 RX (/dev/ttymxc1)",          "GPIO 21 (GPIO1_IO21)", "#4fc3f7" },
        { 33, "UART3_TXD",   "UART3_TX_DATA","UART3 TX (/dev/ttymxc2)",          "GPIO 24 (GPIO1_IO24)", "#4fc3f7" },
        { 34, "UART2_TXD",   "UART2_TX_DATA","UART2 TX (/dev/ttymxc1)",          "GPIO 20 (GPIO1_IO20)", "#4fc3f7" },
        { 35, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 36, "GND",         "—",            "Ground",                           "—",                    "#78909c" },
        { 37, "GEN_3V3",     "—",            "3.3V Power Output",                "—",                    "#ffb74d" },
        { 38, "GEN_3V3",     "—",            "3.3V Power Output",                "—",                    "#ffb74d" },
        { 39, "GEN_5V",      "—",            "5V Power Output",                  "—",                    "#ef5350" },
        { 40, "GEN_5V",      "—",            "5V Power Output",                  "—",                    "#ef5350" }
    };

    int numPins = sizeof(p17Pins) / sizeof(p17Pins[0]);
    table->setRowCount(numPins);

    for (int i = 0; i < numPins; ++i) {
        const PinDef &p = p17Pins[i];

        QTableWidgetItem *itemPin = new QTableWidgetItem(QString::number(p.pin));
        itemPin->setTextAlignment(Qt::AlignCenter);

        QTableWidgetItem *itemLabel = new QTableWidgetItem(p.label);
        itemLabel->setForeground(QColor(p.color));
        itemLabel->setFont(QFont("sans-serif", 10, QFont::Bold));

        QTableWidgetItem *itemSoc = new QTableWidgetItem(p.soc);
        QTableWidgetItem *itemFunc = new QTableWidgetItem(p.func);
        QTableWidgetItem *itemGpio = new QTableWidgetItem(p.gpio);

        table->setItem(i, 0, itemPin);
        table->setItem(i, 1, itemLabel);
        table->setItem(i, 2, itemSoc);
        table->setItem(i, 3, itemFunc);
        table->setItem(i, 4, itemGpio);
    }

    pinoutLayout->addWidget(table);
    layout->addWidget(pinoutBox);

    scrollArea->setWidget(container);
    tabLayout->addWidget(scrollArea);

    return tab;
}

void MainWindow::updateClockAndStats()
{
    // Update Clock
    m_clockLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));

    // Update IP
    m_ipLabel->setText("IP: " + getIpAddress());

    // Update Temperature
    float temp = getCpuTemperature();
    if (temp > 0) {
        m_tempLabel->setText(QString("SoC: %1 °C").arg(temp, 0, 'f', 1));
    }

    // Update Uptime
    m_uptimeLabel->setText("Uptime: " + getSystemUptime());

    // Update RAM
    int totalMb = 0, usedMb = 0;
    getMemoryUsage(totalMb, usedMb);
    if (totalMb > 0) {
        int pct = (usedMb * 100) / totalMb;
        m_ramBar->setValue(pct);
        m_ramTextLabel->setText(QString("RAM: %1 MB / %2 MB (%3%)").arg(usedMb).arg(totalMb).arg(pct));
    }

    updateNetworkTabStats();
}

void MainWindow::onTouchCoordinates(int x, int y, bool isDown)
{
    m_touchCoordLabel->setText(QString("X: %1  Y: %2\nStatus: %3")
        .arg(x, 4).arg(y, 4).arg(isDown ? "TOUCH" : "RELEASE"));
}

void MainWindow::onBrightnessChanged(int value)
{
    int pct = (value * 100) / 7;
    m_brightnessValueLabel->setText(QString("Brightness: %1 / 7 (%2%)").arg(value).arg(pct));

    QFile f("/sys/class/backlight/backlight/brightness");
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << value;
        f.close();
    }
}

void MainWindow::onColorButtonClicked(const QColor &color)
{
    if (m_canvas) {
        m_canvas->setPenColor(color);
    }
}

void MainWindow::onRebootClicked()
{
    ::system("reboot");
}

void MainWindow::onShutdownClicked()
{
    ::system("poweroff");
}

QString MainWindow::getIpAddress()
{
    QStringList parts;
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        const QString name = iface.name();
        if (name != "eth0" && name != "eth1") continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                parts << QString("%1:%2").arg(name).arg(entry.ip().toString());
            }
        }
    }
    return parts.isEmpty() ? "No IP" : parts.join("  |  ");
}

float MainWindow::getCpuTemperature()
{
    QFile file("/sys/class/thermal/thermal_zone0/temp");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();
        file.close();
        bool ok = false;
        int milli = line.toInt(&ok);
        if (ok) return milli / 1000.0f;
    }
    return -1.0f;
}

QString MainWindow::getSystemUptime()
{
    QFile file("/proc/uptime");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString line = in.readLine();
        file.close();
        QStringList parts = line.split(' ');
        if (!parts.isEmpty()) {
            int secs = parts.first().toDouble();
            int hrs = secs / 3600;
            int mins = (secs % 3600) / 60;
            int s = secs % 60;
            return QString("%1h %2m %3s").arg(hrs).arg(mins).arg(s);
        }
    }
    return "--";
}

void MainWindow::getMemoryUsage(int &totalMb, int &usedMb)
{
    QFile file("/proc/meminfo");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        int memTotal = 0, memFree = 0, memAvailable = 0;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("MemTotal:")) {
                memTotal = line.split(QRegExp("\\s+")).value(1).toInt();
            } else if (line.startsWith("MemFree:")) {
                memFree = line.split(QRegExp("\\s+")).value(1).toInt();
            } else if (line.startsWith("MemAvailable:")) {
                memAvailable = line.split(QRegExp("\\s+")).value(1).toInt();
            }
        }
        file.close();
        totalMb = memTotal / 1024;
        int freeKb = memAvailable > 0 ? memAvailable : memFree;
        usedMb = (memTotal - freeKb) / 1024;
    }
}

// ============================================================
//               NETWORK & OTA UPDATES TAB
// ============================================================

QWidget *MainWindow::createNetworkOtaTab()
{
    QWidget *tab = new QWidget(this);
    QHBoxLayout *mainHLayout = new QHBoxLayout(tab);
    mainHLayout->setContentsMargins(12, 10, 12, 10);
    mainHLayout->setSpacing(12);

    auto makeCard = [](const QString &title, const QString &accentCol) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(title);
        box->setStyleSheet(QString(
            "QGroupBox { font-size: 13px; font-weight: bold; color: %1; border: 1px solid #233242; border-radius: 8px; margin-top: 6px; padding: 12px 10px 10px 10px; background-color: #141c26; }"
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; }"
        ).arg(accentCol));
        return box;
    };

    // ================= LEFT COLUMN: ETHERNET & PING =================
    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->setSpacing(10);

    // 1. Ethernet Status Card
    QGroupBox *ethBox = makeCard("Ethernet Interfaces (eth0 / eth1)", "#00d2ff");
    QVBoxLayout *ethLayout = new QVBoxLayout(ethBox);
    ethLayout->setSpacing(6);
    ethLayout->setContentsMargins(8, 8, 8, 8);

    m_eth0StatusLabel = new QLabel("eth0: Checking...", ethBox);
    m_eth0StatusLabel->setStyleSheet("font-size: 12px; color: #ffffff; font-family: monospace;");
    ethLayout->addWidget(m_eth0StatusLabel);

    m_eth1StatusLabel = new QLabel("eth1: Checking...", ethBox);
    m_eth1StatusLabel->setStyleSheet("font-size: 12px; color: #ffffff; font-family: monospace;");
    ethLayout->addWidget(m_eth1StatusLabel);

    QHBoxLayout *dhcpBtnLayout = new QHBoxLayout();
    m_renewDhcpBtn = new QPushButton("Renew Auto-IP (DHCP)", ethBox);
    m_renewDhcpBtn->setFixedHeight(34);
    m_renewDhcpBtn->setStyleSheet("background-color: #0288d1; color: #ffffff; font-size: 12px; font-weight: bold; border-radius: 6px;");
    connect(m_renewDhcpBtn, &QPushButton::clicked, this, &MainWindow::onRenewDhcp);

    m_dhcpStatusLabel = new QLabel("DHCP: Ready", ethBox);
    m_dhcpStatusLabel->setStyleSheet("font-size: 11px; color: #81c784;");

    dhcpBtnLayout->addWidget(m_renewDhcpBtn);
    dhcpBtnLayout->addWidget(m_dhcpStatusLabel);
    ethLayout->addLayout(dhcpBtnLayout);
    leftCol->addWidget(ethBox);

    // 2. Ping Test Card
    QGroupBox *pingBox = makeCard("Network Connectivity Test (Ping)", "#4caf50");
    QVBoxLayout *pingLayout = new QVBoxLayout(pingBox);
    pingLayout->setSpacing(6);
    pingLayout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *targetLayout = new QHBoxLayout();
    m_pingTargetEdit = new QLineEdit("192.168.1.1", pingBox);
    m_pingTargetEdit->setFixedHeight(34);
    m_pingTargetEdit->setReadOnly(true);
    m_pingTargetEdit->setStyleSheet("background-color: #1a2432; color: #ffffff; font-size: 13px; font-weight: bold; padding: 0 8px; border: 1px solid #334d66; border-radius: 6px;");

    QPushButton *editTargetBtn = new QPushButton("Edit Target", pingBox);
    editTargetBtn->setFixedHeight(34);
    editTargetBtn->setStyleSheet("background-color: #263238; color: #00d2ff; font-size: 12px; font-weight: bold; border-radius: 6px; padding: 0 10px;");
    connect(editTargetBtn, &QPushButton::clicked, this, &MainWindow::onEditPingTarget);

    targetLayout->addWidget(m_pingTargetEdit, 1);
    targetLayout->addWidget(editTargetBtn);
    pingLayout->addLayout(targetLayout);

    // Quick target presets
    QHBoxLayout *presetsLayout = new QHBoxLayout();
    presetsLayout->setSpacing(6);
    auto addPreset = [this, presetsLayout, pingBox](const QString &name, const QString &ip) {
        QPushButton *b = new QPushButton(name, pingBox);
        b->setFixedHeight(28);
        b->setStyleSheet("background-color: #1f2d3d; color: #90caf9; font-size: 11px; border-radius: 4px; border: 1px solid #2a3b4c;");
        connect(b, &QPushButton::clicked, [this, ip]() {
            m_pingTargetEdit->setText(ip);
        });
        presetsLayout->addWidget(b);
    };
    addPreset("Gateway", "192.168.1.1");
    addPreset("OTA Server", m_otaServerUrl.section("//", 1, 1).section(':', 0, 0));
    addPreset("DNS (8.8.8.8)", "8.8.8.8");
    pingLayout->addLayout(presetsLayout);

    m_runPingBtn = new QPushButton("Run Ping Test", pingBox);
    m_runPingBtn->setFixedHeight(36);
    m_runPingBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-size: 13px; font-weight: bold; border-radius: 6px;");
    connect(m_runPingBtn, &QPushButton::clicked, this, &MainWindow::onRunPing);
    pingLayout->addWidget(m_runPingBtn);

    m_pingResultLabel = new QLabel("Result: Ready to test", pingBox);
    m_pingResultLabel->setFixedHeight(32);
    m_pingResultLabel->setStyleSheet("background-color: #0d131a; color: #b0bec5; font-size: 12px; font-family: monospace; border: 1px solid #1e2c3c; border-radius: 4px; padding: 4px;");
    pingLayout->addWidget(m_pingResultLabel);

    leftCol->addWidget(pingBox);
    leftCol->addStretch();
    mainHLayout->addLayout(leftCol, 1);

    // ================= RIGHT COLUMN: OTA & DUAL-BANK =================
    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->setSpacing(10);

    // 3. Combined System Version & Server Card
    QGroupBox *otaInfoBox = makeCard("System Firmware && Server Configuration", "#ab47bc");
    QVBoxLayout *otaInfoLayout = new QVBoxLayout(otaInfoBox);
    otaInfoLayout->setSpacing(6);
    otaInfoLayout->setContentsMargins(8, 8, 8, 8);

    QHBoxLayout *verRow = new QHBoxLayout();
    m_otaVersionLabel = new QLabel(QString("Version: %1").arg(getInstalledOtaVersion()), otaInfoBox);
    m_otaVersionLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #e1bee7;");
    m_otaBankLabel = new QLabel(QString("RootFS: %1").arg(getActiveBootBank()), otaInfoBox);
    m_otaBankLabel->setStyleSheet("font-size: 13px; color: #ce93d8; font-weight: bold;");
    verRow->addWidget(m_otaVersionLabel);
    verRow->addWidget(m_otaBankLabel);
    otaInfoLayout->addLayout(verRow);

    QHBoxLayout *serverRow = new QHBoxLayout();
    m_otaServerLabel = new QLabel(QString("Server: %1").arg(m_otaServerUrl), otaInfoBox);
    m_otaServerLabel->setStyleSheet("font-size: 12px; color: #ffe0b2; font-family: monospace;");
    QPushButton *editServerBtn = new QPushButton("Edit Server", otaInfoBox);
    editServerBtn->setFixedHeight(32);
    editServerBtn->setStyleSheet("background-color: #e65100; color: #ffffff; font-size: 12px; font-weight: bold; border-radius: 6px; padding: 0 10px;");
    connect(editServerBtn, &QPushButton::clicked, this, &MainWindow::onEditServerClicked);
    serverRow->addWidget(m_otaServerLabel, 1);
    serverRow->addWidget(editServerBtn);
    otaInfoLayout->addLayout(serverRow);

    rightCol->addWidget(otaInfoBox);

    // 4. Update Checker & Action Card
    QGroupBox *updateBox = makeCard("Firmware Update Management", "#00e676");
    QVBoxLayout *updateLayout = new QVBoxLayout(updateBox);
    updateLayout->setSpacing(8);
    updateLayout->setContentsMargins(8, 8, 8, 8);

    m_checkUpdateBtn = new QPushButton("Check for Update", updateBox);
    m_checkUpdateBtn->setFixedHeight(36);
    m_checkUpdateBtn->setStyleSheet("background-color: #00897b; color: #ffffff; font-size: 13px; font-weight: bold; border-radius: 6px;");
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &MainWindow::onCheckOtaUpdate);
    updateLayout->addWidget(m_checkUpdateBtn);

    m_installUpdateBtn = new QPushButton("Install Update Now", updateBox);
    m_installUpdateBtn->setFixedHeight(36);
    m_installUpdateBtn->setStyleSheet("background-color: #00c853; color: #ffffff; font-size: 13px; font-weight: bold; border-radius: 6px;");
    m_installUpdateBtn->setVisible(false);
    connect(m_installUpdateBtn, &QPushButton::clicked, this, &MainWindow::onInstallOtaUpdate);
    updateLayout->addWidget(m_installUpdateBtn);

    m_otaProgressBar = new QProgressBar(updateBox);
    m_otaProgressBar->setRange(0, 100);
    m_otaProgressBar->setValue(0);
    m_otaProgressBar->setFixedHeight(24);
    m_otaProgressBar->setTextVisible(true);
    m_otaProgressBar->setStyleSheet(
        "QProgressBar { background-color: #0d131a; border: 1px solid #1e2c3c; border-radius: 6px; text-align: center; color: #ffffff; font-weight: bold; font-size: 11px; }"
        "QProgressBar::chunk { background-color: #00e676; border-radius: 5px; }"
    );
    updateLayout->addWidget(m_otaProgressBar);

    m_otaStatusLabel = new QLabel("Status: Idle", updateBox);
    m_otaStatusLabel->setFixedHeight(32);
    m_otaStatusLabel->setStyleSheet("background-color: #0d131a; color: #b2dfdb; font-size: 12px; border: 1px solid #1e2c3c; border-radius: 4px; padding: 4px;");
    m_otaStatusLabel->setWordWrap(true);
    updateLayout->addWidget(m_otaStatusLabel);

    rightCol->addWidget(updateBox);
    rightCol->addStretch();
    mainHLayout->addLayout(rightCol, 1);

    updateNetworkTabStats();

    return tab;
}

void MainWindow::updateNetworkTabStats()
{
    if (!m_eth0StatusLabel || !m_eth1StatusLabel) return;

    auto getIfaceDetails = [](const QString &ifname) -> QString {
        QNetworkInterface iface = QNetworkInterface::interfaceFromName(ifname);
        if (!iface.isValid()) return QString("%1: Not Present").arg(ifname);

        bool isUp = iface.flags().testFlag(QNetworkInterface::IsUp) &&
                    iface.flags().testFlag(QNetworkInterface::IsRunning);

        QString ip = "No IP";
        QString mask = "";
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                ip = entry.ip().toString();
                mask = entry.netmask().toString();
                break;
            }
        }

        QString mac = iface.hardwareAddress();
        if (mac.isEmpty()) mac = "--:--:--:--:--:--";

        return QString("%1: [%2]  IP: %3\n      Mask: %4  MAC: %5")
            .arg(ifname)
            .arg(isUp ? "LINK UP" : "LINK DOWN")
            .arg(ip)
            .arg(mask.isEmpty() ? "--" : mask)
            .arg(mac);
    };

    m_eth0StatusLabel->setText(getIfaceDetails("eth0"));
    m_eth1StatusLabel->setText(getIfaceDetails("eth1"));
}

void MainWindow::onRenewDhcp()
{
    m_renewDhcpBtn->setEnabled(false);
    m_renewDhcpBtn->setText("Renewing Auto-IP...");
    m_dhcpStatusLabel->setText("DHCP: Requesting IP...");
    m_dhcpStatusLabel->setStyleSheet("color: #ffa726; font-size: 12px;");

    QProcess::startDetached("/bin/sh", QStringList() << "-c"
        << "systemctl restart systemd-networkd 2>/dev/null || networkctl reconfigure eth0 eth1 2>/dev/null || true");

    QTimer::singleShot(3000, this, [this]() {
        m_renewDhcpBtn->setEnabled(true);
        m_renewDhcpBtn->setText("Renew Auto-IP (DHCP)");
        m_dhcpStatusLabel->setText("DHCP: Renewed");
        m_dhcpStatusLabel->setStyleSheet("color: #81c784; font-size: 12px;");
        updateNetworkTabStats();
    });
}

void MainWindow::onEditPingTarget()
{
    NumpadDialog dlg("Enter Ping Target Host / IP", m_pingTargetEdit->text(), this);
    if (dlg.exec() == QDialog::Accepted) {
        QString val = dlg.getValue();
        if (!val.isEmpty()) {
            m_pingTargetEdit->setText(val);
        }
    }
}

void MainWindow::onRunPing()
{
    QString target = m_pingTargetEdit->text().trimmed();
    if (target.isEmpty()) return;

    m_runPingBtn->setEnabled(false);
    m_runPingBtn->setText("Pinging...");
    m_runPingBtn->setStyleSheet("background-color: #f57f17; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px;");
    m_pingResultLabel->setText(QString("Pinging %1 (3 packets)...").arg(target));
    m_pingResultLabel->setStyleSheet("background-color: #0d131a; color: #ffa726; font-size: 12px; font-family: monospace; border: 1px solid #1e2c3c; border-radius: 4px; padding: 4px;");

    m_pingProcess->kill();
    m_pingProcess->start("ping", QStringList() << "-c" << "3" << "-W" << "2" << target);
}

void MainWindow::onPingReadyRead()
{
    QString out = m_pingProcess->readAllStandardOutput();
    if (out.contains("rtt") || out.contains("round-trip")) {
        QString stats = out.section("rtt min/avg/max/mdev = ", 1, 1).trimmed();
        if (stats.isEmpty()) stats = out.section("round-trip min/avg/max = ", 1, 1).trimmed();
        if (!stats.isEmpty()) {
            QString avg = stats.split('/').value(1);
            m_pingResultLabel->setText(QString("🟢 Reachable! Avg Latency: %1 ms").arg(avg));
            m_pingResultLabel->setStyleSheet("background-color: #0d131a; color: #4caf50; font-size: 12px; font-weight: bold; font-family: monospace; border: 1px solid #2e7d32; border-radius: 4px; padding: 4px;");
        }
    }
}

void MainWindow::onPingProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    m_runPingBtn->setEnabled(true);
    m_runPingBtn->setText("Run Ping Test");
    m_runPingBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px;");

    if (exitCode != 0) {
        m_pingResultLabel->setText("🔴 Host Unreachable / 100% Packet Loss");
        m_pingResultLabel->setStyleSheet("background-color: #0d131a; color: #ef5350; font-size: 12px; font-weight: bold; font-family: monospace; border: 1px solid #c62828; border-radius: 4px; padding: 4px;");
    }
}

QString MainWindow::getInstalledOtaVersion()
{
    QFile f("/etc/sw-versions");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString ver = f.readAll().trimmed();
        f.close();
        if (!ver.isEmpty()) return ver;
    }
    QFile f2("/etc/version");
    if (f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString ver2 = f2.readAll().trimmed();
        f2.close();
        if (!ver2.isEmpty()) return ver2;
    }
    return "v1.0.0";
}

QString MainWindow::getActiveBootBank()
{
    QFile f("/proc/cmdline");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString cmd = f.readAll();
        f.close();
        if (cmd.contains("mmcblk0p3") || cmd.contains("mmcblk1p3")) {
            return "Bank B (Secondary RootFS)";
        }
    }
    return "Bank A (Primary RootFS)";
}

void MainWindow::loadOtaServerConfig()
{
    m_otaServerUrl = "http://192.168.1.100:8000";
    QFile f("/etc/ota-server.conf");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("OTA_SERVER_URL=")) {
                m_otaServerUrl = line.section("=", 1).replace("\"", "").replace("'", "").trimmed();
            }
        }
        f.close();
    }
}

void MainWindow::saveOtaServerConfig(const QString &url)
{
    m_otaServerUrl = url.trimmed();
    QFile f("/etc/ota-server.conf");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "OTA_SERVER_URL=\"" << m_otaServerUrl << "\"\n";
        out << "CHECK_INTERVAL=30\n";
        f.close();
    }
    if (m_otaServerLabel) {
        m_otaServerLabel->setText(QString("Server: %1").arg(m_otaServerUrl));
    }
}

void MainWindow::onEditServerClicked()
{
    NumpadDialog dlg("Configure OTA Server URL", m_otaServerUrl, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString val = dlg.getValue();
        if (!val.isEmpty()) {
            if (!val.startsWith("http://") && !val.startsWith("https://")) {
                val = "http://" + val;
            }
            saveOtaServerConfig(val);
            m_otaStatusLabel->setText("Server URL saved to /etc/ota-server.conf");
            m_otaStatusLabel->setStyleSheet("color: #81c784; font-size: 12px;");
        }
    }
}

void MainWindow::onCheckOtaUpdate()
{
    m_checkUpdateBtn->setEnabled(false);
    m_checkUpdateBtn->setText("Checking Server...");
    m_installUpdateBtn->setVisible(false);

    QString queryUrl = m_otaServerUrl;
    if (!queryUrl.endsWith('/')) queryUrl += '/';
    queryUrl += "version.json";

    m_otaStatusLabel->setText(QString("Querying %1 ...").arg(queryUrl));
    m_otaStatusLabel->setStyleSheet("color: #ffe082; font-size: 12px;");

    QNetworkRequest req((QUrl(queryUrl)));
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    m_netManager->get(req);
}

void MainWindow::onOtaVersionReply(QNetworkReply *reply)
{
    m_checkUpdateBtn->setEnabled(true);
    m_checkUpdateBtn->setText("Check for Update");

    if (reply->error() != QNetworkReply::NoError) {
        m_otaStatusLabel->setText(QString("Error: %1").arg(reply->errorString()));
        m_otaStatusLabel->setStyleSheet("color: #ef5350; font-size: 12px;");
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        m_otaStatusLabel->setText("Error: Invalid JSON manifest received from server");
        m_otaStatusLabel->setStyleSheet("color: #ef5350; font-size: 12px;");
        return;
    }

    QJsonObject obj = doc.object();
    m_remoteVersion = obj.value("version").toString();
    m_remoteUpdateUrl = obj.value("url").toString();

    QString localVersion = getInstalledOtaVersion();

    if (m_remoteVersion.isEmpty()) {
        m_otaStatusLabel->setText("Error: 'version' field missing in version.json");
        m_otaStatusLabel->setStyleSheet("color: #ef5350; font-size: 12px;");
        return;
    }

    if (m_remoteVersion == localVersion) {
        m_otaStatusLabel->setText(QString("System is up to date (Version: %1)").arg(localVersion));
        m_otaStatusLabel->setStyleSheet("color: #81c784; font-size: 12px; font-weight: bold;");
        m_installUpdateBtn->setVisible(false);
    } else {
        m_otaStatusLabel->setText(QString("New version available: %1 (Current: %2)").arg(m_remoteVersion).arg(localVersion));
        m_otaStatusLabel->setStyleSheet("color: #00e676; font-size: 12px; font-weight: bold;");
        m_installUpdateBtn->setText(QString("Install Update (%1)").arg(m_remoteVersion));
        m_installUpdateBtn->setVisible(true);
    }
}

void MainWindow::onInstallOtaUpdate()
{
    m_installUpdateBtn->setEnabled(false);
    m_installUpdateBtn->setText("Updating...");
    m_otaStatusLabel->setText("Triggering background SWUpdate agent...");
    m_otaStatusLabel->setStyleSheet("color: #00d2ff; font-size: 12px;");

    QProcess::startDetached("/usr/bin/ota-update-agent");

    QTimer::singleShot(4000, this, [this]() {
        m_installUpdateBtn->setEnabled(true);
        m_otaStatusLabel->setText("Update agent started in background. Device will reboot when done.");
        m_otaStatusLabel->setStyleSheet("color: #81c784; font-size: 12px;");
    });
}

void MainWindow::onCalibrateTouchClicked()
{
    CalibrationDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        updateCalibrationStatus();
    }
}

void MainWindow::onResetCalibrationClicked()
{
    TouchCalibration::instance().reset();
    updateCalibrationStatus();
}

void MainWindow::updateCalibrationStatus()
{
    if (m_calibrationStatusLabel) {
        m_calibrationStatusLabel->setText(TouchCalibration::instance().statusString());
    }
}

// ============================================================
//                    GPIO TEST SLOTS
// ============================================================

void MainWindow::onToggleLed1()
{
    m_led1State = !m_led1State;
    QFile f("/sys/class/leds/led1/brightness");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_led1State ? "1\n" : "0\n");
        f.close();
    }
    if (m_led1Lamp) {
        m_led1Lamp->setStyleSheet(m_led1State ?
            "background-color: #00e676; border: 2px solid #69f0ae; border-radius: 14px;" :
            "background-color: #21262d; border: 2px solid #30363d; border-radius: 14px;");
    }
    if (m_led1ToggleBtn) {
        m_led1ToggleBtn->setText(m_led1State ? "Turn LED1 OFF" : "Turn LED1 ON");
        m_led1ToggleBtn->setStyleSheet(m_led1State ?
            "background-color: #2e7d32; color: #ffffff; font-weight: bold; font-size: 12px; border: 1px solid #4caf50; border-radius: 6px;" :
            "background-color: #1f2d3d; color: #00d2ff; font-weight: bold; font-size: 12px; border: 1px solid #2a3b4c; border-radius: 6px;");
    }
}

void MainWindow::onToggleBlinkTest()
{
    if (m_blinkTimer->isActive()) {
        m_blinkTimer->stop();
        m_blinkTestBtn->setText("Start Blink (1Hz)");
        m_blinkTestBtn->setStyleSheet("background-color: #263238; color: #ffb74d; font-weight: bold; font-size: 12px; border: 1px solid #37474f; border-radius: 6px;");
        // Turn LED1 off
        QFile f("/sys/class/leds/led1/brightness");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write("0\n");
            f.close();
        }
        m_led1State = false;
        if (m_led1Lamp) {
            m_led1Lamp->setStyleSheet("background-color: #21262d; border: 2px solid #30363d; border-radius: 14px;");
        }
    } else {
        m_blinkTimer->start(500); // 1Hz cycle (500ms ON, 500ms OFF)
        m_blinkTestBtn->setText("Stop Blink Test");
        m_blinkTestBtn->setStyleSheet("background-color: #d84315; color: #ffffff; font-weight: bold; font-size: 12px; border: 1px solid #ff7043; border-radius: 6px;");
    }
}

void MainWindow::onBlinkTimeout()
{
    m_blinkState = !m_blinkState;
    QFile f("/sys/class/leds/led1/brightness");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_blinkState ? "1\n" : "0\n");
        f.close();
    }
    if (m_led1Lamp) {
        m_led1Lamp->setStyleSheet(m_blinkState ?
            "background-color: #00e676; border: 2px solid #69f0ae; border-radius: 14px;" :
            "background-color: #21262d; border: 2px solid #30363d; border-radius: 14px;");
    }
}

void MainWindow::onHeartbeatModeChanged(int idx)
{
    QFile f("/sys/class/leds/heartbeat/trigger");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(idx == 0 ? "heartbeat\n" : "none\n");
        f.close();
    }
}

void MainWindow::onReadGpioPin()
{
    if (!m_gpioChipCombo || !m_gpioLineSpin) return;

    QString chip = QString("gpiochip%1").arg(m_gpioChipCombo->currentIndex());
    int line = m_gpioLineSpin->value();

    QProcess proc;
    proc.start("gpioget", QStringList() << chip << QString::number(line));
    if (proc.waitForFinished(300)) {
        QString out = proc.readAllStandardOutput().trimmed();
        bool isHigh = (out == "1");
        if (m_gpioPinLamp) {
            m_gpioPinLamp->setStyleSheet(isHigh ?
                "background-color: #00e676; border: 3px solid #b9f6ca; border-radius: 18px;" :
                "background-color: #21262d; border: 2px solid #455a64; border-radius: 18px;");
        }
        if (m_gpioPinStateLabel) {
            m_gpioPinStateLabel->setText(QString("State: %1 (%2)").arg(isHigh ? "HIGH (1)" : "LOW (0)").arg(isHigh ? "3.3V" : "0V"));
            m_gpioPinStateLabel->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1;").arg(isHigh ? "#00e676" : "#cfd8dc"));
        }
    } else {
        if (m_gpioPinStateLabel) {
            m_gpioPinStateLabel->setText("Error: gpioget timed out or line busy");
            m_gpioPinStateLabel->setStyleSheet("font-size: 13px; color: #ef5350;");
        }
    }
}

void MainWindow::onSetGpioHigh()
{
    if (!m_gpioChipCombo || !m_gpioLineSpin) return;

    QString chip = QString("gpiochip%1").arg(m_gpioChipCombo->currentIndex());
    int line = m_gpioLineSpin->value();

    QProcess::execute("gpioset", QStringList() << chip << QString("%1=1").arg(line));
    onReadGpioPin();
}

void MainWindow::onSetGpioLow()
{
    if (!m_gpioChipCombo || !m_gpioLineSpin) return;

    QString chip = QString("gpiochip%1").arg(m_gpioChipCombo->currentIndex());
    int line = m_gpioLineSpin->value();

    QProcess::execute("gpioset", QStringList() << chip << QString("%1=0").arg(line));
    onReadGpioPin();
}

void MainWindow::onGpioAutoPollToggled(bool checked)
{
    if (checked) {
        m_gpioPollTimer->start(500);
        onReadGpioPin();
    } else {
        m_gpioPollTimer->stop();
    }
}

void MainWindow::onGpioPollTimeout()
{
    onReadGpioPin();
}

// ============================================================
//                   OTA REAL-TIME PROGRESS
// ============================================================

void MainWindow::updateOtaProgress()
{
    QFile f("/tmp/ota_progress.json");
    if (!f.exists()) return;

    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = f.readAll();
        f.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString stage = obj.value("stage").toString();
            int percent = obj.value("percent").toInt();
            QString detail = obj.value("detail").toString();

            if (m_otaProgressBar) {
                m_otaProgressBar->setValue(percent);
            }
            if (m_otaStatusLabel && !detail.isEmpty()) {
                m_otaStatusLabel->setText(detail);
                if (stage == "downloading" || stage == "flashing") {
                    m_otaStatusLabel->setStyleSheet("color: #00d2ff; font-size: 12px; font-weight: bold;");
                } else if (stage == "completed") {
                    m_otaStatusLabel->setStyleSheet("color: #00e676; font-size: 13px; font-weight: bold;");
                } else if (stage == "failed") {
                    m_otaStatusLabel->setStyleSheet("color: #ef5350; font-size: 12px; font-weight: bold;");
                }
            }
        }
    }
}

// ============================================================
//                   SLEEP TIMER CONFIG
// ============================================================

void MainWindow::loadSleepConfig()
{
    int secs = 120;
    QFile f("/etc/hmi-sleep.conf");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("SLEEP_TIMEOUT_SEC=")) {
                bool ok = false;
                int val = line.section('=', 1, 1).trimmed().toInt(&ok);
                if (ok) secs = val;
                break;
            }
        }
        f.close();
    }
    if (m_sleepTimeoutSpin) {
        m_sleepTimeoutSpin->setValue(secs);
    }
    if (m_sleepStatusLabel) {
        if (secs == 0) {
            m_sleepStatusLabel->setText("Sleep Timeout: Always ON (Disabled)");
        } else {
            m_sleepStatusLabel->setText(QString("Sleep Timeout: %1 seconds (%2 min)").arg(secs).arg(secs / 60.0, 0, 'f', 1));
        }
    }
}

void MainWindow::saveSleepConfig(int seconds)
{
    QFile f("/etc/hmi-sleep.conf");
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&f);
        out << QString("SLEEP_TIMEOUT_SEC=%1\n").arg(seconds);
        f.close();
    }
    if (m_sleepTimeoutSpin) {
        m_sleepTimeoutSpin->setValue(seconds);
    }
    if (m_sleepStatusLabel) {
        if (seconds == 0) {
            m_sleepStatusLabel->setText("Sleep Timeout: Always ON (Disabled)");
        } else {
            m_sleepStatusLabel->setText(QString("Sleep Timeout: %1 seconds (Saved live!)").arg(seconds));
        }
    }
}

void MainWindow::onSleepPresetClicked(int seconds)
{
    saveSleepConfig(seconds);
}

void MainWindow::onApplyCustomSleep()
{
    if (m_sleepTimeoutSpin) {
        saveSleepConfig(m_sleepTimeoutSpin->value());
    }
}

void MainWindow::onSleepNowClicked()
{
    QFile f("/sys/class/graphics/fb0/blank");
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "1";
        f.close();
    }
    QFile fb("/sys/class/backlight/backlight/bl_power");
    if (fb.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&fb);
        out << "1";
        fb.close();
    }
}

// ============================================================
//                   UART CONSOLE SLOTS
// ============================================================

void MainWindow::onUartPortChanged(int index)
{
    if (!m_uartPinRefLabel || !m_uartPortCombo) return;
    QString dev = m_uartPortCombo->itemData(index).toString();
    if (dev == "/dev/ttymxc1") {
        m_uartPinRefLabel->setText("📌 UART2 (/dev/ttymxc1) on P17 Header: RX = Pin 32 | TX = Pin 34 | GND = Pin 30/35 | 3.3V TTL\n💡 Loopback Test: Place jumper wire between P17 Pin 32 and Pin 34, then click 'Send Loopback Ping'.");
    } else if (dev == "/dev/ttymxc2") {
        m_uartPinRefLabel->setText("📌 UART3 (/dev/ttymxc2) on P17 Header: RX = Pin 31 | TX = Pin 33 | GND = Pin 30/35 | 3.3V TTL\n💡 Loopback Test: Place jumper wire between P17 Pin 31 and Pin 33, then click 'Send Loopback Ping'.");
    } else if (dev == "/dev/ttymxc3") {
        m_uartPinRefLabel->setText("📌 UART4 (/dev/ttymxc3) on P17 Header: RX = Pin 26 | TX = Pin 28 | GND = Pin 24/29 | 3.3V TTL\n💡 Loopback Test: Place jumper wire between P17 Pin 26 and Pin 28, then click 'Send Loopback Ping'.");
    } else if (dev == "/dev/ttymxc4") {
        m_uartPinRefLabel->setText("📌 UART5 (/dev/ttymxc4) on P17 Header: RX = Pin 25 | TX = Pin 27 | GND = Pin 24/29 | 3.3V TTL\n💡 Loopback Test: Place jumper wire between P17 Pin 25 and Pin 27, then click 'Send Loopback Ping'.");
    } else {
        m_uartPinRefLabel->setText("📌 UART1 (/dev/ttymxc0): Primary Serial Console on microUSB (J9 connector) @ 115200 8N1.");
    }
}

void MainWindow::onOpenCloseUart()
{
    if (m_uartFd >= 0) {
        // Close port
        if (m_uartNotifier) {
            m_uartNotifier->setEnabled(false);
            delete m_uartNotifier;
            m_uartNotifier = nullptr;
        }
        ::close(m_uartFd);
        m_uartFd = -1;

        m_uartOpenCloseBtn->setText("Open Port");
        m_uartOpenCloseBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-weight: bold; border-radius: 6px;");
        m_uartStatusLabel->setText("Status: Port Closed");
        m_uartStatusLabel->setStyleSheet("color: #8b949e; font-size: 12px; font-weight: bold;");
        m_uartPortCombo->setEnabled(true);
        m_uartBaudCombo->setEnabled(true);
        m_uartLogEdit->append("<span style='color:#ff9800;'>[SYSTEM] Port closed.</span>");
        return;
    }

    // Open port
    QString devPath = m_uartPortCombo->currentData().toString();
    int baud = m_uartBaudCombo->currentText().toInt();
    speed_t speed = B115200;
    switch (baud) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
        default: speed = B115200; break;
    }

    m_uartFd = ::open(devPath.toLocal8Bit().constData(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (m_uartFd < 0) {
        m_uartLogEdit->append(QString("<span style='color:#ef5350;'>[ERROR] Failed to open %1: %2</span>").arg(devPath, strerror(errno)));
        return;
    }

    fcntl(m_uartFd, F_SETFL, 0);

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(m_uartFd, &tty) != 0) {
        m_uartLogEdit->append(QString("<span style='color:#ef5350;'>[ERROR] tcgetattr failed: %1</span>").arg(strerror(errno)));
        ::close(m_uartFd);
        m_uartFd = -1;
        return;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(m_uartFd, TCSANOW, &tty) != 0) {
        m_uartLogEdit->append(QString("<span style='color:#ef5350;'>[ERROR] tcsetattr failed: %1</span>").arg(strerror(errno)));
        ::close(m_uartFd);
        m_uartFd = -1;
        return;
    }

    tcflush(m_uartFd, TCIOFLUSH);

    m_uartNotifier = new QSocketNotifier(m_uartFd, QSocketNotifier::Read, this);
    connect(m_uartNotifier, &QSocketNotifier::activated, this, &MainWindow::onUartDataReady);

    m_uartOpenCloseBtn->setText("Close Port");
    m_uartOpenCloseBtn->setStyleSheet("background-color: #d32f2f; color: #ffffff; font-weight: bold; border-radius: 6px;");
    m_uartStatusLabel->setText(QString("Connected: %1 @ %2 8N1").arg(devPath).arg(baud));
    m_uartStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px; font-weight: bold;");
    m_uartPortCombo->setEnabled(false);
    m_uartBaudCombo->setEnabled(false);
    m_uartLogEdit->append(QString("<span style='color:#4caf50;'>[SYSTEM] Successfully opened %1 @ %2 baud 8N1</span>").arg(devPath).arg(baud));
}

void MainWindow::onUartDataReady()
{
    if (m_uartFd < 0) return;
    char buf[512];
    ssize_t n = ::read(m_uartFd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        QString text = QString::fromUtf8(buf, n);
        QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        m_uartLogEdit->append(QString("<span style='color:#8b949e;'>[%1] </span><span style='color:#00d2ff; font-weight:bold;'>[RX] </span><span style='color:#ffffff;'>%2</span>")
            .arg(timeStr, text.toHtmlEscaped()));
        if (m_uartAutoScrollCheck && m_uartAutoScrollCheck->isChecked()) {
            m_uartLogEdit->moveCursor(QTextCursor::End);
        }
    }
}

void MainWindow::onSendUartData()
{
    if (m_uartFd < 0) {
        m_uartLogEdit->append("<span style='color:#ef5350;'>[ERROR] Port is closed. Open port first!</span>");
        return;
    }
    QString text = m_uartSendEdit->text();
    if (text.isEmpty()) return;

    QString ending = m_uartEndingCombo->currentText();
    QByteArray payload = text.toUtf8();
    if (ending.contains("CR+LF")) {
        payload.append("\r\n");
    } else if (ending.contains("LF")) {
        payload.append("\n");
    }

    ssize_t written = ::write(m_uartFd, payload.constData(), payload.size());
    if (written > 0) {
        QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        m_uartLogEdit->append(QString("<span style='color:#8b949e;'>[%1] </span><span style='color:#69f0ae; font-weight:bold;'>[TX] </span><span style='color:#e0e0e0;'>%2</span>")
            .arg(timeStr, text.toHtmlEscaped()));
        m_uartSendEdit->clear();
        if (m_uartAutoScrollCheck && m_uartAutoScrollCheck->isChecked()) {
            m_uartLogEdit->moveCursor(QTextCursor::End);
        }
    } else {
        m_uartLogEdit->append(QString("<span style='color:#ef5350;'>[ERROR] Write failed: %1</span>").arg(strerror(errno)));
    }
}

void MainWindow::onSendLoopbackPing()
{
    if (m_uartFd < 0) {
        m_uartLogEdit->append("<span style='color:#ef5350;'>[ERROR] Open port first before sending loopback ping.</span>");
        return;
    }
    QByteArray ping = "PING_OKMX6ULL\r\n";
    ssize_t written = ::write(m_uartFd, ping.constData(), ping.size());
    if (written > 0) {
        QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        m_uartLogEdit->append(QString("<span style='color:#8b949e;'>[%1] </span><span style='color:#ffab00; font-weight:bold;'>[PING] </span><span style='color:#fff8e1;'>Sent PING_OKMX6ULL (Ensure RX & TX pins are connected for echo!)</span>")
            .arg(timeStr));
        if (m_uartAutoScrollCheck && m_uartAutoScrollCheck->isChecked()) {
            m_uartLogEdit->moveCursor(QTextCursor::End);
        }
    }
}

void MainWindow::onClearUartLog()
{
    if (m_uartLogEdit) {
        m_uartLogEdit->clear();
    }
}

// ============================================================
//                   SPI TEST HELPER & SLOTS
// ============================================================

bool MainWindow::transferSpi(const QString &device, uint8_t mode, uint32_t speed,
                             const QByteArray &txData, QByteArray &rxData, QString &errorMsg)
{
    int fd = ::open(device.toLocal8Bit().constData(), O_RDWR);
    if (fd < 0) {
        errorMsg = QString("Failed to open %1: %2").arg(device, strerror(errno));
        return false;
    }

    uint8_t bits = 8;
    if (::ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        errorMsg = QString("Failed to configure %1: %2").arg(device, strerror(errno));
        ::close(fd);
        return false;
    }

    rxData.resize(txData.size());
    rxData.fill(0);

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)txData.constData();
    tr.rx_buf = (unsigned long)rxData.data();
    tr.len = txData.size();
    tr.speed_hz = speed;
    tr.bits_per_word = bits;
    tr.delay_usecs = 0;

    if (::ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        errorMsg = QString("ioctl SPI_IOC_MESSAGE failed: %1").arg(strerror(errno));
        ::close(fd);
        return false;
    }

    ::close(fd);
    return true;
}

void MainWindow::onSpiDeviceChanged(int index)
{
    if (!m_spiPinRefLabel || !m_spiDevCombo) return;
    QString dev = m_spiDevCombo->itemData(index).toString();
    if (dev == "/dev/spidev0.0") {
        m_spiPinRefLabel->setText(
            "📌 ECSPI1 on P17 Header: MOSI = Pin 14 | MISO = Pin 13 | SCLK = Pin 16 | CS = Pin 15 | GND = Pin 11/12/17/18 (3.3V Logic)\n"
            "💡 Default Loopback Test: Connect jumper wire between P17 Pin 14 (MOSI) and Pin 13 (MISO), then click 'Run Loopback Test'."
        );
    } else {
        m_spiPinRefLabel->setText(
            "📌 ECSPI2 on P17 Header: MOSI = Pin 20 | MISO = Pin 19 | SCLK = Pin 22 | CS = Pin 21 | GND = Pin 17/18/23/24 (3.3V Logic)\n"
            "💡 Default Loopback Test: Connect jumper wire between P17 Pin 20 (MOSI) and Pin 19 (MISO), then click 'Run Loopback Test'."
        );
    }
}

void MainWindow::onRunSpiLoopbackTest()
{
    if (!m_spiDevCombo || !m_spiModeCombo || !m_spiSpeedCombo || !m_spiLogEdit) return;

    QString dev = m_spiDevCombo->currentData().toString();
    uint8_t mode = m_spiModeCombo->currentData().toUInt();
    uint32_t speed = m_spiSpeedCombo->currentData().toUInt();

    static const unsigned char pattern[] = {
        0x55, 0xAA, 0x00, 0xFF, 0x12, 0x34, 0x56, 0x78,
        0xDE, 0xAD, 0xBE, 0xEF, 0x43, 0x21, 0xA5, 0x5A
    };
    QByteArray tx((const char*)pattern, sizeof(pattern));
    QByteArray rx;
    QString err;

    bool ok = transferSpi(dev, mode, speed, tx, rx, err);
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    if (!ok) {
        m_spiLogEdit->append(QString("<span style='color:#ef5350;'>[%1] [SPI ERROR] %2</span>").arg(timeStr, err));
        if (m_spiResultBadge) {
            m_spiResultBadge->setText("Status: " + err);
            m_spiResultBadge->setStyleSheet("background-color: #b71c1c; color: #ffffff; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
        }
        return;
    }

    bool matched = (tx == rx);
    QString txHex = tx.toHex(' ').toUpper();
    QString rxHex = rx.toHex(' ').toUpper();

    m_spiLogEdit->append(QString("<span style='color:#8b949e;'>[%1] </span><span style='color:#00d2ff; font-weight:bold;'>[LOOPBACK TEST]</span> on %2 @ %3 Hz (Mode %4)")
        .arg(timeStr, dev).arg(speed).arg(mode));
    m_spiLogEdit->append(QString("  <span style='color:#69f0ae;'>TX: %1</span>").arg(txHex));
    m_spiLogEdit->append(QString("  <span style='color:%1;'>RX: %2</span>").arg(matched ? "#69f0ae" : "#ffb74d", rxHex));

    if (matched) {
        m_spiLogEdit->append("<span style='color:#00e676; font-weight:bold;'>  ✓ VERIFIED: All 16 bytes matched exactly! Full-duplex bus loopback confirmed.</span>");
        if (m_spiResultBadge) {
            m_spiResultBadge->setText(QString("✓ PASS: Loopback Verified (16/16 Bytes Matched on %1 @ %2 kHz)").arg(dev).arg(speed / 1000));
            m_spiResultBadge->setStyleSheet("background-color: #1b5e20; color: #69f0ae; border: 1px solid #2e7d32; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
        }
    } else {
        m_spiLogEdit->append("<span style='color:#ef5350; font-weight:bold;'>  ✗ MISMATCH: Received bytes do not match TX. Ensure MOSI and MISO jumper is in place!</span>");
        if (m_spiResultBadge) {
            m_spiResultBadge->setText("✗ LOOPBACK MISMATCH: Jumper MOSI to MISO to test loopback echo!");
            m_spiResultBadge->setStyleSheet("background-color: #bf360c; color: #ffccbc; border: 1px solid #d84315; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
        }
    }
}

void MainWindow::onRunSpiProbeId()
{
    if (!m_spiDevCombo || !m_spiModeCombo || !m_spiSpeedCombo || !m_spiLogEdit) return;

    QString dev = m_spiDevCombo->currentData().toString();
    uint8_t mode = m_spiModeCombo->currentData().toUInt();
    uint32_t speed = m_spiSpeedCombo->currentData().toUInt();

    // Standard JEDEC ID command: 0x9F followed by 3 dummy bytes
    QByteArray tx;
    tx.append((char)0x9F);
    tx.append((char)0x00);
    tx.append((char)0x00);
    tx.append((char)0x00);

    QByteArray rx;
    QString err;
    bool ok = transferSpi(dev, mode, speed, tx, rx, err);
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    if (!ok) {
        m_spiLogEdit->append(QString("<span style='color:#ef5350;'>[%1] [SPI ERROR] %2</span>").arg(timeStr, err));
        return;
    }

    QString rxHex = rx.toHex(' ').toUpper();
    m_spiLogEdit->append(QString("<span style='color:#8b949e;'>[%1] </span><span style='color:#0288d1; font-weight:bold;'>[CHIP PROBE 0x9F]</span> on %2: RX = %3")
        .arg(timeStr, dev, rxHex));

    uint8_t mfg = (uint8_t)rx.at(1);
    uint8_t devType = (uint8_t)rx.at(2);
    uint8_t cap = (uint8_t)rx.at(3);

    if (mfg != 0x00 && mfg != 0xFF) {
        m_spiLogEdit->append(QString("<span style='color:#00e676;'>  ✓ Detected Device Response: Mfg ID=0x%1, Type=0x%2, Capacity=0x%3</span>")
            .arg(mfg, 2, 16, QChar('0')).arg(devType, 2, 16, QChar('0')).arg(cap, 2, 16, QChar('0')));
        if (m_spiResultBadge) {
            m_spiResultBadge->setText(QString("✓ Device Detected: Mfg=0x%1 Type=0x%2 Cap=0x%3")
                .arg(mfg, 2, 16, QChar('0')).arg(devType, 2, 16, QChar('0')).arg(cap, 2, 16, QChar('0')));
            m_spiResultBadge->setStyleSheet("background-color: #0d47a1; color: #80d8ff; border: 1px solid #1976d2; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
        }
    } else {
        m_spiLogEdit->append("<span style='color:#b0bec5;'>  No external SPI chip recognized (all 0x00 or 0xFF). Bus is open or unpopulated.</span>");
        if (m_spiResultBadge) {
            m_spiResultBadge->setText(QString("Probe Result: %1 (No peripheral slave detected)").arg(rxHex));
            m_spiResultBadge->setStyleSheet("background-color: #263238; color: #b0bec5; border: 1px solid #37474f; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
        }
    }
}

void MainWindow::onRunSpiPatternTest()
{
    if (!m_spiDevCombo || !m_spiModeCombo || !m_spiSpeedCombo || !m_spiLogEdit) return;

    QString dev = m_spiDevCombo->currentData().toString();
    uint8_t mode = m_spiModeCombo->currentData().toUInt();
    uint32_t speed = m_spiSpeedCombo->currentData().toUInt();

    static const unsigned char bits[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    QByteArray tx((const char*)bits, sizeof(bits));
    QByteArray rx;
    QString err;

    bool ok = transferSpi(dev, mode, speed, tx, rx, err);
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    if (!ok) {
        m_spiLogEdit->append(QString("<span style='color:#ef5350;'>[%1] [SPI ERROR] %2</span>").arg(timeStr, err));
        return;
    }

    bool matched = (tx == rx);
    m_spiLogEdit->append(QString("<span style='color:#8b949e;'>[%1] </span><span style='color:#ff9800; font-weight:bold;'>[WALKING BITS]</span> TX: %2 | RX: %3 (%4)")
        .arg(timeStr, tx.toHex(' ').toUpper(), rx.toHex(' ').toUpper(), matched ? "MATCHED" : "MISMATCH"));

    if (m_spiResultBadge) {
        if (matched) {
            m_spiResultBadge->setText("✓ Walking Bits PASS: All 8 bit positions verified!");
            m_spiResultBadge->setStyleSheet("background-color: #1b5e20; color: #69f0ae; border: 1px solid #2e7d32; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
        } else {
            m_spiResultBadge->setText(QString("Walking Bits: TX [%1] != RX [%2]").arg(tx.toHex(' ').toUpper(), rx.toHex(' ').toUpper()));
            m_spiResultBadge->setStyleSheet("background-color: #37474f; color: #eceff1; border: 1px solid #455a64; border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: bold;");
        }
    }
}

void MainWindow::onTransferCustomSpi()
{
    if (!m_spiDevCombo || !m_spiModeCombo || !m_spiSpeedCombo || !m_spiCustomTxEdit || !m_spiLogEdit) return;

    QString dev = m_spiDevCombo->currentData().toString();
    uint8_t mode = m_spiModeCombo->currentData().toUInt();
    uint32_t speed = m_spiSpeedCombo->currentData().toUInt();
    QString input = m_spiCustomTxEdit->text().trimmed();
    if (input.isEmpty()) return;

    QByteArray tx;
    if (m_spiFormatCombo && m_spiFormatCombo->currentData().toString() == "ascii") {
        tx = input.toUtf8();
    } else {
        // Hex bytes: remove spaces, colons, 0x prefixes
        QString clean = input;
        clean.remove("0x", Qt::CaseInsensitive);
        clean.remove(' ');
        clean.remove(':');
        clean.remove(',');
        tx = QByteArray::fromHex(clean.toLatin1());
    }

    if (tx.isEmpty()) {
        m_spiLogEdit->append("<span style='color:#ef5350;'>[ERROR] Invalid custom TX data (empty or invalid hex).</span>");
        return;
    }

    QByteArray rx;
    QString err;
    bool ok = transferSpi(dev, mode, speed, tx, rx, err);
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    if (!ok) {
        m_spiLogEdit->append(QString("<span style='color:#ef5350;'>[%1] [SPI ERROR] %2</span>").arg(timeStr, err));
        return;
    }

    QString txHex = tx.toHex(' ').toUpper();
    QString rxHex = rx.toHex(' ').toUpper();
    QString rxAscii;
    for (char c : rx) {
        rxAscii.append((c >= 32 && c <= 126) ? QChar(c) : QChar('.'));
    }

    m_spiLogEdit->append(QString("<span style='color:#8b949e;'>[%1] </span><span style='color:#69f0ae; font-weight:bold;'>[CUSTOM SPI]</span> %2 bytes on %3")
        .arg(timeStr).arg(tx.size()).arg(dev));
    m_spiLogEdit->append(QString("  <span style='color:#b0bec5;'>TX Hex:</span> %1").arg(txHex));
    m_spiLogEdit->append(QString("  <span style='color:#00d2ff;'>RX Hex:</span> %1 <span style='color:#78909c;'>(ASCII: \"%2\")</span>")
        .arg(rxHex, rxAscii));
}

void MainWindow::onClearSpiLog()
{
    if (m_spiLogEdit) {
        m_spiLogEdit->clear();
    }
}

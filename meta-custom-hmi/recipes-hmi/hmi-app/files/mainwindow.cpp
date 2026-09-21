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

    setupUi();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::updateClockAndStats);
    m_timer->start(1000);

    updateClockAndStats();
}

MainWindow::~MainWindow()
{
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
        "QTabBar::tab { background: #16202c; color: #8b949e; padding: 9px 16px; font-size: 13px; font-weight: bold; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 3px; }"
        "QTabBar::tab:selected { background: #1f2d3d; color: #00d2ff; border-bottom: 3px solid #00d2ff; }"
        "QTabBar::tab:hover { background: #1b2636; color: #e6edf3; }"
    );
    m_tabWidget->tabBar()->setExpanding(true);

    m_tabWidget->addTab(createDashboardTab(), "Dashboard");
    m_tabWidget->addTab(createTouchTestTab(), "Touch Test");
    m_tabWidget->addTab(createHardwareControlTab(), "Hardware && Display");
    m_tabWidget->addTab(createGpioTestTab(), "GPIO Test");
    m_tabWidget->addTab(createNetworkOtaTab(), "Network && OTA");
    m_tabWidget->addTab(createSystemTab(), "System && Power");

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
    m_ledToggleBtn->setFixedHeight(50);
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
    grid->addWidget(gpioBox, 0, 1);

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




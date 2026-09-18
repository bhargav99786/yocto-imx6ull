#include "mainwindow.h"
#include "touchcanvas.h"
#include "numpaddialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
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
    , m_pingProcess(nullptr)
    , m_netManager(nullptr)
{
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
        "QTabBar::tab { background: #16202c; color: #8b949e; padding: 12px 24px; font-size: 15px; font-weight: bold; border-top-left-radius: 6px; border-top-right-radius: 6px; margin-right: 4px; }"
        "QTabBar::tab:selected { background: #1f2d3d; color: #00d2ff; border-bottom: 3px solid #00d2ff; }"
        "QTabBar::tab:hover { background: #1b2636; color: #e6edf3; }"
    );

    m_tabWidget->addTab(createDashboardTab(), "System Dashboard");
    m_tabWidget->addTab(createTouchTestTab(), "Touch Screen Test");
    m_tabWidget->addTab(createHardwareControlTab(), "Hardware & Display");
    m_tabWidget->addTab(createNetworkOtaTab(), "Network & OTA");
    m_tabWidget->addTab(createSystemTab(), "System & Power");

    mainLayout->addWidget(m_tabWidget);
}

QWidget *MainWindow::createDashboardTab()
{
    QWidget *tab = new QWidget(this);
    QGridLayout *grid = new QGridLayout(tab);
    grid->setContentsMargins(16, 16, 16, 16);
    grid->setSpacing(16);

    auto makeCard = [](const QString &title, const QString &text, const QString &accentCol) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(title);
        box->setStyleSheet(QString(
            "QGroupBox { font-size: 14px; font-weight: bold; color: %1; border: 1px solid #233242; border-radius: 8px; margin-top: 8px; padding-top: 14px; background-color: #141c26; }"
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 8px; }"
        ).arg(accentCol));
        QVBoxLayout *l = new QVBoxLayout(box);
        QLabel *lbl = new QLabel(text, box);
        lbl->setStyleSheet("font-size: 15px; color: #c9d1d9; font-weight: normal;");
        lbl->setWordWrap(true);
        l->addWidget(lbl);
        return box;
    };

    m_cpuModelLabel = new QLabel("ARM Cortex-A7 @ 528 MHz\nNXP i.MX6ULL Processor", tab);
    m_cpuModelLabel->setStyleSheet("font-size: 14px; color: #e6edf3;");

    m_kernelLabel = new QLabel("Linux 6.1.57-fslc\nYocto Kirkstone (Poky 4.0)", tab);
    m_kernelLabel->setStyleSheet("font-size: 14px; color: #e6edf3;");

    m_uptimeLabel = new QLabel("Uptime: --", tab);
    m_uptimeLabel->setStyleSheet("font-size: 15px; color: #00d2ff; font-weight: bold;");

    QGroupBox *boxCpu = makeCard("Processor & Architecture", "", "#00d2ff");
    boxCpu->layout()->addWidget(m_cpuModelLabel);

    QGroupBox *boxOs = makeCard("Operating System & Kernel", "", "#4caf50");
    boxOs->layout()->addWidget(m_kernelLabel);

    QGroupBox *boxUptime = makeCard("System Uptime", "", "#ff9800");
    boxUptime->layout()->addWidget(m_uptimeLabel);

    // RAM Card
    QGroupBox *boxRam = new QGroupBox("Memory (RAM) Utilization", tab);
    boxRam->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #e040fb; border: 1px solid #233242; border-radius: 8px; margin-top: 8px; padding-top: 14px; background-color: #141c26; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 8px; }"
    );
    QVBoxLayout *ramLayout = new QVBoxLayout(boxRam);
    m_ramBar = new QProgressBar(boxRam);
    m_ramBar->setRange(0, 100);
    m_ramBar->setValue(25);
    m_ramBar->setFixedHeight(24);
    m_ramBar->setTextVisible(false);
    m_ramBar->setStyleSheet(
        "QProgressBar { background-color: #1c2633; border: 1px solid #2a3b4c; border-radius: 4px; }"
        "QProgressBar::chunk { background-color: #e040fb; border-radius: 4px; }"
    );
    m_ramTextLabel = new QLabel("RAM: -- / -- MB", boxRam);
    m_ramTextLabel->setStyleSheet("font-size: 13px; color: #c9d1d9;");
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
    toolsPanel->setFixedWidth(180);
    toolsPanel->setStyleSheet("background-color: #141c26; border: 1px solid #233242; border-radius: 8px; padding: 6px;");
    QVBoxLayout *vTools = new QVBoxLayout(toolsPanel);
    vTools->setSpacing(10);

    QLabel *toolTitle = new QLabel("Color Palette", toolsPanel);
    toolTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #00d2ff;");
    vTools->addWidget(toolTitle);

    auto makeColorBtn = [this, vTools, toolsPanel](const QString &name, const QColor &c) {
        QPushButton *btn = new QPushButton(name, toolsPanel);
        btn->setFixedHeight(38);
        btn->setStyleSheet(QString("background-color: %1; color: %2; font-weight: bold; border-radius: 6px; font-size: 13px;")
            .arg(c.name()).arg(c.lightness() > 150 ? "#000000" : "#ffffff"));
        connect(btn, &QPushButton::clicked, this, [this, c]() { onColorButtonClicked(c); });
        vTools->addWidget(btn);
    };

    makeColorBtn("Cyan", QColor(0, 220, 255));
    makeColorBtn("Yellow", QColor(255, 235, 59));
    makeColorBtn("Green", QColor(76, 175, 80));
    makeColorBtn("Red", QColor(244, 67, 54));
    makeColorBtn("White", QColor(240, 244, 248));

    vTools->addSpacing(10);

    QPushButton *clearBtn = new QPushButton("Clear Canvas", toolsPanel);
    clearBtn->setFixedHeight(44);
    clearBtn->setStyleSheet("background-color: #2e3846; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px; border: 1px solid #455a64;");
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_canvas) m_canvas->clearCanvas();
    });
    vTools->addWidget(clearBtn);

    vTools->addStretch();

    m_touchCoordLabel = new QLabel("X: --\nY: --", toolsPanel);
    m_touchCoordLabel->setStyleSheet("background: #0d1117; color: #00d2ff; font-family: monospace; font-size: 14px; font-weight: bold; padding: 8px; border-radius: 6px; border: 1px solid #233242;");
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
    grid->setContentsMargins(20, 20, 20, 20);
    grid->setSpacing(20);

    // Backlight Box
    QGroupBox *blBox = new QGroupBox("LCD Backlight Intensity", tab);
    blBox->setStyleSheet(
        "QGroupBox { font-size: 15px; font-weight: bold; color: #00d2ff; border: 1px solid #233242; border-radius: 8px; margin-top: 8px; padding-top: 16px; background-color: #141c26; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 8px; }"
    );
    QVBoxLayout *blLayout = new QVBoxLayout(blBox);

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

    m_brightnessValueLabel = new QLabel("Brightness: 7 / 7 (100%)", blBox);
    m_brightnessValueLabel->setStyleSheet("font-size: 14px; color: #e6edf3; font-weight: bold;");

    blLayout->addWidget(m_brightnessValueLabel);
    blLayout->addWidget(m_brightnessSlider);

    // GPIO & LED Box
    QGroupBox *gpioBox = new QGroupBox("Industrial I/O & Relay Simulation", tab);
    gpioBox->setStyleSheet(
        "QGroupBox { font-size: 15px; font-weight: bold; color: #4caf50; border: 1px solid #233242; border-radius: 8px; margin-top: 8px; padding-top: 16px; background-color: #141c26; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 8px; }"
    );
    QVBoxLayout *gpioLayout = new QVBoxLayout(gpioBox);

    m_ledToggleBtn = new QPushButton("Carrier Board LED / Relay: OFF", gpioBox);
    m_ledToggleBtn->setFixedHeight(50);
    m_ledToggleBtn->setStyleSheet("background-color: #21262d; color: #8b949e; font-size: 15px; font-weight: bold; border-radius: 8px; border: 2px solid #30363d;");
    connect(m_ledToggleBtn, &QPushButton::clicked, this, [this]() {
        m_ledState = !m_ledState;
        if (m_ledState) {
            m_ledToggleBtn->setText("Carrier Board LED / Relay: ON");
            m_ledToggleBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-size: 15px; font-weight: bold; border-radius: 8px; border: 2px solid #4caf50;");
        } else {
            m_ledToggleBtn->setText("Carrier Board LED / Relay: OFF");
            m_ledToggleBtn->setStyleSheet("background-color: #21262d; color: #8b949e; font-size: 15px; font-weight: bold; border-radius: 8px; border: 2px solid #30363d;");
        }
    });
    gpioLayout->addWidget(m_ledToggleBtn);

    grid->addWidget(blBox, 0, 0);
    grid->addWidget(gpioBox, 0, 1);

    return tab;
}

QWidget *MainWindow::createSystemTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *vLayout = new QVBoxLayout(tab);
    vLayout->setContentsMargins(24, 24, 24, 24);
    vLayout->setSpacing(20);

    QLabel *desc = new QLabel("System Power & Maintenance", tab);
    desc->setStyleSheet("font-size: 18px; font-weight: bold; color: #00d2ff;");
    vLayout->addWidget(desc);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(24);

    QPushButton *rebootBtn = new QPushButton("Restart System (Reboot)", tab);
    rebootBtn->setFixedHeight(55);
    rebootBtn->setStyleSheet("background-color: #f57c00; color: #ffffff; font-size: 16px; font-weight: bold; border-radius: 8px;");
    connect(rebootBtn, &QPushButton::clicked, this, &MainWindow::onRebootClicked);

    QPushButton *shutdownBtn = new QPushButton("Power Off (Shutdown)", tab);
    shutdownBtn->setFixedHeight(55);
    shutdownBtn->setStyleSheet("background-color: #d32f2f; color: #ffffff; font-size: 16px; font-weight: bold; border-radius: 8px;");
    connect(shutdownBtn, &QPushButton::clicked, this, &MainWindow::onShutdownClicked);

    btnLayout->addWidget(rebootBtn);
    btnLayout->addWidget(shutdownBtn);

    vLayout->addLayout(btnLayout);
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
    m_touchCoordLabel->setText(QString("Touch:\nX: %1\nY: %2\nState: %3")
        .arg(x).arg(y).arg(isDown ? "DOWN" : "UP"));
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
    mainHLayout->setContentsMargins(14, 12, 14, 12);
    mainHLayout->setSpacing(14);

    auto makeCard = [](const QString &title, const QString &accentCol) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(title);
        box->setStyleSheet(QString(
            "QGroupBox { font-size: 14px; font-weight: bold; color: %1; border: 1px solid #233242; border-radius: 8px; margin-top: 8px; padding-top: 14px; background-color: #141c26; }"
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 8px; }"
        ).arg(accentCol));
        return box;
    };

    // ================= LEFT COLUMN: ETHERNET & PING =================
    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->setSpacing(10);

    // 1. Ethernet Status Card
    QGroupBox *ethBox = makeCard("Ethernet Interfaces (eth0 / eth1)", "#00d2ff");
    QVBoxLayout *ethLayout = new QVBoxLayout(ethBox);
    ethLayout->setSpacing(8);

    m_eth0StatusLabel = new QLabel("eth0: Checking...", ethBox);
    m_eth0StatusLabel->setStyleSheet("font-size: 13px; color: #ffffff; font-family: monospace;");
    ethLayout->addWidget(m_eth0StatusLabel);

    m_eth1StatusLabel = new QLabel("eth1: Checking...", ethBox);
    m_eth1StatusLabel->setStyleSheet("font-size: 13px; color: #ffffff; font-family: monospace;");
    ethLayout->addWidget(m_eth1StatusLabel);

    QHBoxLayout *dhcpBtnLayout = new QHBoxLayout();
    m_renewDhcpBtn = new QPushButton("Renew Auto-IP (DHCP)", ethBox);
    m_renewDhcpBtn->setFixedHeight(40);
    m_renewDhcpBtn->setStyleSheet("background-color: #0288d1; color: #ffffff; font-size: 13px; font-weight: bold; border-radius: 6px;");
    connect(m_renewDhcpBtn, &QPushButton::clicked, this, &MainWindow::onRenewDhcp);

    m_dhcpStatusLabel = new QLabel("DHCP: Ready", ethBox);
    m_dhcpStatusLabel->setStyleSheet("font-size: 12px; color: #81c784;");

    dhcpBtnLayout->addWidget(m_renewDhcpBtn);
    dhcpBtnLayout->addWidget(m_dhcpStatusLabel);
    ethLayout->addLayout(dhcpBtnLayout);
    leftCol->addWidget(ethBox);

    // 2. Ping Test Card
    QGroupBox *pingBox = makeCard("Network Connectivity Test (Ping)", "#4caf50");
    QVBoxLayout *pingLayout = new QVBoxLayout(pingBox);
    pingLayout->setSpacing(8);

    QHBoxLayout *targetLayout = new QHBoxLayout();
    m_pingTargetEdit = new QLineEdit("192.168.1.1", pingBox);
    m_pingTargetEdit->setFixedHeight(38);
    m_pingTargetEdit->setReadOnly(true);
    m_pingTargetEdit->setStyleSheet("background-color: #1a2432; color: #ffffff; font-size: 14px; font-weight: bold; padding: 0 8px; border: 1px solid #334d66; border-radius: 6px;");

    QPushButton *editTargetBtn = new QPushButton("Touch to Edit", pingBox);
    editTargetBtn->setFixedHeight(38);
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
        b->setFixedHeight(30);
        b->setStyleSheet("background-color: #1f2d3d; color: #90caf9; font-size: 11px; border-radius: 4px;");
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
    m_runPingBtn->setFixedHeight(42);
    m_runPingBtn->setStyleSheet("background-color: #2e7d32; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px;");
    connect(m_runPingBtn, &QPushButton::clicked, this, &MainWindow::onRunPing);
    pingLayout->addWidget(m_runPingBtn);

    m_pingResultLabel = new QLabel("Result: Ready to test", pingBox);
    m_pingResultLabel->setFixedHeight(38);
    m_pingResultLabel->setStyleSheet("background-color: #0d131a; color: #b0bec5; font-size: 12px; font-family: monospace; border: 1px solid #1e2c3c; border-radius: 4px; padding: 4px;");
    pingLayout->addWidget(m_pingResultLabel);

    leftCol->addWidget(pingBox);
    mainHLayout->addLayout(leftCol, 1);

    // ================= RIGHT COLUMN: OTA & DUAL-BANK =================
    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->setSpacing(10);

    // 3. OTA System & Dual-Bank Card
    QGroupBox *otaInfoBox = makeCard("System Version & Dual-Bank Status", "#ab47bc");
    QVBoxLayout *otaInfoLayout = new QVBoxLayout(otaInfoBox);
    otaInfoLayout->setSpacing(8);

    m_otaVersionLabel = new QLabel(QString("Installed Version: %1").arg(getInstalledOtaVersion()), otaInfoBox);
    m_otaVersionLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #e1bee7;");
    otaInfoLayout->addWidget(m_otaVersionLabel);

    m_otaBankLabel = new QLabel(QString("Active RootFS: %1").arg(getActiveBootBank()), otaInfoBox);
    m_otaBankLabel->setStyleSheet("font-size: 13px; color: #ce93d8;");
    otaInfoLayout->addWidget(m_otaBankLabel);

    rightCol->addWidget(otaInfoBox);

    // 4. OTA Server Configuration Card
    QGroupBox *serverBox = makeCard("OTA Server Endpoint", "#ff9800");
    QVBoxLayout *serverLayout = new QVBoxLayout(serverBox);
    serverLayout->setSpacing(8);

    m_otaServerLabel = new QLabel(QString("Server: %1").arg(m_otaServerUrl), serverBox);
    m_otaServerLabel->setStyleSheet("font-size: 13px; color: #ffe0b2; font-family: monospace;");
    m_otaServerLabel->setWordWrap(true);
    serverLayout->addWidget(m_otaServerLabel);

    QPushButton *editServerBtn = new QPushButton("Edit Server URL", serverBox);
    editServerBtn->setFixedHeight(40);
    editServerBtn->setStyleSheet("background-color: #e65100; color: #ffffff; font-size: 13px; font-weight: bold; border-radius: 6px;");
    connect(editServerBtn, &QPushButton::clicked, this, &MainWindow::onEditServerClicked);
    serverLayout->addWidget(editServerBtn);

    rightCol->addWidget(serverBox);

    // 5. Update Checker & Action Card
    QGroupBox *updateBox = makeCard("Firmware Update Management", "#00e676");
    QVBoxLayout *updateLayout = new QVBoxLayout(updateBox);
    updateLayout->setSpacing(8);

    m_checkUpdateBtn = new QPushButton("Check for Update", updateBox);
    m_checkUpdateBtn->setFixedHeight(42);
    m_checkUpdateBtn->setStyleSheet("background-color: #00897b; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px;");
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &MainWindow::onCheckOtaUpdate);
    updateLayout->addWidget(m_checkUpdateBtn);

    m_installUpdateBtn = new QPushButton("Install Update Now", updateBox);
    m_installUpdateBtn->setFixedHeight(42);
    m_installUpdateBtn->setStyleSheet("background-color: #00c853; color: #ffffff; font-size: 14px; font-weight: bold; border-radius: 6px;");
    m_installUpdateBtn->setVisible(false);
    connect(m_installUpdateBtn, &QPushButton::clicked, this, &MainWindow::onInstallOtaUpdate);
    updateLayout->addWidget(m_installUpdateBtn);

    m_otaStatusLabel = new QLabel("Status: Idle", updateBox);
    m_otaStatusLabel->setFixedHeight(36);
    m_otaStatusLabel->setStyleSheet("background-color: #0d131a; color: #b2dfdb; font-size: 12px; border: 1px solid #1e2c3c; border-radius: 4px; padding: 4px;");
    m_otaStatusLabel->setWordWrap(true);
    updateLayout->addWidget(m_otaStatusLabel);

    rightCol->addWidget(updateBox);
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


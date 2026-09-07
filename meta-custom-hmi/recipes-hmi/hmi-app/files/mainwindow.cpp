#include "mainwindow.h"
#include "touchcanvas.h"

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_ledState(false)
{
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
    makeColorBtn("White", QColor(255, 255, 255));

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
    for (const QHostAddress &address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            return address.toString();
        }
    }
    return "Disconnected";
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

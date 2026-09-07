#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QSlider>
#include <QPushButton>
#include <QProgressBar>
#include <QTabWidget>

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

private:
    void setupUi();
    QWidget *createDashboardTab();
    QWidget *createTouchTestTab();
    QWidget *createHardwareControlTab();
    QWidget *createSystemTab();
    QString getIpAddress();
    float getCpuTemperature();
    QString getSystemUptime();
    void getMemoryUsage(int &totalMb, int &usedMb);

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

    QTimer *m_timer;
};

#endif // MAINWINDOW_H

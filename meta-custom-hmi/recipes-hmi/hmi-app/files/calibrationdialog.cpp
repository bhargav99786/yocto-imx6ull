#include "calibrationdialog.h"
#include "touchcalibration.h"
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <cmath>

CalibrationDialog::CalibrationDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      m_currentStep(0),
      m_pulsePhase(0)
{
    m_tapDebounce.invalidate();
    setFixedSize(1024, 600);
    setStyleSheet("background-color: #0a0e14;");

    // 4 Reference Calibration Targets:
    // P0: Top-Left (60, 60)
    // P1: Top-Right (964, 60)
    // P2: Bottom-Right (964, 540)
    // P3: Bottom-Left (60, 540)
    m_targetPoints.append(QPoint(60, 60));
    m_targetPoints.append(QPoint(964, 60));
    m_targetPoints.append(QPoint(964, 540));
    m_targetPoints.append(QPoint(60, 540));

    m_measuredPoints.resize(4);

    // Center instruction card
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addStretch(1);

    m_titleLabel = new QLabel("TOUCHSCREEN CALIBRATION WIZARD", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #00d2ff; letter-spacing: 2px;");
    mainLayout->addWidget(m_titleLabel);

    mainLayout->addSpacing(10);

    m_instructionLabel = new QLabel("Tap the center of the crosshair (Point 1 of 4)", this);
    m_instructionLabel->setAlignment(Qt::AlignCenter);
    m_instructionLabel->setStyleSheet("font-size: 16px; color: #e6edf3; font-weight: normal;");
    mainLayout->addWidget(m_instructionLabel);

    mainLayout->addSpacing(8);

    m_statusLabel = new QLabel("Align your finger precisely with the bullseye", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 13px; color: #8b949e;");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addSpacing(24);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_skipBtn = new QPushButton("Skip / Keep Default (1:1)", this);
    m_skipBtn->setFixedSize(210, 42);
    m_skipBtn->setStyleSheet(
        "QPushButton { background-color: #161b22; color: #58a6ff; font-size: 13px; font-weight: bold; border: 1px solid #30363d; border-radius: 6px; }"
        "QPushButton:hover { background-color: #21262d; border-color: #58a6ff; }"
    );
    connect(m_skipBtn, &QPushButton::clicked, this, &CalibrationDialog::onSkipClicked);
    btnLayout->addWidget(m_skipBtn);

    btnLayout->addSpacing(16);

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setFixedSize(140, 42);
    m_cancelBtn->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #f85149; font-size: 13px; font-weight: bold; border: 1px solid #30363d; border-radius: 6px; }"
        "QPushButton:hover { background-color: #30363d; }"
    );
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    mainLayout->addStretch(1);

    // Pulse animation timer for the crosshair
    m_pulseTimer = new QTimer(this);
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulsePhase = (m_pulsePhase + 1) % 20;
        update();
    });
    m_pulseTimer->start(60);
}

void CalibrationDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_currentStep < m_targetPoints.size()) {
        QPoint target = m_targetPoints[m_currentStep];

        // Animated target ring
        int pulseOffset = m_pulsePhase - 10;
        int radius1 = 20 + (pulseOffset > 0 ? pulseOffset : -pulseOffset) / 2;
        int radius2 = 34;

        // Glowing outer circle
        p.setPen(QPen(QColor(0, 210, 255, 120), 2, Qt::DashLine));
        p.drawEllipse(target, radius2, radius2);

        // Active inner circle
        p.setPen(QPen(QColor(255, 64, 129), 2));
        p.setBrush(QColor(255, 64, 129, 40));
        p.drawEllipse(target, radius1, radius1);

        // Center dot
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255));
        p.drawEllipse(target, 4, 4);

        // Crosshairs
        p.setPen(QPen(QColor(255, 64, 129), 2));
        p.drawLine(target.x() - 40, target.y(), target.x() - 8, target.y());
        p.drawLine(target.x() + 8, target.y(), target.x() + 40, target.y());
        p.drawLine(target.x(), target.y() - 40, target.x(), target.y() - 8);
        p.drawLine(target.x(), target.y() + 8, target.x(), target.y() + 40);

        // Corner label
        p.setPen(QColor(0, 210, 255));
        QFont f = p.font();
        f.setPointSize(11);
        f.setBold(true);
        p.setFont(f);

        QString cornerName;
        switch (m_currentStep) {
            case 0: cornerName = "[ 1. TOP-LEFT ]"; break;
            case 1: cornerName = "[ 2. TOP-RIGHT ]"; break;
            case 2: cornerName = "[ 3. BOTTOM-RIGHT ]"; break;
            case 3: cornerName = "[ 4. BOTTOM-LEFT ]"; break;
        }

        int textX = (target.x() < 512) ? target.x() + 48 : target.x() - 170;
        int textY = (target.y() < 300) ? target.y() + 25 : target.y() - 15;
        p.drawText(textX, textY, cornerName);
    }
}

void CalibrationDialog::mousePressEvent(QMouseEvent *event)
{
    if (m_currentStep >= m_targetPoints.size()) return;

    // 1. Debounce: ignore touches faster than 300ms from previous accepted tap
    if (m_tapDebounce.isValid() && m_tapDebounce.elapsed() < 300) {
        return;
    }

    QPoint rawPos = event->pos();
    QPoint target = m_targetPoints[m_currentStep];

    // 2. Proximity check: Must be within 85px of the bullseye target
    int dx = rawPos.x() - target.x();
    int dy = rawPos.y() - target.y();
    double dist = std::sqrt(dx * dx + dy * dy);

    if (dist > 85.0) {
        qWarning() << "Calibration tap rejected: too far from target. Dist=" << dist
                   << "Raw=" << rawPos << "Target=" << target;
        m_statusLabel->setText(
            QString("⚠️ Tap was too far from target (%1px away). Please tap directly on the crosshair.")
                .arg(qRound(dist)));
        m_statusLabel->setStyleSheet("font-size: 13px; color: #ffa726; font-weight: bold;");
        return;
    }

    // 3. Clean hit within target radius!
    m_measuredPoints[m_currentStep] = rawPos;
    m_tapDebounce.start();

    qDebug() << "Calibration Step" << m_currentStep
             << "Target:" << target
             << "Raw Measured:" << rawPos
             << "Dist:" << dist;

    m_currentStep++;

    if (m_currentStep < m_targetPoints.size()) {
        m_instructionLabel->setText(
            QString("Tap the center of the crosshair (Point %1 of 4)").arg(m_currentStep + 1));
        m_statusLabel->setText(QString("✓ Target %1 recorded at (%2, %3)")
                                   .arg(m_currentStep)
                                   .arg(rawPos.x())
                                   .arg(rawPos.y()));
        m_statusLabel->setStyleSheet("font-size: 13px; color: #00e676; font-weight: bold;");
        update();
    } else {
        m_pulseTimer->stop();
        calculateAndFinish();
    }
}

void CalibrationDialog::calculateAndFinish()
{
    // Expected Target geometry
    // Target dx: 964 - 60 = 904
    // Target dy: 540 - 60 = 480
    double target_dx = 904.0;
    double target_dy = 480.0;

    // Measured top/bottom horizontal spans
    double measured_dx_top = m_measuredPoints[1].x() - m_measuredPoints[0].x();
    double measured_dx_bot = m_measuredPoints[2].x() - m_measuredPoints[3].x();
    double measured_dx = (measured_dx_top + measured_dx_bot) / 2.0;

    // Measured left/right vertical spans
    double measured_dy_left = m_measuredPoints[3].y() - m_measuredPoints[0].y();
    double measured_dy_right = m_measuredPoints[2].y() - m_measuredPoints[1].y();
    double measured_dy = (measured_dy_left + measured_dy_right) / 2.0;

    // Sanity check spans: must be within reasonable physical bounds
    bool spanError = (qAbs(measured_dx_top - target_dx) > 100.0 ||
                      qAbs(measured_dx_bot - target_dx) > 100.0 ||
                      qAbs(measured_dy_left - target_dy) > 80.0 ||
                      qAbs(measured_dy_right - target_dy) > 80.0);

    if (spanError || qAbs(measured_dx) < 700.0 || qAbs(measured_dy) < 350.0) {
        m_instructionLabel->setText("Calibration Inconsistent: Taps were distorted");
        m_instructionLabel->setStyleSheet("font-size: 16px; color: #f85149; font-weight: bold;");
        m_statusLabel->setText("Please tap carefully on all four crosshairs. Restarting...");
        m_statusLabel->setStyleSheet("font-size: 13px; color: #ffa726; font-weight: bold;");
        m_currentStep = 0;
        m_pulseTimer->start(60);
        update();
        return;
    }

    double scale_x = target_dx / measured_dx;
    double scale_y = target_dy / measured_dy;

    // Average measured Top-Left point
    double p0_meas_x = (m_measuredPoints[0].x() + m_measuredPoints[3].x()) / 2.0;
    double p0_meas_y = (m_measuredPoints[0].y() + m_measuredPoints[1].y()) / 2.0;

    double offset_x = 60.0 - (scale_x * p0_meas_x);
    double offset_y = 60.0 - (scale_y * p0_meas_y);

    qDebug() << "Calibration Result: ScaleX=" << scale_x << "ScaleY=" << scale_y
             << "OffsetX=" << offset_x << "OffsetY=" << offset_y;

    bool applied = TouchCalibration::instance().setCalibration(scale_x, scale_y, offset_x, offset_y);
    if (!applied) {
        m_instructionLabel->setText("Calibration Out-of-Bounds — Reverted to Safe 1:1");
        m_instructionLabel->setStyleSheet("font-size: 16px; color: #ffa726; font-weight: bold;");
        m_statusLabel->setText("Shifts exceeded safe physical limits (+-40px). Safe 1:1 default kept.");
        m_statusLabel->setStyleSheet("font-size: 13px; color: #8b949e;");
        TouchCalibration::instance().reset();
        QTimer::singleShot(1800, this, &QDialog::accept);
        return;
    }

    m_instructionLabel->setText("Calibration Successful!");
    m_instructionLabel->setStyleSheet("font-size: 18px; color: #00e676; font-weight: bold;");

    m_statusLabel->setText(
        QString("Shift X: %1px  |  Shift Y: %2px  |  Scale: %3x")
            .arg(qRound(offset_x))
            .arg(qRound(offset_y))
            .arg(QString::number(scale_x, 'f', 2)));
    m_statusLabel->setStyleSheet("font-size: 14px; color: #ffffff; font-weight: bold;");

    m_cancelBtn->setVisible(false);
    if (m_skipBtn) m_skipBtn->setVisible(false);
    update();

    QTimer::singleShot(1400, this, &QDialog::accept);
}

void CalibrationDialog::onSkipClicked()
{
    TouchCalibration::instance().reset();
    TouchCalibration::instance().save();
    accept();
}



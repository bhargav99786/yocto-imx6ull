#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include <QDialog>
#include <QPoint>
#include <QVector>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

class CalibrationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationDialog(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void calculateAndFinish();

    int m_currentStep;
    QVector<QPoint> m_targetPoints;
    QVector<QPoint> m_measuredPoints;

    QLabel *m_titleLabel;
    QLabel *m_instructionLabel;
    QLabel *m_statusLabel;
    QPushButton *m_cancelBtn;
    QTimer *m_pulseTimer;
    int m_pulsePhase;
};

#endif // CALIBRATIONDIALOG_H

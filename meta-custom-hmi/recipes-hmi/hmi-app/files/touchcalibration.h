#ifndef TOUCHCALIBRATION_H
#define TOUCHCALIBRATION_H

#include <QObject>
#include <QPointF>
#include <QString>
#include <QEvent>

class TouchCalibration : public QObject
{
    Q_OBJECT

public:
    static TouchCalibration &instance();

    bool isCalibrated() const { return m_calibrated; }
    double scaleX() const { return m_scaleX; }
    double scaleY() const { return m_scaleY; }
    double offsetX() const { return m_offsetX; }
    double offsetY() const { return m_offsetY; }

    QPointF map(const QPointF &p) const;
    QPoint map(const QPoint &p) const;

    bool setCalibration(double sx, double sy, double ox, double oy);
    void reset();
    bool load(const QString &path = "/etc/touch-calibration.json");
    bool save(const QString &path = "/etc/touch-calibration.json");

    QString statusString() const;

signals:
    void calibrationChanged();

private:
    TouchCalibration();
    ~TouchCalibration() = default;
    TouchCalibration(const TouchCalibration &) = delete;
    TouchCalibration &operator=(const TouchCalibration &) = delete;

    bool m_calibrated;
    double m_scaleX;
    double m_scaleY;
    double m_offsetX;
    double m_offsetY;
    QString m_filePath;
};

class TouchCalibrationFilter : public QObject
{
    Q_OBJECT

public:
    explicit TouchCalibrationFilter(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool m_inDispatch;
};

#endif // TOUCHCALIBRATION_H

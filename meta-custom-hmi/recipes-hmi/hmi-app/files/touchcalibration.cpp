#include "touchcalibration.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QApplication>
#include <QWidget>
#include <QDebug>

TouchCalibration &TouchCalibration::instance()
{
    static TouchCalibration s_instance;
    return s_instance;
}

TouchCalibration::TouchCalibration()
    : m_calibrated(false),
      m_scaleX(1.0),
      m_scaleY(1.0),
      m_offsetX(0.0),
      m_offsetY(0.0),
      m_filePath("/etc/touch-calibration.json")
{
    load();
}

QPointF TouchCalibration::map(const QPointF &p) const
{
    if (!m_calibrated) return p;
    double nx = p.x() * m_scaleX + m_offsetX;
    double ny = p.y() * m_scaleY + m_offsetY;
    if (nx < 0.0) nx = 0.0;
    if (nx > 1023.0) nx = 1023.0;
    if (ny < 0.0) ny = 0.0;
    if (ny > 599.0) ny = 599.0;
    return QPointF(nx, ny);
}

QPoint TouchCalibration::map(const QPoint &p) const
{
    QPointF pf = map(QPointF(p.x(), p.y()));
    return QPoint(qRound(pf.x()), qRound(pf.y()));
}

void TouchCalibration::setCalibration(double sx, double sy, double ox, double oy)
{
    // Sanity checks: scale must be between 0.5 and 2.0, offsets within [-300, 300]
    if (sx < 0.5 || sx > 2.0 || sy < 0.5 || sy > 2.0) {
        qWarning("TouchCalibration: Out-of-bounds scale factors, ignoring");
        return;
    }
    if (qAbs(ox) > 300.0 || qAbs(oy) > 300.0) {
        qWarning("TouchCalibration: Out-of-bounds offsets, ignoring");
        return;
    }

    m_scaleX = sx;
    m_scaleY = sy;
    m_offsetX = ox;
    m_offsetY = oy;
    m_calibrated = (qAbs(sx - 1.0) > 0.005 || qAbs(sy - 1.0) > 0.005 || qAbs(ox) > 2.0 || qAbs(oy) > 2.0);

    save();
    emit calibrationChanged();
}

void TouchCalibration::reset()
{
    m_calibrated = false;
    m_scaleX = 1.0;
    m_scaleY = 1.0;
    m_offsetX = 0.0;
    m_offsetY = 0.0;

    QFile::remove(m_filePath);
    emit calibrationChanged();
}

bool TouchCalibration::load(const QString &path)
{
    m_filePath = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray data = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;

    QJsonObject obj = doc.object();
    m_scaleX = obj.value("scale_x").toDouble(1.0);
    m_scaleY = obj.value("scale_y").toDouble(1.0);
    m_offsetX = obj.value("offset_x").toDouble(0.0);
    m_offsetY = obj.value("offset_y").toDouble(0.0);
    m_calibrated = obj.value("calibrated").toBool(false);

    if (m_scaleX < 0.5 || m_scaleX > 2.0) m_scaleX = 1.0;
    if (m_scaleY < 0.5 || m_scaleY > 2.0) m_scaleY = 1.0;

    return true;
}

bool TouchCalibration::save(const QString &path)
{
    m_filePath = path;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("Failed to open %s for writing touch calibration", qPrintable(path));
        return false;
    }

    QJsonObject obj;
    obj["calibrated"] = m_calibrated;
    obj["scale_x"] = m_scaleX;
    obj["scale_y"] = m_scaleY;
    obj["offset_x"] = m_offsetX;
    obj["offset_y"] = m_offsetY;

    QJsonDocument doc(obj);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

QString TouchCalibration::statusString() const
{
    if (!m_calibrated) {
        return "Status: Default 1:1 (Uncalibrated)";
    }
    return QString("Status: Calibrated (Shift X: %1px, Shift Y: %2px)")
        .arg(qRound(m_offsetX))
        .arg(qRound(m_offsetY));
}

// -------------------------------------------------------------
// TouchCalibrationFilter implementation
// -------------------------------------------------------------
TouchCalibrationFilter::TouchCalibrationFilter(QObject *parent)
    : QObject(parent), m_inDispatch(false)
{
}

bool TouchCalibrationFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (m_inDispatch || !TouchCalibration::instance().isCalibrated()) {
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseMove) {

        QMouseEvent *me = static_cast<QMouseEvent *>(event);

        QPointF rawWindowPos = me->windowPos();
        QPointF calWindowPos = TouchCalibration::instance().map(rawWindowPos);

        // If offset is negligible, proceed normally
        if (qAbs(rawWindowPos.x() - calWindowPos.x()) < 0.5 &&
            qAbs(rawWindowPos.y() - calWindowPos.y()) < 0.5) {
            return QObject::eventFilter(watched, event);
        }

        QWidget *activeWin = QApplication::activeWindow();
        if (!activeWin) {
            activeWin = qobject_cast<QWidget *>(watched);
            if (activeWin) activeWin = activeWin->window();
        }
        if (!activeWin) return QObject::eventFilter(watched, event);

        // Find the specific widget at the calibrated coordinate
        QWidget *target = activeWin->childAt(calWindowPos.toPoint());
        if (!target) target = activeWin;

        QPoint localPos = target->mapFrom(activeWin, calWindowPos.toPoint());

        QMouseEvent newEvent(me->type(),
                             localPos,
                             calWindowPos,
                             activeWin->mapToGlobal(calWindowPos.toPoint()),
                             me->button(),
                             me->buttons(),
                             me->modifiers(),
                             me->source());

        m_inDispatch = true;
        QApplication::sendEvent(target, &newEvent);
        m_inDispatch = false;

        return true; // Filter out the original uncalibrated event
    }

    return QObject::eventFilter(watched, event);
}

#include "touchcanvas.h"
#include <QPainter>
#include <QMouseEvent>

TouchCanvas::TouchCanvas(QWidget *parent)
    : QWidget(parent)
    , m_penColor(QColor(0, 220, 255)) // Cyan default
    , m_penWidth(6)
    , m_drawing(false)
{
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setStyleSheet("background-color: #121820; border: 2px solid #2a3b4c; border-radius: 8px;");
}

void TouchCanvas::setPenColor(const QColor &color)
{
    m_penColor = color;
}

void TouchCanvas::setPenWidth(int width)
{
    m_penWidth = width;
}

void TouchCanvas::clearCanvas()
{
    m_image.fill(QColor(18, 24, 32));
    update();
}

void TouchCanvas::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawImage(QPoint(0, 0), m_image);
}

void TouchCanvas::resizeEvent(QResizeEvent *event)
{
    if (width() > m_image.width() || height() > m_image.height()) {
        int newW = qMax(width() + 128, m_image.width());
        int newH = qMax(height() + 128, m_image.height());
        QImage newImage(newW, newH, QImage::Format_RGB32);
        newImage.fill(QColor(18, 24, 32));

        QPainter painter(&newImage);
        painter.drawImage(QPoint(0, 0), m_image);
        m_image = newImage;
    }
    QWidget::resizeEvent(event);
}

void TouchCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_lastPoint = event->pos();
        m_lastEmittedPos = event->pos();
        m_drawing = true;
        drawLineTo(event->pos());
        emit touchCoordinatesChanged(event->x(), event->y(), true);
    }
}

void TouchCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && m_drawing) {
        drawLineTo(event->pos());
        if ((event->pos() - m_lastEmittedPos).manhattanLength() > 20) {
            m_lastEmittedPos = event->pos();
            emit touchCoordinatesChanged(event->x(), event->y(), true);
        }
    }
}

void TouchCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drawing) {
        drawLineTo(event->pos());
        m_drawing = false;
        m_lastEmittedPos = event->pos();
        emit touchCoordinatesChanged(event->x(), event->y(), false);
    }
}

void TouchCanvas::drawLineTo(const QPoint &endPoint)
{
    QPainter painter(&m_image);
    painter.setPen(QPen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawLine(m_lastPoint, endPoint);

    int rad = (m_penWidth / 2) + 2;
    update(QRect(m_lastPoint, endPoint).normalized().adjusted(-rad, -rad, +rad, +rad));
    m_lastPoint = endPoint;
}

#ifndef TOUCHCANVAS_H
#define TOUCHCANVAS_H

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QColor>
#include <QVector>

class TouchCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit TouchCanvas(QWidget *parent = nullptr);

    void setPenColor(const QColor &color);
    void setPenWidth(int width);
    void clearCanvas();
    QColor penColor() const { return m_penColor; }

signals:
    void touchCoordinatesChanged(int x, int y, bool isDown);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void drawLineTo(const QPoint &endPoint);

    QImage m_image;
    QPoint m_lastPoint;
    QColor m_penColor;
    int m_penWidth;
    bool m_drawing;
};

#endif // TOUCHCANVAS_H

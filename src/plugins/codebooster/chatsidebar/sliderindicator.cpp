#include "SliderIndicator.h"
#include <QPainter>
#include <QColor>
#include <QTimer>

SliderIndicator::SliderIndicator(QWidget *parent)
    : QWidget(parent)
    , color(Blue)
    , length(0)
    , speed(1.0)
    , direction(1)
{
    setMaximumHeight(sliderHeight);
    setMinimumHeight(sliderHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SliderIndicator::updateLength);
    start();
}

void SliderIndicator::start()
{
    //timer->start(10);
}

void SliderIndicator::stop()
{
    timer->stop();
    length = 0;
    update();
}

void SliderIndicator::setSpeed(double newSpeed) {
    speed = newSpeed;
}

void SliderIndicator::setColor(Color c)
{
    color = c;
}

void SliderIndicator::updateLength()
{
    length += speed * direction;

    if (length > width() || length < 0) {
        direction *= -1;
    }
    update();
}

void SliderIndicator::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw the base line
    painter.setPen(QPen(Qt::lightGray, 1));
    painter.drawLine(0, height() / 2, width(), height() / 2);

    if (timer->isActive())
    {
        // Draw the dynamic length line
        QColor lineColor;
        if (color == Green) {
            lineColor = QColor(0, 255, 0);
        } else {
            lineColor = QColor(34, 158, 220);
        }

        painter.setPen(QPen(lineColor, sliderHeight));
        painter.drawLine(0, height() / 2, length, height() / 2);
        painter.drawLine(width() - length, height() / 2, width(), height() / 2);
    }
}

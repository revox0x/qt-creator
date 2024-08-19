#ifndef SLIDERINDICATOR_H
#define SLIDERINDICATOR_H

#include <QWidget>
#include <QTimer>

class SliderIndicator : public QWidget {
    Q_OBJECT

public:
    enum Color { Blue, Green };
    explicit SliderIndicator(QWidget *parent = nullptr);

    void setSpeed(double newSpeed);
    void setColor(Color c);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void updateLength();

private:


    void start();
    void stop();

    // 成员变量
    Color color; // 当前颜色
    double length; // 横线长度
    double speed; // 横线长度变化速度
    int direction; // 横线长度变化方向

    QTimer *timer; // 定时器

    static const int sliderHeight = 16; // 横线高度
};

#endif // SLIDERINDICATOR_H

#ifndef CUSTOMLINEWIDGET_H
#define CUSTOMLINEWIDGET_H

#include "widgettheme.h"

namespace CodeBooster::Internal{
/********************************************************************
 CustomLineWidget
 两端透明的横线控件
*********************************************************************/
class CustomLineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CustomLineWidget(QWidget *parent = nullptr, int transEndLength = 10, QColor c = Qt::white)
        : QWidget(parent)
        , transparentLength(transEndLength)
        , color(c)
    {
        color = QColor(CB_THEME.Color_MarkdownBlockWidget_CodeLine);

        setMinimumHeight(2); // 设置最小高度
        setMaximumHeight(2);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        int width = this->width();
        int height = this->height();

        // 绘制中间的灰色部分
        painter.setPen(QPen(color, height));
        painter.drawLine(transparentLength, height , width - transparentLength, height );

        // 设置透明部分
        QColor transparentColor(0, 0, 0, 0);

        // 绘制左边的透明部分
        painter.setPen(QPen(transparentColor, height));
        painter.drawLine(0, height / 2, transparentLength, height / 2);

        // 绘制右边的透明部分
        painter.setPen(QPen(transparentColor, height));
        painter.drawLine(width - transparentLength, height / 2, width, height / 2);
    }

private:
    int transparentLength; // 透明部分的长度
    QColor color;
};

}

#endif // CUSTOMLINEWIDGET_H

#ifndef CHATSESSIONWGT_H
#define CHATSESSIONWGT_H

#include <QWidget>
#include <QFrame>
#include <QSpacerItem>
#include <QPropertyAnimation>

#include "chatdatabase.h"

namespace Ui {
class ChatSessionWgt;
}

namespace CodeBooster::Internal{

class ChatSessionWgt : public QFrame
{
    Q_OBJECT

public:
    explicit ChatSessionWgt(const ChatSessionBrief &brief, QWidget *parent = nullptr);
    ~ChatSessionWgt();

    QString uuid() const;
    int modifiedTime() const;

    void setHighlight(bool hl);
    void updateInfo(const ChatSessionBrief &brief);

    void setCheckMode(bool canCheck);
    bool checkMode() const;

    void setChecked(bool check);
    bool isChecked() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void startDeleteAnimation();

signals:
    void sessionClicked(const QString &uuid);
    void deleteBtnClicked(const QString &uuid);

private:
    Ui::ChatSessionWgt *ui;
    ChatSessionBrief mBrief;


    QMap<QString, QString> mColorCodes; ///< QMap<objectName, color>
    bool mHighlight;
};

}
#endif // CHATSESSIONWGT_H

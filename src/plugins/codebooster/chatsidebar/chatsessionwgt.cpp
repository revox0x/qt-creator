#include "chatsessionwgt.h"
#include "ui_chatsessionwgt.h"

#include <QMouseEvent>

#include "codeboostericons.h"
#include "codeboosterutils.h"

namespace CodeBooster::Internal{

ChatSessionWgt::ChatSessionWgt(const ChatSessionBrief &brief, QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::ChatSessionWgt)
    , mBrief(brief)
    , mHighlight(false)
{
    ui->setupUi(this);

    // 根据主题设置颜色
    if (isDarkTheme())
    {
        mColorCodes.insert(this->objectName(), "#37373C");
        mColorCodes.insert(ui->label_title->objectName(), "#CCCCCC" );
        mColorCodes.insert(ui->label_time->objectName(), "#7F7F82" );
        mColorCodes.insert(ui->label_count->objectName(), "#7F7F82" );
    }
    else
    {
        mColorCodes.insert(this->objectName(), "#E3E6F1");
        mColorCodes.insert(ui->label_title->objectName(), "#616161" );
        mColorCodes.insert(ui->label_time->objectName(), "#7C7C7C" );
        mColorCodes.insert(ui->label_count->objectName(), "#7C7C7C" );
    }

    // 设置控件样式
    this->setStyleSheet(QString( "QFrame#ChatSessionWgt { background-color: %1; border-radius: 2px;  }").arg(mColorCodes.value(this->objectName())));
    this->setMaximumHeight(48);

    // 设置文本标签样式
    ui->label_title->setStyleSheet(QString("QLabel{font-weight: bold; color: %1;}").arg(mColorCodes.value(ui->label_title->objectName())));
    ui->label_time->setStyleSheet(QString("color: %1").arg(mColorCodes.value(ui->label_time->objectName())));
    ui->label_count->setStyleSheet(QString("color: %1").arg(mColorCodes.value(ui->label_count->objectName())));

    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(mBrief.title);


    // 设置按钮样式
    QString btnStyle = "QPushButton {"
                       "    border: none;"  // 去掉边框
                       "    background: transparent;"  // 背景透明
                       "}";
    ui->pushButton_delete->setStyleSheet(btnStyle);
    ui->pushButton_delete->setIcon(DELETE_ICON.icon());
    ui->pushButton_delete->installEventFilter(this);
    ui->pushButton_delete->setVisible(false);
    connect(ui->pushButton_delete, &QPushButton::clicked, this, [this](){startDeleteAnimation();});

    // 隐藏时布局尺寸不变
    QSizePolicy sp_retain = ui->pushButton_delete->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    ui->pushButton_delete->setSizePolicy(sp_retain);

    ui->pushButton_edit->setStyleSheet(btnStyle);
    ui->pushButton_edit->setIcon(EDIT_ICON.icon());
    ui->pushButton_edit->installEventFilter(this);

    // NOTE:需要编辑名称功能吗？
    ui->pushButton_edit->setVisible(false);
    // END

    // 显示属性
    {
        ui->label_title->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        ui->label_title->setWordWrap(true);
        updateInfo(brief);
    }

    ui->checkBox->setVisible(false);
}

ChatSessionWgt::~ChatSessionWgt()
{
    delete ui;
}

QString ChatSessionWgt::uuid() const
{
    return mBrief.uuid;
}

int ChatSessionWgt::modifiedTime() const
{
    return mBrief.modifiedTime;
}

void ChatSessionWgt::setHighlight(bool hl)
{
    mHighlight = hl;
    if (hl)
    {
        QString color = isDarkTheme() ? "#03395E" : "#96C9F4";
        this->setStyleSheet(QString( "QFrame#ChatSessionWgt { background-color: %1; border-radius: 2px;  }").arg(color));
    }
    else
    {
        this->setStyleSheet(QString( "QFrame#ChatSessionWgt { background-color: %1; border-radius: 2px;  }").arg(mColorCodes.value(this->objectName())));
    }
}

void ChatSessionWgt::updateInfo(const ChatSessionBrief &brief)
{
    ui->label_time->setText(ChatSession::readableTime(brief.modifiedTime));
    ui->label_count->setText(QString("%1条对话").arg(brief.messageCount));
}

void ChatSessionWgt::setCheckMode(bool canCheck)
{
    if (canCheck == checkMode())
        return;

    ui->checkBox->setVisible(canCheck);
    ui->pushButton_delete->setVisible(!canCheck);
}

bool ChatSessionWgt::checkMode() const
{
    return ui->checkBox->isVisible();
}

void ChatSessionWgt::setChecked(bool check)
{
    ui->checkBox->setChecked(check);
}

bool ChatSessionWgt::isChecked() const
{
    return ui->checkBox->isChecked();
}

bool ChatSessionWgt::eventFilter(QObject *obj, QEvent *event)
{
    // 根据鼠标悬浮的状态切换按钮的图标
    if (QPushButton *btn = qobject_cast<QPushButton *>(obj))
    {
        if (event->type() == QEvent::Enter)
        {
            if (btn == ui->pushButton_delete)
            {
                btn->setIcon(DELETE_ICON_INFO.icon());
            }
            else if (btn == ui->pushButton_edit)
            {
                btn->setIcon(EDIT_ICON_INFO.icon());
            }
            return true;
        }
        else if (event->type() == QEvent::Leave)
        {
            if (btn == ui->pushButton_delete)
            {
                btn->setIcon(DELETE_ICON.icon());
            }
            else if (btn == ui->pushButton_edit)
            {
                btn->setIcon(EDIT_ICON.icon());
            }
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}

void ChatSessionWgt::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        event->accept();

        if (checkMode())
        {
            ui->checkBox->setChecked(!ui->checkBox->isChecked());
        }
        else
        {
            emit sessionClicked(uuid());
        }

        return;
    }
    QFrame::mousePressEvent(event);
}

void ChatSessionWgt::enterEvent(QEnterEvent *event)
{
    if (!mHighlight)
    {
        if (isDarkTheme())
            ui->label_title->setStyleSheet("QLabel{font-weight: bold; color: #3794FF;}");
        else
            ui->label_title->setStyleSheet("QLabel{font-weight: bold; color: #006AB1;}");
    }

    if (!checkMode())
        ui->pushButton_delete->setVisible(true);
}

void ChatSessionWgt::leaveEvent(QEvent *event)
{
    if (!mHighlight)
    {
        ui->label_title->setStyleSheet(QString("QLabel{font-weight: bold; color: %1;}").arg(mColorCodes.value(ui->label_title->objectName())));
    }

    ui->pushButton_delete->setVisible(false);
}

void ChatSessionWgt::resizeEvent(QResizeEvent *event)
{
    // 根据Label长度调整显示的文本
    QFontMetrics metrics(ui->label_title->font());
    int width = ui->label_title->width();
    QString elidedText = metrics.elidedText(mBrief.title, Qt::ElideRight, width);
    ui->label_title->setText(elidedText.simplified());

    QFrame::resizeEvent(event);
}

void ChatSessionWgt::startDeleteAnimation()
{
    auto *animation = new QPropertyAnimation(this, "pos", this);
    animation->setDuration(300);
    animation->setStartValue(pos());
    animation->setEndValue(QPoint(pos().x() + width() + 20, pos().y()));
    animation->setEasingCurve(QEasingCurve::OutQuint);
    connect(animation, &QPropertyAnimation::finished, this, [this](){emit deleteBtnClicked(uuid());});
    animation->start(QPropertyAnimation::DeleteWhenStopped);
}



}

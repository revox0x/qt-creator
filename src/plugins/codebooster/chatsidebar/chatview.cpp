#include "chatview.h"
#include "ui_chatview.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QSpacerItem>
#include <QScrollBar>
#include <QToolButton>
#include <utils/stylehelper.h>
#include <QDebug>
#include <utils/utilsicons.h>

#include "chatexportdialog.h"
#include "codeboostericons.h"
#include "chathistorypage.h"
#include "instrumentor.h"
#include "widgettheme.h"
#include "codeboosterutils.h"

namespace CodeBooster::Internal{

ChatView::ChatView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatView)
    , mCurAssistantMsgWgt(nullptr)
    , manager(new QNetworkAccessManager(this))
    , mRequestRunning(false)
    , mUserScrolledUpWhileStreaming(false)
    , previousScrollValue(0)
    , mHistoryPage(nullptr)
{
    ui->setupUi(this);

    // 暗色模式下微调横线样式
    if (isDarkTheme())
    {
        ui->line_2->setMaximumHeight(1);
        ui->line_2->setMinimumHeight(1);
        ui->line_2->setStyleSheet("border: 1px solid #d0d0d0");
    }

    ui->scrollArea->setStyleSheet("QScrollArea{border: none;}");
    connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value){
        if ((previousScrollValue != 0) && mRequestRunning)
        {
            int maxValue = ui->scrollArea->verticalScrollBar()->maximum();
            if (value < previousScrollValue)
            {
                if (previousScrollValue < maxValue)
                    mUserScrolledUpWhileStreaming = true;
            }
            else if (value == maxValue)
            {
                mUserScrolledUpWhileStreaming = false;
            }
        }
        previousScrollValue = value;
    });

    mMsgLayout = new QVBoxLayout(ui->scrollAreaWidgetContents);
    mMsgLayout->setContentsMargins(6, 6, 6, 10);
    mMsgLayout->addStretch(1);

    // 初始化错误信息标签
    ui->label_errInfo->setVisible(false);
    ui->label_errInfo->setWordWrap(true);
    QString labelStyle(R"(
    QLabel {
        background-color: #FFCCCC;
    border: 1px solid red;
    padding: 5px;
        border-radius: 6px;
    }
)");
    ui->label_errInfo->setStyleSheet(labelStyle);

    // 初始化输入控件
    mInputWidget = new InputWidget(this);
    connect(mInputWidget, &InputWidget::sendUserMessage, this, &ChatView::onSendUserMessage);
    connect(mInputWidget, &InputWidget::stopReceivingMessage, this, &ChatView::getFinish);
    connect(mInputWidget, &InputWidget::createNewChat, this, [this](){
        newChat();
    });

    {
        QVBoxLayout *inputLayout = new QVBoxLayout();
        inputLayout->addWidget(mInputWidget);
        inputLayout->setContentsMargins(4, 0, 4, 0);
        ui->verticalLayout->insertLayout(2, inputLayout);
        ui->verticalLayout->setStretch(0, 1);
    }

    connect(ui->pushButton_newSession, &QPushButton::clicked, this, &ChatView::newChat);
    connect(ui->pushButton_export, &QPushButton::clicked, this, &ChatView::onExportBtnClicked);
    // 默认不显示输入控件下方导出按钮，导出功能通过工具栏触发
    ui->pushButton_export->setVisible(false);

    newChat();

    // 加载模型配置信息
    loadModelSettings();
    connect(&CodeBoosterSettings::instance(), &CodeBoosterSettings::modelConfigUpdated, this, &ChatView::loadModelSettings);

    ui->pushButton_backToChat->setIcon(BACK_ICON.icon());
    connect(ui->pushButton_backToChat, &QPushButton::clicked, this, &ChatView::onBtnBackToChatClicked);

    // 默认显示对话页
    ui->stackedWidget->setCurrentWidget(ui->page_chat);


    // 初始化动作
    {
        mActionHistory = new QAction(HISTORY_ICON.icon(), "对话历史", this);
        mActionHistory->setCheckable(false);
        connect(mActionHistory, &QAction::triggered, this, &ChatView::onActionHistoryTriggered);

        mActionShowEditorSelection = new QAction(Utils::Icons::LINK_TOOLBAR.icon(), "显示编辑器选中文本", this);
        mActionShowEditorSelection->setCheckable(true);
        mActionShowEditorSelection->setChecked(CodeBoosterSettings::instance().showEditorSelection);
        connect(mActionShowEditorSelection, &QAction::triggered, this, [this](){
            bool show = mActionShowEditorSelection->isChecked();
            CodeBoosterSettings::instance().showEditorSelection = show;
            mInputWidget->setShowEditorSelection(show);
        });

        mActionExport = new QAction(Utils::Icons::EXPORTFILE_TOOLBAR.icon(), "导出当前对话", this);
        mActionExport->setCheckable(false);
        connect(mActionExport, &QAction::triggered, this, &ChatView::onExportBtnClicked);
    }

    // TODO: 在配置文件中添加开发者选项

    // 设置外观样式
    setupTheme();

    qDebug() << Q_FUNC_INFO << this;
}

ChatView::~ChatView()
{
    delete ui;
}

void ChatView::newChat()
{
    if (mRequestRunning)
    {
        return;
    }

    // 创建对话
    mCurSession = ChatSession();

    // 移除当前的消息控件
    mMessageWgts.clear();
    clearLayout(mMsgLayout);
}

void ChatView::loadChat(const QString &uuid)
{
    showChatPage();

    if (uuid == mCurSession.uuid())
        return;

    ChatSession session;
    if (!ChatDatabase::instance().loadSessionByUuid(uuid, session))
    {
        showErrInfo({"加载对话失败：", QString("uuid: %1").arg(uuid), ChatDatabase::instance().lastError()});
        return;
    }

    newChat();

    mCurSession = session;

    // 加载并显示历史消息
    for (int index = 0; index < session.chatStorage().size(); index++)
    {
        QJsonObject obj = session.chatStorage().at(index).toObject();
        QString role = obj.value("role").toString();
        QString content = obj.value("content").toString();

        if (role == "user")
        {
            MessagePreviewWidget *wgt = newMessageWidget(MessagePreviewWidget::User);
            wgt->setUserMessage(content);
        }
        else if (role == "assistant")
        {
            QString modelName = session.messageSource(index);
            MessagePreviewWidget *wgt = newMessageWidget(MessagePreviewWidget::Assistant, modelName);
            wgt->updatePreview(content);
        }
    }
}

QList<QToolButton *> ChatView::createToolButtons()
{
    auto historyButton = new QToolButton;
    historyButton->setDefaultAction(mActionHistory);
    historyButton->setProperty(Utils::StyleHelper::C_NO_ARROW, true);

    auto showEditorSelectionButton =  new QToolButton;
    showEditorSelectionButton->setDefaultAction(mActionShowEditorSelection);
    showEditorSelectionButton->setProperty(Utils::StyleHelper::C_NO_ARROW, true);

    auto exportChat =  new QToolButton;
    exportChat->setDefaultAction(mActionExport);
    exportChat->setProperty(Utils::StyleHelper::C_NO_ARROW, true);

    return {historyButton, showEditorSelectionButton, exportChat};
}

bool ChatView::event(QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        mInputWidget->activateInput();
    }
    return QWidget::event(event);
}

void ChatView::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_N)
        {
            newChat();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void ChatView::onSendUserMessage(const QString &message)
{
    if (!canCreateNewRequest())
        return;

    // 显示用户消息
    auto userMsgWgt = newMessageWidget(MessagePreviewWidget::User);
    userMsgWgt->setUserMessage(message);

    // 读取设置构建请求参数
    ModelParam param = currentModelParam();
    QJsonObject responseJson = CodeBoosterSettings::buildRequestParamJson(param, true);;
    QJsonArray messages = mCurSession.getChatMessage(message, CodeBoosterSettings::instance().chatAttachedMsgCount());
    responseJson.insert("messages", messages);

    // 发送请求
    request(responseJson, param);
}

void ChatView::sendUserMessageNoContext(const QString &sysMsg, const QString &userMsg, const QMap<QString, QVariant> &overrideParams)
{
    if (!canCreateNewRequest())
        return;

    // 覆写模型参数
    ModelParam param = currentModelParam();
    param.overrideParams(overrideParams);

    // 设置对话控件状态
    mInputWidget->waitingForReceiveMsg();

    // 显示用户消息
    auto userMsgWgt = newMessageWidget(MessagePreviewWidget::User);
    userMsgWgt->setUserMessage(userMsg);

    // 读取设置构建请求参数
    QJsonObject responseJson = CodeBoosterSettings::buildRequestParamJson(param, true);;
    QJsonArray messages = mCurSession.getChatMessage(sysMsg, userMsg);
    responseJson.insert("messages", messages);

    // 发送请求
    request(responseJson, param);
}

void ChatView::request(const QJsonObject &responseJson, const ModelParam &param)
{
    QJsonDocument doc(responseJson);
    QByteArray data_s = doc.toJson(QJsonDocument::JsonFormat::Compact);

    // 创建请求
    QString url = param.apiUrl ;
    QNetworkRequest request = QNetworkRequest(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(param.apiKey).toUtf8());

    repl = manager->post(request, data_s);
    connect(repl, &QNetworkReply::readyRead, this, &ChatView::streamReceived);
    connect(repl, &QNetworkReply::finished, this, &ChatView::handleReplyError);

    // 设置超时定时器
    mTimeoutTimer.setSingleShot(true);
    connect(&mTimeoutTimer, &QTimer::timeout, this, &ChatView::requestTimeout);
    mTimeoutTimer.start(5000); // 5秒超时

    // 设置请求状态
    requestBegin();

    // 保存
    saveChatSession();
}


void ChatView::onExportBtnClicked()
{
    ChatExportDialog dlg(mCurSession, this);
    dlg.exec();
}

void ChatView::onActionHistoryTriggered()
{
    PROFILE_FUNCTION();

    if (ui->stackedWidget->currentWidget() == ui->page_history)
        return;

    // 初始化对话历史页面
    if (!mHistoryPage)
    {
        mHistoryPage = new ChatHistoryPage(mCurSession.uuid(), this);
        connect(mHistoryPage, &ChatHistoryPage::loadSessionHistory, this, &ChatView::loadChat);
        connect(mHistoryPage, &ChatHistoryPage::chatDeleted, this, [=](const QString &uuid){
            if (uuid == mCurSession.uuid())
            {
                newChat();
            }
        });
        ui->verticalLayout_2->addWidget(mHistoryPage);
    }
    else
    {
        mHistoryPage->highlightSession(mCurSession.uuid());
    }

    ui->stackedWidget->setCurrentWidget(ui->page_history);
}

void ChatView::onBtnBackToChatClicked()
{
    showChatPage();
}

bool ChatView::canCreateNewRequest(bool showInfo)
{
    // 是否有模型参数
    ModelParam param = currentModelParam();
    if (param.title.isEmpty())
    {
        if (showInfo)
        {
            outputMessage("无法发送对话请求：请配置模型参数", Error);
        }
        return false;
    }

    if (mRequestRunning)
    {
        outputMessage("无法发送对话请求：当前对话请求进行中，请结束后再试", Normal);
        return false;
    }

    return true;
}

void ChatView::setupTheme()
{
    ui->scrollAreaWidgetContents->setStyleSheet(CB_THEME.SS_ChatBackground);
}

MessagePreviewWidget *ChatView::newMessageWidget(MessagePreviewWidget::MessageMode mode, QString modelName)
{
    if (modelName.isEmpty())
    {
        modelName = (mode == MessagePreviewWidget::Assistant) ? currentModelParam().modelName : QString();
    }

    MessagePreviewWidget *wgt = new MessagePreviewWidget(mode, modelName, this);
    mMessageWgts << wgt;
    mMsgLayout->insertWidget(mMsgLayout->count() - 1, wgt);
    return wgt;
}

void ChatView::updateAssistantMessage(const QString &content)
{
    if (!mCurAssistantMsgWgt)
    {
        mCurAssistantMsgWgt = newMessageWidget(MessagePreviewWidget::Assistant);
    }

    mCurAssistantMsgWgt->updatePreview(content);
    resp += content;

    // 接受消息时保持滚动条在最下方显示最新消息
    QScrollBar *scrollBar = ui->scrollArea->verticalScrollBar();
    //qDebug() << "scrollBar->isVisible()" << scrollBar->isVisible() << "mUserScrolledWhileStreaming" << mUserScrolledUpWhileStreaming;
    if (scrollBar->isVisible() && !mUserScrolledUpWhileStreaming)
    {
        scrollBar->setValue(scrollBar->maximum());
    }

    // // 将流式传输的结果写入文件
    // if (mLogStream)
    // {
    //     *mLogStream << content << "\n";
    //     mLogStream->flush();
    // }
}

void ChatView::handleReplyError()
{
    mTimeoutTimer.stop();

    // 对话采用流式传输，因此只需要进行错误处理
    if (repl->error() != QNetworkReply::NoError)
    {
        QStringList errInfos;
        errInfos << "请求错误：";

        // 获取HTTP状态码
        QVariant statusCode = repl->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (statusCode.isValid()) {
            errInfos<< "HTTP status code：" + QString::number(statusCode.toInt());
        }

        errInfos << QString("Network error code: %1").arg(repl->error());
        errInfos << QString("Network error string: %1").arg(repl->errorString());

        showErrInfo(errInfos);
        getFinish();
    }
}

void ChatView::streamReceived()
{
    // 终止超时定时器
    mTimeoutTimer.stop();

    // 隐藏错误提示
    ui->label_errInfo->setVisible(false);

    QByteArray get;
    get = repl->readLine();
    while (get.length() != 0)
    {
        if (get == "\n")
        {
            get = repl->readLine();
            continue;
        }

        int index = get.indexOf('{');
        QByteArray j_data = get.mid(index);
        if (get == "data: [DONE]\n")
        {
            getFinish();
            break;
        }

        QJsonDocument doc = QJsonDocument::fromJson(j_data);
        if (doc.isEmpty())
        {
            get = repl->readLine();
            continue;
        }

        QJsonObject jo = doc.object();
        QJsonArray a = jo["choices"].toArray();
        QJsonObject jjo = a[0].toObject()["delta"].toObject();
        if (jjo.contains("content"))
        {
            QString s = jjo["content"].toString();
            updateAssistantMessage(s);
        }
        get = repl->readLine();
    }
}

void ChatView::getFinish()
{
    repl->disconnect();
    repl->deleteLater();

    if (resp.length() > 0)
    {
        mCurSession.appendAssistantMessage(resp, currentModelParam().title);
        // 保存
        saveChatSession();
        resp.clear();
    }

    requestFinished();
}

void ChatView::requestTimeout()
{
    // 停止
    repl->abort();

    // 显示超时提示
    ModelParam param = currentModelParam();

    QStringList msgs;
    msgs << "请求超时，请检查网络参数：";
    msgs << "Title: " + param.title;
    msgs << "Model: " + param.modelName;
    msgs << "apiUrl: " + param.apiUrl;
    msgs << "apiKey: " + param.apiKey;

    showErrInfo(msgs);

    // 重置请求状态
    getFinish();
}

/**
 * @brief ChatView::requestBegin 网络请求开始
 */
void ChatView::requestBegin()
{
    mRequestRunning = true;

    ui->pushButton_newSession->setEnabled(false);
    mActionHistory->setEnabled(false);
}

/**
 * @brief ChatView::requestEnd 网络请求结束
 */
void ChatView::requestFinished()
{
    mRequestRunning = false;

    mInputWidget->messageReceiveFinished();
    mCurAssistantMsgWgt = nullptr;
    mUserScrolledUpWhileStreaming = false;

    ui->pushButton_newSession->setEnabled(true);
    mActionHistory->setEnabled(true);
}

void ChatView::loadModelSettings()
{
    // 记录当前选中模型的信息
    QString oldSelectedModelTitle = currentModelParam().title;

    // 清空选线
    ui->comboBox_model->clear();

    // 加载模型信息
    for (const auto &param : CodeBoosterSettings::instance().chatParams())
    {
        ui->comboBox_model->addItem(param.title, QVariant::fromValue(param));
    }

    // 还原之前选择的模型
    if (!oldSelectedModelTitle.isEmpty())
    {
        ui->comboBox_model->setCurrentText(oldSelectedModelTitle);
    }

    // 更新发送消息按钮状态
    if (currentModelParam().title.isEmpty())
    {
        mInputWidget->setSendButtonEnabled(false, "请配置模型参数");
    }
    else
    {
        mInputWidget->setSendButtonEnabled(true);
    }
}

void ChatView::showErrInfo(QStringList errInfos) const
{
    if (errInfos.isEmpty()) return;
    qDebug() << Q_FUNC_INFO;
    qDebug() << errInfos;
    ui->label_errInfo->setVisible(true);
    QString htmlStr = QString("<b><font color='black'>%1</font></b><br>").arg(errInfos.takeFirst());
    for (const QString &err : errInfos)
    {
        htmlStr += QString("<font color='black'>%1</font><br>").arg(err);
    }
    // 去掉末尾换行
    htmlStr.chop(4);

    ui->label_errInfo->setText(htmlStr);
}

void ChatView::clearLayout(QLayout *layout)
{
    if (!layout) return;

    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            QWidget* widget = item->widget();
            widget->setParent(nullptr);
            delete widget;
        } else if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        } else {
            delete item;
        }
    }

    // 在布局末尾加上弹簧
    mMsgLayout->addStretch(1);
}

void ChatView::saveChatSession()
{
    QString err;
    if (!ChatDatabase::instance().saveChatSession(mCurSession, err))
    {
        showErrInfo({"保存对话失败：", err});
    }

    // 保存对话后清理历史页面
    clearHistoryPage();
}

void ChatView::showChatPage()
{
    ui->stackedWidget->setCurrentWidget(ui->page_chat);
}

void ChatView::clearHistoryPage()
{
    if (mHistoryPage)
    {
        mHistoryPage->deleteLater();
        mHistoryPage = nullptr;
    }
}

ModelParam ChatView::currentModelParam() const
{
    if (ui->comboBox_model->count() == 0)
        return ModelParam();

    ModelParam param = ui->comboBox_model->currentData().value<ModelParam>();
    return param;
}

} // namespace

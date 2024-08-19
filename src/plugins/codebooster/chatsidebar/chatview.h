#ifndef CHATVIEW_H
#define CHATVIEW_H

#include <QWidget>
#include <QNetworkReply>
#include <QVBoxLayout>

#include "inputwidget.h"
#include "codeboostersettings.h"
#include "chatdatabase.h"
#include "markdownpreview/messagepreviewwidget.h"

#include <solutions/spinner/spinner.h>

namespace Ui {
class ChatView;
}

class SliderIndicator;

namespace CodeBooster::Internal{

class ChatSession;
class InputWidget;
class ChatHistoryPage;

class ChatView : public QWidget
{
    Q_OBJECT

public:
    explicit ChatView(QWidget *parent = nullptr);
    ~ChatView();

    void newChat();
    void loadChat(const QString &uuid);

    QList<QToolButton *> createToolButtons();

    void sendUserMessageNoContext(const QString &sysMsg, const QString &userMsg,
                                  const QMap<QString, QVariant> &overrideParams = QMap<QString, QVariant>());

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSendUserMessage(const QString &message);
    void updateAssistantMessage(const QString &content);

    void handleReplyError();
    void streamReceived();
    void getFinish();
    void requestTimeout();

    void onExportBtnClicked();

    void onActionHistoryTriggered();
    void onBtnBackToChatClicked();

private:
    bool canCreateNewRequest(bool showInfo = true);
    void request(const QJsonObject &responseJson, const ModelParam &param);
    void setupTheme();
    MessagePreviewWidget *newMessageWidget(MessagePreviewWidget::MessageMode mode, QString modelName = QString());
    void requestBegin();
    void requestFinished();
    void loadModelSettings();
    void showErrInfo(QStringList errInfos) const;
    void clearLayout(QLayout* layout);
    void saveChatSession();
    void showChatPage();
    void clearHistoryPage();

    ModelParam currentModelParam() const;

private:
    Ui::ChatView *ui;

    QAction *mActionHistory; ///< 打开对话历史动作
    QAction *mActionShowEditorSelection; ///< 输入控件是否显示编辑器选中文本
    QAction *mActionExport;  ///< 导出对话

    InputWidget *mInputWidget;  ///< 消息输入控件
    ChatSession mCurSession; ///< 当前的对话
    QVBoxLayout *mMsgLayout;
    QSpacerItem *mMsgSpacer;

    ChatHistoryPage *mHistoryPage; ///< 对话历史页

    QList<MessagePreviewWidget *> mMessageWgts; ///< 显示消息的控件
    MessagePreviewWidget *mCurAssistantMsgWgt;

    // 网络请求相关
    QNetworkReply* repl = nullptr;
    QNetworkAccessManager* manager;
    QString resp; ///< 当前请求的回复
    bool mRequestRunning;
    QTimer mTimeoutTimer;
    // end

    bool mUserScrolledUpWhileStreaming; ///< 在接受流式传输数据时用户滚动了滚动条
    int previousScrollValue;

    QFile *mLogFile;
    QTextStream *mLogStream;
};

}// namespace CodeBooster::Internal



#endif // CHATVIEW_H

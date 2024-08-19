#ifndef MESSAGEPREVIEWWIDGET_H
#define MESSAGEPREVIEWWIDGET_H

#include "qframe.h"
#include <QWidget>
#include <QRegularExpression>

class QLabel;
class QToolBar;
class QTextBrowser;
class QVBoxLayout;
class NotePreviewWidget;

namespace CodeBooster::Internal{

class MarkdownBlockWidget : public QFrame
{
    Q_OBJECT
public:
    explicit MarkdownBlockWidget(bool codeMode = false, const QString &language = "PlainText", QWidget *parent = nullptr);
    ~MarkdownBlockWidget();

public:
    void update(const QString &newStr);
    QString toPlainText() const;
    void setPlainText(const QString &text);

signals:

private slots:
    void onActionCopyTriggered();
    void onActionInsertTriggered();

private:
    void setupCodeToolBar(const QString &language);
    bool codeMode() const;

    QString extractLanguage(const QString &markdownCode);

private:
    bool mCodeMode;

    QVBoxLayout *mLayout;

    QToolBar *mToolBar;

    QAction *mActionCopy;
    QAction *mActionInsert;

    NotePreviewWidget *mPreviewWgt;
    QLabel *mTitle;
    QString mMarkdownText;
};

/********************************************************************
 MessagePreviewWidget
 单条消息的预览控件
*********************************************************************/
class MessagePreviewWidget : public QFrame
{
    Q_OBJECT
public:
    /**
     * @brief The MessageMode enum 消息模式
     */
    enum MessageMode
    {
        User = 0,       ///< 用户消息
        Assistant = 1   ///< 助手消息
    };

    explicit MessagePreviewWidget(MessageMode mode, const QString &modelName = QString(), QWidget *parent = nullptr);
    ~MessagePreviewWidget();

public:
    void updatePreview(const QString &text);
    void setUserMessage(const QString &message);
    MessageMode mode() const;

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

signals:

private slots:
    void onActionCopyTriggered();
    void onActionExportPngTriggered();

private:
    void setupToolBar();
    void buildAssistantMainTextBrowser();
    void buildAssistantCodeTextBrowser(const QString &language);
    MarkdownBlockWidget* currentMDBlock();
    void showActions(bool show);

    void removeLastCharacters(MarkdownBlockWidget *mdBlock, int numChars);
    void simplifyCharacters(MarkdownBlockWidget *mdBlock);
    void cleanCodeStartMainBlock();
    void connectActionTextChangeSlot(QAction *action);

private:
    static const QRegularExpression codeBlockStartEmpty;
    static const QRegularExpression codeBlockStart;
    static const QRegularExpression codeBlockEnd;

private:
    MessageMode mMode;

    QVBoxLayout *mLayout;

    // 标题工具栏
    QToolBar *mToolBar;
    QLabel *mIcon;
    QLabel *mTitle;
    QAction *mActionCopy;
    QAction *mActionExportToPng;


    // 助手消息预览相关
    QList<MarkdownBlockWidget *> mBlocks;

    MarkdownBlockWidget *mainTextBrowser = nullptr;
    MarkdownBlockWidget *codeTextBrowser = nullptr;
    bool inCodeBlock = false;

    QString buffer;
    int pos = 0;
    int mLastCodeStartPos = 0;
    int mLastCodeEndPos = 0;
    // end

    // 用户消息相关
    QString mModelName;
    MarkdownBlockWidget *mUserInput;
    QString mUserMessage;

    // 控件参数
    QMargins mContentWidgetMargin;
    // end
};


} // namespace

#endif // MESSAGEPREVIEWWIDGET_H

#include "messagepreviewwidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QToolBar>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QWidgetAction>
#include <QPainter>
#include <QFileDialog>
#include <QTimer>
#include <QScrollBar>

#include "notepreviewwidget.h"
#include "markdownhtmlconverter.h"
#include "../chatexportdialog.h"
#include "codeboostericons.h"
#include "instrumentor.h"
#include "widgettheme.h"
#include "codeboosterutils.h"
#include "../customlinewidget.h"

namespace CodeBooster::Internal{

/**
 * @brief MarkdownBlockWidget::MarkdownCodeBlockWidget markdown文本块显示控件
 * @param language
 * @param codeMode
 * @param parent
 */
MarkdownBlockWidget::MarkdownBlockWidget(bool codeMode, const QString &language, QWidget *parent)
    : QFrame{parent}
    , mCodeMode(codeMode)
    , mToolBar(nullptr)
    , mActionCopy(nullptr)
    , mActionInsert(nullptr)
    , mPreviewWgt(nullptr)
    , mTitle(nullptr)
{
    setObjectName("MarkdownBlockWidget");

    mLayout = new QVBoxLayout(this);
    mLayout->setSpacing(0);

    // 初始化预览控件
    mPreviewWgt = new NotePreviewWidget(this);
    mPreviewWgt->setObjectName("mPreviewWgt");
    mLayout->addWidget(mPreviewWgt, 1);

    if (codeMode)
    {
        setupCodeToolBar(language);
        this->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeMode);

        mLayout->setContentsMargins(10, 0, 10, 0);
        mLayout->insertWidget(1, new CustomLineWidget(this));

        mPreviewWgt->disableLineWrap();
        // BUG: 滚动条的样式不生效
        mPreviewWgt->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeMode_PreWgt);
    }
    else
    {
        mPreviewWgt->setStyleSheet(QString("QWidget#mPreviewWgt {border: none; border-radius: 6px; background-color: %1;}").arg(CB_THEME.Color_MarkdownBlockWidget_NomalBackground));
    }
}

MarkdownBlockWidget::~MarkdownBlockWidget()
{

}

void MarkdownBlockWidget::update(const QString &newStr)
{
    // qDebug() << Q_FUNC_INFO << newStr;
    mMarkdownText += newStr;
    // 显示预览
    QString htmlStr = MarkdownHtmlConverter::toMarkdownHtml(mMarkdownText, mCodeMode);
    mPreviewWgt->setHtml(htmlStr);
}

QString MarkdownBlockWidget::toPlainText() const
{
    return mMarkdownText;
}

void MarkdownBlockWidget::setPlainText(const QString &text)
{
    mMarkdownText = text;
    QString htmlStr = MarkdownHtmlConverter::toMarkdownHtml(text);
    mPreviewWgt->setHtml(htmlStr);
}

void MarkdownBlockWidget::onActionCopyTriggered()
{
    // 获取系统剪切板
    QClipboard *clipboard = QApplication::clipboard();
    // 将 QString 复制到系统剪切板
    clipboard->setText(mMarkdownText);
}

void MarkdownBlockWidget::onActionInsertTriggered()
{

}

void MarkdownBlockWidget::setupCodeToolBar(const QString &language)
{
    mToolBar = new QToolBar(this);
    mToolBar->setObjectName("mToolBar");
    mToolBar->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeToolBar);

    // 显示编程语言类型
    mTitle = new QLabel(language, this);
    mTitle->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeToolBar_Label);
    mToolBar->addWidget(mTitle);

    // 添加弹簧
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mToolBar->addWidget(spacer);

    mActionCopy = new QAction(COPY_ICON.icon(), "复制", this);
    connect(mActionCopy, &QAction::triggered, this, &MarkdownBlockWidget::onActionCopyTriggered);

    mActionInsert = new QAction(INSERT_ICON.icon(), "插入", this);
    connect(mActionInsert, &QAction::triggered, this, &MarkdownBlockWidget::onActionInsertTriggered);

    mToolBar->addAction(mActionCopy);

    {
        // 添加间隔
        QWidget *actionSpacer = new QWidget();
        actionSpacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        actionSpacer->setFixedWidth(10); // 设置间隔的宽度
        mToolBar->addWidget(actionSpacer);
    }

    mToolBar->addAction(mActionInsert);

    {
        // 添加间隔
        QWidget *actionSpacer = new QWidget();
        actionSpacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        actionSpacer->setFixedWidth(10); // 设置间隔的宽度
        mToolBar->addWidget(actionSpacer);
    }

    // 将工具栏插入到布局上方
    mLayout->insertWidget(0, mToolBar);
}

bool MarkdownBlockWidget::codeMode() const
{
    return mCodeMode;
}

QString MarkdownBlockWidget::extractLanguage(const QString &markdownCode)
{
    // 使用正则表达式匹配代码块的第一行
    static QRegularExpression re("```(\\w+)");
    QRegularExpressionMatch match = re.match(markdownCode);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString("未知");
}

const QRegularExpression MessagePreviewWidget::codeBlockStartEmpty("\\n```(\\w*)\\n");
const QRegularExpression MessagePreviewWidget::codeBlockStart("```(\\w*)\\n");
const QRegularExpression MessagePreviewWidget::codeBlockEnd("\\n\\s*```\\n");

/**
 * @brief MessagePreviewWidget::MessagePreviewWidget
 * @param parent
 */
MessagePreviewWidget::MessagePreviewWidget(MessageMode mode, const QString &modelName, QWidget *parent): QFrame{parent}
    , mMode(mode)
    , mLayout(nullptr)
    , mToolBar(nullptr)
    , mIcon(nullptr)
    , mTitle(nullptr)
    , mActionCopy(nullptr)
    , mActionExportToPng(nullptr)
    , mModelName(modelName)
    , mUserInput(nullptr)
    , mContentWidgetMargin(6, 0, 6, 0)
{
    mLayout = new QVBoxLayout(this);
    mLayout->setContentsMargins(0, 3, 0, 0);
    mLayout->setSpacing(0);

    setupToolBar();
    setObjectName("MessagePreviewWidget");

    setStyleSheet(CB_THEME.SS_MessagePreview);
}

MessagePreviewWidget::~MessagePreviewWidget()
{

}

void MessagePreviewWidget::updatePreview(const QString &text)
{
    buffer += text;

    //qDebug() << "--------------" << text ;

    while (pos < buffer.length())
    {
        if (inCodeBlock)
        {
            QRegularExpressionMatch match = codeBlockEnd.match(buffer, mLastCodeEndPos);
            if (match.hasMatch()) {
                int endPos = match.capturedStart();
                int endPatternLength = match.capturedLength();
                int posAfterMatch = endPos + endPatternLength;

                QString codeContent = buffer.mid(pos, posAfterMatch - pos);
                codeTextBrowser->update(codeContent);

                simplifyCharacters(codeTextBrowser);

                pos = endPos + endPatternLength; // 更新 pos 到结束标识符之后

                codeTextBrowser = nullptr;
                inCodeBlock = false;
                mLastCodeStartPos = pos;
                mLastCodeEndPos = pos;
            }
            else
            {
                // 将代码块内容进行显示
                codeTextBrowser->update(buffer.mid(pos));
                break;
            }
        }
        else
        { 
            int startPatternLength = (pos == 0) ? 4 : 3;
            QRegularExpression codeStartReg = (pos == 0) ? codeBlockStartEmpty : codeBlockStart;

            QRegularExpressionMatch match = codeStartReg.match(buffer, mLastCodeStartPos);
            if (match.hasMatch())
            {
                int startPos = match.capturedStart();
                int matchLength = match.capturedLength();
                int posAfterMatch = startPos + matchLength;

                if (startPos > pos)
                {
                    QString normalContent = buffer.mid(pos, startPos - pos);
                    if (!mainTextBrowser)
                    {
                        buildAssistantMainTextBrowser();
                        mLayout->addWidget(mainTextBrowser);
                    }

                    mainTextBrowser->update(normalContent);
                    pos = startPos;
                }

                /// \n```cpp\n
                QString language = buffer.mid(startPos + startPatternLength, matchLength - (startPatternLength + 1));
                if (language.isEmpty())
                {
                    language = "PlainText";
                }

                cleanCodeStartMainBlock();

                buildAssistantCodeTextBrowser(language);
                mLayout->addWidget(codeTextBrowser);

                if (pos >= startPos)
                {
                    int codeStartOffset = pos - startPos;
                    if (mainTextBrowser)
                    {
                        removeLastCharacters(mainTextBrowser, codeStartOffset);
                    }

                    QString startPrefix = buffer.mid(startPos, matchLength);
                    if (startPrefix.last(1) != "\n")
                        startPrefix += "\n";
                    codeTextBrowser->update(startPrefix);
                }

                if (mainTextBrowser)
                {
                    simplifyCharacters(mainTextBrowser);
                }


                pos = posAfterMatch; // 更新 pos 到起始标识符之后
                inCodeBlock = true;
                mainTextBrowser = nullptr;
                mLastCodeStartPos = pos;
                mLastCodeEndPos = pos;
            }
            else
            {
                QString mainText = buffer.mid(pos);
                if (!mainTextBrowser)
                {
                    buildAssistantMainTextBrowser();
                    mLayout->addWidget(mainTextBrowser);
                }

                mainTextBrowser->update(mainText);

                break;
            }
        }
    }

    // 将位置偏移到末尾
    pos = buffer.size();
}

void MessagePreviewWidget::setUserMessage(const QString &message)
{
    mUserMessage = message;

    mUserInput = new MarkdownBlockWidget(false, QString(), this);
    mUserInput->setObjectName("MessagePreviewWidget_mUserInput");
    mUserInput->setStyleSheet(CB_THEME.SS_MessagePreview_UserTextBrowser);
    mUserInput->layout()->setContentsMargins(mContentWidgetMargin);
    mLayout->addWidget(mUserInput);
    mUserInput->setPlainText(message);
}

MessagePreviewWidget::MessageMode MessagePreviewWidget::mode() const
{
    return mMode;
}

void MessagePreviewWidget::enterEvent(QEnterEvent *event)
{
    showActions(true);
    QFrame::enterEvent(event);
}

void MessagePreviewWidget::leaveEvent(QEvent *event)
{
    showActions(false);
    QFrame::leaveEvent(event);
}

void MessagePreviewWidget::onActionCopyTriggered()
{
    // 获取系统剪切板
    QClipboard *clipboard = QApplication::clipboard();
    // 将 QString 复制到系统剪切板
    if (User == mode())
    {
        clipboard->setText(mUserMessage);
    }
    else if (Assistant == mode())
    {
        clipboard->setText(buffer);
    }
}

void MessagePreviewWidget::onActionExportPngTriggered()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setImage(ChatExportDialog::generateImageFromMarkdonwText(buffer, width()));
}

void MessagePreviewWidget::setupToolBar()
{
    mToolBar = new QToolBar(this);
    mToolBar->setObjectName("MessagePreviewWidgetToolBar");
    mToolBar->setContentsMargins(0, 0, 8, 0);
    mToolBar->setStyleSheet(CB_THEME.SS_MessagePreview_ToolBar);

    // 初始化对话来源图标
    mIcon = new QLabel(this);
    mIcon->setFixedWidth(24);
    mIcon->setAlignment(Qt::AlignCenter);

    // 设置消息分类名称
    mTitle = new QLabel(this);

    {
        // 使用QHBoxLayout手动布局mIcon和mTitle
        QHBoxLayout *layout = new QHBoxLayout();
        layout->setContentsMargins(4, 0, 0, 0); // 设置mIcon的左边距
        layout->addWidget(mIcon);
        layout->addWidget(mTitle);

        // 创建一个QWidget来容纳布局
        QWidget *layoutWidget = new QWidget();
        layoutWidget->setLayout(layout);

        // 将布局Widget添加到工具栏
        mToolBar->addWidget(layoutWidget);
    }

    // 将标题添加到工具栏
    mToolBar->addWidget(mIcon);
    mToolBar->addWidget(mTitle);

    // 添加弹簧
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mToolBar->addWidget(spacer);

    mActionCopy = new QAction("复制", this);
    connect(mActionCopy, &QAction::triggered, this, &MessagePreviewWidget::onActionCopyTriggered);
    mToolBar->addAction(mActionCopy);

    QString titleStr;
    if (mMode == User)
    {
        titleStr = "你";
        mIcon->setPixmap(USER_ICON_INFO.icon().pixmap(QSize(16, 16)));
    }
    else if (mMode == Assistant)
    {
        titleStr = mModelName;
        mActionExportToPng = new QAction("导出",this);
        mActionExportToPng->setToolTip("导出为图片到剪切板");
        connect(mActionExportToPng, &QAction::triggered, this, &MessagePreviewWidget::onActionExportPngTriggered);
        mToolBar->addAction(mActionExportToPng);

        connectActionTextChangeSlot(mActionExportToPng);

        mIcon->setPixmap(ROBOT_ICON_INFO.icon().pixmap(QSize(16, 16)));
    }
    mTitle->setText(titleStr);

    // 默认隐藏动作
    showActions(false);

    mLayout->addWidget(mToolBar);
}

void MessagePreviewWidget::buildAssistantMainTextBrowser()
{
    // 第二个文本块开始，加上与代码块的顶部距离
    if (!mBlocks.isEmpty())
        mContentWidgetMargin.setTop(6);

    mainTextBrowser = new MarkdownBlockWidget(false, QString(), this);
    mainTextBrowser->layout()->setContentsMargins(mContentWidgetMargin);

    mBlocks << mainTextBrowser;
}

void MessagePreviewWidget::buildAssistantCodeTextBrowser(const QString &language)
{
    codeTextBrowser = new MarkdownBlockWidget(true, language, this);
    mBlocks << codeTextBrowser;
}

MarkdownBlockWidget* MessagePreviewWidget::currentMDBlock()
{
    if (!mBlocks.isEmpty()) mBlocks.last();

    return nullptr;
}

void MessagePreviewWidget::showActions(bool show)
{
    for(const auto &action : mToolBar->actions())
    {
        if (dynamic_cast<QWidgetAction*>(action))
            continue;
        action->setVisible(show);
    }
}

void MessagePreviewWidget::removeLastCharacters(MarkdownBlockWidget *mdBlock, int numChars)
{
    // 获取当前文本
    QString currentText = mdBlock->toPlainText();

    // 检查是否需要移除的字符数大于文本长度
    if (numChars > currentText.length()) {
        numChars = currentText.length();
    }

    // 移除末尾的指定数量的字符
    currentText.chop(numChars);

    // 设置修改后的文本
    mdBlock->setPlainText(currentText);
}

void MessagePreviewWidget::simplifyCharacters(MarkdownBlockWidget *mdBlock)
{
    QString currentText = mdBlock->toPlainText();
    if (currentText.isEmpty())
        return;

    QString simplifiedText = currentText;
    /* 去除代码块末尾的换行符号：
        before:
        ---------------------------------------------
        ```cpp
            std::cout << "Hello, World!" << std::endl;
        ```

        ---------------------------------------------

        after:
        ---------------------------------------------
        ```cpp
            std::cout << "Hello, World!" << std::endl;
        ```
        ---------------------------------------------
    */
    while (simplifiedText.last(1) == "\n")
    {
        simplifiedText.chop(1);
    }

    /* 去除代码块结束符前的空格
        before:
        ---------------------------------------------
        ```cpp
            std::cout << "Hello, World!" << std::endl;
            ```
        ---------------------------------------------

        after:
        ---------------------------------------------
        ```cpp
            std::cout << "Hello, World!" << std::endl;
        ```
        ---------------------------------------------
    */
    if (simplifiedText.right(3) == "```")
    {
        simplifiedText.chop(3);
        while (simplifiedText.last(1) == " ")
        {
            simplifiedText.chop(1);
        }

        simplifiedText += "```";
    }

    if (simplifiedText != currentText)
    {
        mdBlock->setPlainText(simplifiedText);
    }
}

/**
 * @brief MessagePreviewWidget::cleanCodeStartMainBlock 清理创建代码块前显示代码开始标志的文本块
 * 如果不执行此函数，当返回的文本开始就是代码块时代码block上方会创建一个空的文本块
 */
void MessagePreviewWidget::cleanCodeStartMainBlock()
{
    if (mainTextBrowser)
    {
        QString text = mainTextBrowser->toPlainText();
        if (text.contains("```") && !text.contains("\n"))
        {
            qDebug() << Q_FUNC_INFO << text;
            mLayout->removeWidget(mainTextBrowser);
            delete mainTextBrowser;
            mainTextBrowser = nullptr;
        }
    }
}

void MessagePreviewWidget::connectActionTextChangeSlot(QAction *action)
{
    QString text = action->text();
    connect(action, &QAction::triggered, this, [this, action, text]() {
        action->setText("成功");
        // 启动一个定时器，2秒后还原文本
        QTimer::singleShot(2000, this, [action, text]() {
            action->setText(text);
        });
    });
}

} // namespace

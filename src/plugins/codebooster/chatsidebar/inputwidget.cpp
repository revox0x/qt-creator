#include "inputwidget.h"

#include <QTextBlock>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QScrollBar>
#include <utils/utilsicons.h>
#include <texteditor/texteditor.h>

#include "markdownpreview/notepreviewwidget.h"
#include "markdownpreview/markdownhtmlconverter.h"
#include "customlinewidget.h"
#include "codeboostericons.h"
#include "widgettheme.h"
#include "codeboosterplugin.h"
#include "codeboostersettings.h"
#include "codeboosterutils.h"

using namespace TextEditor;


namespace CodeBooster::Internal{

/********************************************************************
 CodeSnippetWidget
 代码段显示控件
*********************************************************************/
CodeSnippetWidget::CodeSnippetWidget(QWidget *parent) : QFrame(parent)
{
    this->setObjectName("CodeSnippetWidget");
    this->setStyleSheet(CB_THEME.SS_InputWidget_CodeSnippet);

    QVBoxLayout *innerLayout = new QVBoxLayout(this);
    innerLayout->setSpacing(0);
    innerLayout->setContentsMargins(0, 0, 0, 0);

    // 初始化工具栏
    {
        mToolBar = new QToolBar(this);
        mToolBar->setObjectName("mToolBar");
        mToolBar->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeToolBar);
        mToolBar->installEventFilter(this);

        mFileIcon = new QLabel(this);
        mFileIcon->setFixedWidth(24);
        mFileIcon->setAlignment(Qt::AlignCenter);
        mFileIcon->setPixmap(CODEFILE_ICON.icon().pixmap(QSize(16, 16)));
        mToolBar->addWidget(mFileIcon);

        // 添加文件名称标签
        mFileNameTitle = new QLabel(this);
        mFileNameTitle->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeToolBar_Label);
        mToolBar->addWidget(mFileNameTitle);

        // 添加弹簧
        QWidget *spacer = new QWidget();
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        mToolBar->addWidget(spacer);

        mActionClose = new QAction(Utils::Icons::CLOSE_TOOLBAR.icon(), "关闭", this);
        connect(mActionClose, &QAction::triggered, this, &CodeSnippetWidget::onActionCloseTriggered);
        mToolBar->addAction(mActionClose);


        mActionExpand = new QAction(EXPAND_ICON.icon(), "折叠", this);
        connect(mActionExpand, &QAction::triggered, this, &CodeSnippetWidget::onActionExpandTriggered);
        mToolBar->addAction(mActionExpand);

        innerLayout->addWidget(mToolBar);
    }

    {
        mHorLine = new CustomLineWidget(this, 12);
        innerLayout->addWidget(mHorLine);
    }

    {
        mPreviewWgt = new NotePreviewWidget(this);
        mPreviewWgt->setObjectName("mPreviewWgt");
        mPreviewWgt->disableLineWrap();
        mPreviewWgt->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeMode_PreWgt);

        mPreviewWgt->setHeightMode(NotePreviewWidget::MaxLimit);
        innerLayout->addWidget(mPreviewWgt, 1);
    }

    setMaximumHeight(330);

    this->setLayout(innerLayout);
}

void CodeSnippetWidget::showCodeSnippet(const QString &fileName, const QString &selectedText)
{
    QString snippet = selectedText;
    snippet.replace(QChar(0x2028), "\n"); // 替换行分隔符 (LS)
    snippet.replace(QChar(0x2029), "\n"); // 替换段落分隔符 (PS)
    snippet = snippet.trimmed();
    if (snippet.isEmpty())
    {
        clear();
        return;
    }

    this->setVisible(true);

    mFileNameTitle->setText(fileName);

    mFileName = fileName;
    mCodeSnippet = snippet;

    QString language = languageFromFileSuffix(QString(fileName.section(".", -1)));
    QString codeSnippet = QString("```%1\n%2\n```").arg(language).arg(mCodeSnippet);
    QString htmlStr = MarkdownHtmlConverter::toMarkdownHtml(codeSnippet);
    mPreviewWgt->setHtml(htmlStr);
    if (mPreviewWgt->verticalScrollBar()->isVisible())
    {
        // TODO: 是否可以判断鼠标指针移动的方向来调整滚动条移动到最上方还是最下方？
        mPreviewWgt->verticalScrollBar()->setValue(mPreviewWgt->verticalScrollBar()->maximum());
    }

    mActionExpand->setText("展开");
    onActionExpandTriggered();
}

QString CodeSnippetWidget::codeSnippet() const
{
    if (mCodeSnippet.isEmpty())
        return QString();

    QString codeText;
    codeText += QString("代码 (%1):\n").arg(mFileName);
    codeText += QString("```%1\n").arg(languageFromFileSuffix(QString(mFileName.section(".", -1))));
    codeText += mCodeSnippet;
    codeText += QString("\n```");

    return codeText;
}

void CodeSnippetWidget::clear()
{
    mFileName.clear();
    mCodeSnippet.clear();
    mPreviewWgt->clear();
    mPreviewWgt->setHtml(QString());
    this->setVisible(false);
}

void CodeSnippetWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    emit heightChanged(height());
}

bool CodeSnippetWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == mToolBar)
    {
        if (event->type() == QEvent::HoverEnter)
        {
            this->setCursor(Qt::PointingHandCursor);
            mToolBar->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeToolBar_Highlight);
        }
        else if (event->type() == QEvent::HoverLeave)
        {
            mToolBar->setStyleSheet(CB_THEME.SS_MarkdownBlockWidget_CodeToolBar);
        }
        else if (event->type() == QEvent::MouseButtonPress)
        {
            onActionExpandTriggered();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void CodeSnippetWidget::onActionCloseTriggered()
{
    clear();
}

void CodeSnippetWidget::onActionExpandTriggered()
{
    if (mActionExpand->text() == "展开")
    {
        mActionExpand->setText("折叠");
        mActionExpand->setIcon(EXPAND_ICON.icon());

        mHorLine->setVisible(true);
        mPreviewWgt->setVisible(true);
    }
    else
    {
        mActionExpand->setText("展开");
        mActionExpand->setIcon(COLLAPSE_ICON.icon());

        mHorLine->setVisible(false);
        mPreviewWgt->setVisible(false);
    }
}

/********************************************************************
 CustomTextEdit
 自定义输入控件
*********************************************************************/
CustomTextEdit::CustomTextEdit(QWidget *parent) :
    QPlainTextEdit(parent)
{
    // 初始化输入框高度相关设置
    mMinInputHeight = 40;
    mMaxInputHeight = 400;

    setMaximumHeight(mMinInputHeight);
    connect(this, &CustomTextEdit::textChanged, this, &CustomTextEdit::adjustInputEditSize);
    // connect(this, &CustomTextEdit::textChanged, this, [this](){
    //     if (toPlainText().isEmpty())
    //     {
    //         setPlaceholderTextVisible(true);
    //     }
    // });
    connect(this, &CustomTextEdit::sizeChanged, this, &CustomTextEdit::adjustInputEditSize);

    // 文本输入框不显示边框
    setStyleSheet("QPlainTextEdit { border: none; }");

    // 设置占位文本
    setPlaceholderTextVisible(true);
}

bool CustomTextEdit::event(QEvent *event)
{
    // FIXME: 使用输入法输入时占位文本不会自动消失
    // if (event->type() == QEvent::InputMethod)
    // {
    //     qDebug() << Q_FUNC_INFO << toPlainText();
    //     setPlaceholderTextVisible(false);
    // }
    return QPlainTextEdit::event(event);
}

void CustomTextEdit::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    emit sizeChanged();
}

void CustomTextEdit::focusInEvent(QFocusEvent *event)
{
    QPlainTextEdit::focusInEvent(event);
    emit focusChange(true);
}

void CustomTextEdit::focusOutEvent(QFocusEvent *event)
{
    QPlainTextEdit::focusOutEvent(event);
    emit focusChange(false);
}

void CustomTextEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return)
    {
        if (event->modifiers() == Qt::ShiftModifier)
        {
            // 处理 Shift+Enter 组合键
            insertPlainText("\n");
        }
        else if (event->modifiers() == Qt::ControlModifier)
        {
            emit newChat();
        }
        else
        {
            emit sendMessage();
        }
    }
    else
    {
        // 调用基类的 keyPressEvent 处理其他按键事件
        QPlainTextEdit::keyPressEvent(event);
    }
}

void CustomTextEdit::setPlaceholderTextVisible(bool visible)
{
    QString text = visible ? QString("Enter 发送，Shift+Enter 换行，Ctrl+Enter 创建新对话，Alt+Enter 引用当前文件") : QString();
    setPlaceholderText(text);
}

void CustomTextEdit::adjustInputEditSize()
{
    QTextDocument *doc = document();
    QFontMetrics fm(font());
    int contentHeight = doc->size().height() * fm.lineSpacing();

    // 计算实际显示的行数
    int lineCount = 0;
    for (QTextBlock block = doc->begin(); block != doc->end(); block = block.next()) {
        QTextLayout *layout = block.layout();
        lineCount += layout->lineCount();
    }
    // 当文本行数大于1行时,为了防止文本末尾的空行导致显示进度条，将文本内容高度加上一个文本高度，
    // 将空行显示在输入控件内部，禁止进度条的显示
    if (lineCount >= 2)
    {
        contentHeight += fm.height();
    }

    // 控件高度固定比文本高度高一定的高度，数值是手动调整到合适的高度得到的
    int widgetHeight = contentHeight + 12;

    int newHeight = qMin(widgetHeight, mMaxInputHeight);
    newHeight     = qMax(mMinInputHeight,newHeight );

    if (newHeight <= mMinInputHeight) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    // 防止重复触发此槽函数
    blockSignals(true);
    setMaximumHeight(newHeight);
    setMinimumHeight(newHeight);
    blockSignals(false);

    emit heightChanged(newHeight);
}

/***************************************************************************************************
 * @brief InputWidget::InputWidget
 * @param parent
 ****************************************************************************************************/
InputWidget::InputWidget(QWidget *parent) : QFrame(parent)
    , mShowSnippet(CodeBoosterSettings::instance().showEditorSelection)
    , mHorLine(nullptr)
    , mInStreaming(false)
{
    setObjectName("InputWidget");

    // 初始化控件
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 0, 1);

    {
        mCodeSnippetWgt = new CodeSnippetWidget(this);
        mCodeSnippetWgt->setVisible(false);
        connect(CodeBoosterPlugin::instance(), &CodeBoosterPlugin::documentSelectionChanged, this, &InputWidget::onShowCodeSnippet);
        connect(mCodeSnippetWgt, &CodeSnippetWidget::heightChanged, this, &InputWidget::onChildWidgetHeightChanged);

        QVBoxLayout *containerLayout = new QVBoxLayout();
        containerLayout->addWidget(mCodeSnippetWgt);
        containerLayout->setContentsMargins(4, 4, 4, 0);
        layout->addLayout(containerLayout, 0);
    }

    {
        QHBoxLayout *inputLayout = new QHBoxLayout();
        inputLayout->setSpacing(0);
        inputLayout->setContentsMargins(0, 0, 0, 6);

        {
            mTextEdit = new CustomTextEdit(this);
            connect(mTextEdit, &CustomTextEdit::focusChange, this, &InputWidget::onTextEditFocusChange);
            connect(mTextEdit, &CustomTextEdit::heightChanged, this, &InputWidget::onChildWidgetHeightChanged);
            connect(mTextEdit, &CustomTextEdit::textChanged, this, &InputWidget::onInputTextChanged);
            connect(mTextEdit, &CustomTextEdit::sendMessage, this, [this]{
                if (!mInStreaming)
                {
                    onSendButtonClicked();
                }
            });
            connect(mTextEdit, &CustomTextEdit::newChat, this, [this]{
                QString message = mTextEdit->toPlainText().simplified();
                if (!message.isEmpty() && !mInStreaming)
                {
                    emit createNewChat();
                    onSendButtonClicked();
                }
            });

            inputLayout->addWidget(mTextEdit);
        }

        {
            mSendButton = new QPushButton(this);
            mSendButton->setText("发送");
            mSendButton->setMaximumWidth(40);
            mSendButton->setMinimumHeight(20);
            mSendButton->setDisabled(true);
            connect(mSendButton, &QPushButton::clicked, this, &InputWidget::onSendButtonClicked);

            spacer = new QWidget(this);
            spacer->setMinimumHeight(16);
            spacer->setMaximumHeight(16);

            mSpinner = new SpinnerSolution::Spinner(SpinnerSolution::SpinnerSize::Small, spacer);
            mSpinner->setVisible(false);

            // 将按钮和弹簧放到垂直布局中
            QVBoxLayout *ly = new QVBoxLayout();
            ly->setSpacing(2);
            ly->addStretch();
            ly->addWidget(spacer);
            ly->addWidget(mSendButton);
            ly->setContentsMargins(0, 0, 4, 0);

            inputLayout->addLayout(ly);
        }

        layout->addLayout(inputLayout, 1);
    }

    // 获取当前的背景颜色
    QPalette pal = palette();
    QColor bgColor = pal.color(QPalette::Base);
    mBgColorStr = bgColor.name();

    // 初始化样式
    onTextEditFocusChange(false);

    //setMaximumHeight(600);
}

InputWidget::~InputWidget()
{

}

void InputWidget::setSendButtonEnabled(bool enable, const QString &disableReason)
{
    mSendButton->setEnabled(enable);

    if (!enable)
    {
        mSendButton->setToolTip(disableReason);
    }
}

void InputWidget::waitingForReceiveMsg()
{
    mInStreaming = true;
    mTextEdit->clear();
    mSendButton->setText("停止");

    mSpinner->setVisible(true);
}

void InputWidget::messageReceiveFinished()
{
    mInStreaming = false;
    mSendButton->setText("发送");

    mSpinner->setVisible(false);
}

void InputWidget::setShowEditorSelection(bool show)
{
    mShowSnippet = show;
    if (show == false)
    {
        mCodeSnippetWgt->clear();
    }
}

void InputWidget::activateInput()
{
    mTextEdit->setFocus();

    // 自动显示文本编辑器选中的代码
    if (mCodeSnippetWgt->codeSnippet().isEmpty())
    {
        auto textEditor = BaseTextEditor::currentTextEditor();
        if (!textEditor)
            return;
        TextEditorWidget *widget = textEditor->editorWidget();

        QString fileName = widget->textDocument()->filePath().fileName();
        QString snippet = widget->selectedText();

        if(!snippet.isEmpty())
            mCodeSnippetWgt->showCodeSnippet(fileName, snippet);
    }
}

void InputWidget::onSendButtonClicked()
{
    if (!mInStreaming)
    {
        QString message = mTextEdit->toPlainText().trimmed();
        if (mCodeSnippetWgt->isVisible() && !mCodeSnippetWgt->codeSnippet().isEmpty())
        {
            message.prepend(QString("%1\n\n").arg(mCodeSnippetWgt->codeSnippet()));
            mCodeSnippetWgt->clear();
        }

        if (message.isEmpty())
            return;

        waitingForReceiveMsg();
        emit sendUserMessage(message);
    }
    else
    {
        emit stopReceivingMessage();
        onInputTextChanged();
    }
}

void InputWidget::onInputTextChanged()
{
    if (!mInStreaming)
    {
        mSendButton->setDisabled(mTextEdit->toPlainText().isEmpty());
    }
    else
    {
        mSendButton->setDisabled(false);
    }
}

void InputWidget::onShowCodeSnippet(const QString &fileName, const QString &text)
{
    if (!mShowSnippet)
        return;
    mCodeSnippetWgt->showCodeSnippet(fileName, text);
}

void InputWidget::onTextEditFocusChange(bool focus)
{
    QString styleSheetStr;

    if (focus)
    {
        styleSheetStr = QString("QFrame#InputWidget { border: 1px solid #005BBE; background-color: %1; }").arg(mBgColorStr);
    }
    else
    {
        styleSheetStr = QString("QFrame#InputWidget { border: 1px solid #A8A8A9; background-color: %1; }").arg(mBgColorStr);
    }

    setStyleSheet(styleSheetStr);
}

void InputWidget::onChildWidgetHeightChanged(int height)
{
    int selfHeight = mCodeSnippetWgt->height() + mTextEdit->height() + 16;
    //setFixedHeight(selfHeight);
    // setMinimumHeight(selfHeight);
}



}

#ifndef INPUTWIDGET_H
#define INPUTWIDGET_H

#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QPushButton>
#include <QLabel>
#include <QToolBar>

#include <solutions/spinner/spinner.h>

class NotePreviewWidget;

namespace CodeBooster::Internal{

class CustomLineWidget;

/**
 * @brief The CodeSnippetWidget class 代码段控件
 */
class CodeSnippetWidget : public QFrame
{
    Q_OBJECT
public:
    explicit CodeSnippetWidget(QWidget *parent = nullptr);

public:
    void showCodeSnippet(const QString &fileName, const QString &selectedText);
    QString codeSnippet() const;
    void clear();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event);

signals:
    void heightChanged(int height);

private slots:
    void onActionCloseTriggered();
    void onActionExpandTriggered();

private:
    bool mCodeMode;

    QVBoxLayout *mLayout;

    QToolBar *mToolBar;
    QLabel *mFileIcon;
    QLabel *mFileNameTitle;
    QAction *mActionClose;
    QAction *mActionExpand;

    CustomLineWidget *mHorLine;

    NotePreviewWidget *mPreviewWgt;
    QString mFileName;
    QString mCodeSnippet;
};

/**
 * @brief The CustomTextEdit class 自定义的文本输入框
 */
class CustomTextEdit : public QPlainTextEdit
{
     Q_OBJECT

public:
    CustomTextEdit(QWidget *parent = nullptr);

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setPlaceholderTextVisible(bool visible);

private slots:
    void adjustInputEditSize();

signals:
    void sizeChanged();
    void heightChanged(int height);
    void sendMessage();
    void newChat();
    void focusChange(bool focus);

private:
    // 常量
    int mMinInputHeight;
    int mMaxInputHeight;
    //
};

/**
 * @brief The InputWidget class 输入控件区域
 */
class InputWidget : public QFrame
{
    Q_OBJECT

public:
    InputWidget(QWidget *parent = nullptr);
    ~InputWidget();

    static bool defaultShowEditorSelection() {return true;}

public:
    void setSendButtonEnabled(bool enable, const QString &disableReason = QString());
    void waitingForReceiveMsg();
    void messageReceiveFinished();
    void setShowEditorSelection(bool show);

    void activateInput();


signals:
    void sendUserMessage(const QString &message);
    void stopReceivingMessage();
    void createNewChat();

private slots:
    void onSendButtonClicked();
    void onInputTextChanged();
    void onShowCodeSnippet(const QString &fileName, const QString &text);
    void onTextEditFocusChange(bool focus);
    void onChildWidgetHeightChanged(int height);

private:
    QString mBgColorStr;

    CodeSnippetWidget *mCodeSnippetWgt;
    bool mShowSnippet;

    CustomLineWidget *mHorLine;

    CustomTextEdit *mTextEdit;
    QPushButton *mSendButton;
    bool mInStreaming;

    QWidget *spacer;
    SpinnerSolution::Spinner *mSpinner{nullptr};
};

}

#endif // INPUTWIDGET_H

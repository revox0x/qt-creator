#include "askcodeboostertaskhandler.h"

#include <projectexplorer/task.h>
#include <coreplugin/editormanager/editormanager.h>
#include <texteditor/texteditor.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>

#include "codeboostertr.h"
#include "codeboostericons.h"
#include "codeboosterutils.h"

namespace CodeBooster::Internal {

QString AskCodeBoosterTaskHandler::sysMessage =
    R"(你是一名专业的软件工程师,专门负责检查代码中的错误信息。你能够深入分析问题，找出根本原因，并提供详细的修复方法。你的能力有:
- 解析错误信息
- 定位问题源头
- 提供修复建议)";

bool AskCodeBoosterTaskHandler::canHandle(const ProjectExplorer::Task &task) const
{
    QFileInfo fi(task.file.toFileInfo());
    if (!fi.exists() || !fi.isFile())
        return false;

    if (task.summary.isEmpty())
        return false;

    return true;
}

void AskCodeBoosterTaskHandler::handle(const ProjectExplorer::Tasks &tasks)
{
    // 编辑器打开task对应的文本
    ProjectExplorer::Task task = tasks.first();
    const int column = task.column ? task.column - 1 : 0;
    auto editor = Core::EditorManager::openEditorAt({task.file, task.movedLine, column},
                                      {},
                                      Core::EditorManager::SwitchSplitIfAlreadyVisible);

    auto textEditor = TextEditor::BaseTextEditor::currentTextEditor();

    auto editorWgt = textEditor->editorWidget();
    QTextDocument *document = editorWgt->document();

    int line = task.line;
    const int blockNumber = qMin(line, document->blockCount()) - 1;
    const QTextBlock &block = document->findBlockByNumber(blockNumber);
    if (block.isValid())
    {
        QTextCursor cursor(block);
        if (column > 0)
        {
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
        }
        else
        {
            int pos = cursor.position();
            while (document->characterAt(pos).category() == QChar::Separator_Space)
            {
                ++pos;
            }
            cursor.setPosition(pos);
        }
    }

    // 获取光标前后指定行的文本
    const int addBlockCount = 5;
    const int maxEmptyBlockCount = 3;

    int startLineNum = 0;
    int endLineNum = 0;
    QString codeSnippet;
    {
        QTextCursor cursor = textEditor->textCursor();

        // 获取当前光标所在的块
        QTextBlock currentBlock = cursor.block();
        startLineNum = cursor.blockNumber() + 1;
        endLineNum = cursor.blockNumber() + 1;

        // 获取前三行
        {
            int previousBlocksCount = addBlockCount;
            int emptyBlockCount = 0;

            QTextBlock previousBlock = currentBlock.previous();
            while (previousBlock.isValid() && previousBlocksCount > 0)
            {
                QString blockText = previousBlock.text();
                codeSnippet.prepend(blockText + "\n");

                if (!blockText.isEmpty())
                {
                    previousBlocksCount--;
                }
                else
                {
                    emptyBlockCount++;
                    if (emptyBlockCount >= maxEmptyBlockCount)
                    {
                        break;
                    }
                }

                previousBlock = previousBlock.previous();
                startLineNum--;
            }
        }

        // 获取当前行
        codeSnippet.append(currentBlock.text() + "\n");

        // 获取后三行
        {
            int nextBlocksCount = addBlockCount;
            int emptyBlockCount = 0;

            QTextBlock nextBlock = currentBlock.next();
            while (nextBlock.isValid() && nextBlocksCount > 0)
            {
                QString blockText = nextBlock.text();
                codeSnippet.append(blockText + "\n");

                if (!blockText.isEmpty())
                {
                    nextBlocksCount--;
                }
                else
                {
                    emptyBlockCount++;
                    if (emptyBlockCount >= maxEmptyBlockCount)
                    {
                        break;
                    }
                }

                nextBlock = nextBlock.next();
                endLineNum++;
            }

            // 去掉最后一个多余的换行符
            if (!codeSnippet.isEmpty()) {
                codeSnippet.chop(1);
            }
        }
    }

    QString errorSummary;
    {
        for (const ProjectExplorer::Task &task : tasks) {
            QString type;
            switch (task.type) {
            case ProjectExplorer::Task::Error:
                //: Task is of type: error
                type = Tr::tr("error:") + QLatin1Char(' ');
                break;
            case ProjectExplorer::Task::Warning:
                //: Task is of type: warning
                type = Tr::tr("warning:") + QLatin1Char(' ');
                break;
            default:
                break;
            }

            errorSummary += task.file.toUserOutput() + ':' + QString::number(task.line)
                            + ": " + type + task.description();
            errorSummary += "\n";
        }
        if (!errorSummary.isEmpty())
            errorSummary.chop(1);
    }

    QString userMsg;
    userMsg += QString("错误信息:\n%1\n\n").arg(errorSummary);
    userMsg += QString("代码:\n");
    userMsg += QString("%1:%2-%3\n").arg(task.file.fileName()).arg(startLineNum).arg(endLineNum);
    userMsg += QString("```%1\n").arg(languageFromFileSuffix(task.file.suffix()));
    userMsg += codeSnippet;
    userMsg += QString("\n```");

    // 调试
    saveToTxtFile(userMsg);
    // end

    emit askCompileError(sysMessage, userMsg);
}

QAction *AskCodeBoosterTaskHandler::createAction(QObject *parent) const
{
    QAction *showAction = new QAction(CODEBOOSTER_ICON.icon(), Tr::tr("Ask CodeBooster to fix"), parent);
    showAction->setToolTip(Tr::tr("向 CodeBooster 提问如何解决"));
    return showAction;
}



}

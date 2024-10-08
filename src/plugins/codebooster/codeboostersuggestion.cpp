#include "codeboostersuggestion.h"

#include <texteditor/texteditor.h>

#include <utils/stringutils.h>

using namespace Utils;
using namespace TextEditor;
using namespace LanguageServerProtocol;

namespace CodeBooster::Internal {

CodeBoosterSuggestion::CodeBoosterSuggestion(const QList<Completion> &completions,
                                     QTextDocument *origin,
                                     int currentCompletion)
    : m_completions(completions)
    , m_currentCompletion(currentCompletion)
{
    const Completion completion = completions.value(currentCompletion);
    const Position start = completion.range().start();
    const Position end = completion.range().end();
    QString text = start.toTextCursor(origin).block().text();
    int length = text.length() - start.character();
    if (start.line() == end.line())
        length = end.character() - start.character();
    text.replace(start.character(), length, completion.text());
    document()->setPlainText(text);
    m_start = completion.position().toTextCursor(origin);
    m_start.setKeepPositionOnInsert(true);
    setCurrentPosition(m_start.position());
}

bool CodeBoosterSuggestion::apply()
{
    reset();
    const Completion completion = m_completions.value(m_currentCompletion);
    QTextCursor cursor = completion.range().toSelection(m_start.document());
    cursor.insertText(completion.text());
    return true;
}

bool CodeBoosterSuggestion::applyLine()
{
    reset();
    const Completion completion = m_completions.value(m_currentCompletion);
    QTextCursor cursor = completion.range().toSelection(m_start.document());
    QString str=completion.text();
    int ind=str.indexOf('\n');
    if(ind<0){
        return apply();
    }
    cursor.insertText(str.left(ind+1));
    return false;
}

bool CodeBoosterSuggestion::applyWord(TextEditorWidget *widget)
{
    const Completion completion = m_completions.value(m_currentCompletion);
    const QTextCursor cursor = completion.range().toSelection(m_start.document());
    QTextCursor currentCursor = widget->textCursor();
    const QString text = completion.text();
    const int startPos = currentCursor.positionInBlock() - cursor.positionInBlock()
                         + (cursor.selectionEnd() - cursor.selectionStart());
    const int next = endOfNextWord(text, startPos);

    if (next == -1)
        return apply();

    // TODO: Allow adding more than one line
    QString subText = text.mid(startPos, next - startPos);
    subText = subText.left(subText.indexOf('\n'));
    if (subText.isEmpty())
        return false;

    currentCursor.insertText(subText);
    return false;
}

void CodeBoosterSuggestion::reset()
{
    m_start.removeSelectedText();
}

int CodeBoosterSuggestion::position()
{
    return m_start.position();
}

} // namespace CodeBooster::Internal


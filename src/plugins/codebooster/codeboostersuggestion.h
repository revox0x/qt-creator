#pragma once

#include "requests/getcompletions.h"

#include <texteditor/textdocumentlayout.h>
#include <texteditor/texteditor.h>

namespace CodeBooster::Internal {

class CodeBoosterSuggestion final : public TextEditor::TextSuggestion
{
public:
    CodeBoosterSuggestion(const QList<Completion> &completions,
                      QTextDocument *origin,
                      int currentCompletion = 0);

    bool apply() final;
    bool applyLine();
    bool applyWord(TextEditor::TextEditorWidget *widget) final;
    void reset() final;
    int position() final;

    const QList<Completion> &completions() const { return m_completions; }
    int currentCompletion() const { return m_currentCompletion; }

private:
    QList<Completion> m_completions;
    int m_currentCompletion = 0;
    QTextCursor m_start;
};
} // namespace CodeBooster::Internal

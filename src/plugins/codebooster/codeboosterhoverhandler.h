#include "requests/getcompletions.h"

#include <texteditor/basehoverhandler.h>

#include <QTextBlock>

#pragma once

namespace TextEditor { class TextSuggestion; }
namespace CodeBooster::Internal {

class CodeBoosterClient;

class CodeBoosterHoverHandler final : public TextEditor::BaseHoverHandler
{
public:
    CodeBoosterHoverHandler() = default;

protected:
    void identifyMatch(TextEditor::TextEditorWidget *editorWidget,
                       int pos,
                       ReportPriority report) final;
    void operateTooltip(TextEditor::TextEditorWidget *editorWidget, const QPoint &point) final;

private:
    QTextBlock m_block;
};

} // namespace CodeBooster::Internal

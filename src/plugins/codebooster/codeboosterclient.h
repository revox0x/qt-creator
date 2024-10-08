#pragma once

#include "codeboosterhoverhandler.h"
#include "requests/getcompletions.h"

#include <languageclient/client.h>

#include <utils/filepath.h>
#include <QHash>
#include <QTemporaryDir>


namespace CodeBooster::Internal {

class CodeBoosterClient : public LanguageClient::Client
{
    Q_OBJECT
public:
    CodeBoosterClient();
    ~CodeBoosterClient() override;

    void openDocument(TextEditor::TextDocument *document) override;

    void scheduleRequest(TextEditor::TextEditorWidget *editor);
    void requestCompletions(TextEditor::TextEditorWidget *editor);
    void handleCompletions(const GetCompletionRequest::Response &response,
                           TextEditor::TextEditorWidget *editor);
    void cancelRunningRequest(TextEditor::TextEditorWidget *editor);

    bool canOpenProject(ProjectExplorer::Project *project) override;

    bool isEnabled(ProjectExplorer::Project *project);

signals:
    void documentSelectionChanged(const QString &fileName, const QString &text);

private slots:
    void onCurrentEditorChanged(Core::IEditor *editor);

private:
    QMap<TextEditor::TextEditorWidget *, GetCompletionRequest> m_runningRequests;
    struct ScheduleData
    {
        int cursorPosition = -1;
        QTimer *timer = nullptr;
    };
    QMap<TextEditor::TextEditorWidget *, ScheduleData> m_scheduledRequests;
    CodeBoosterHoverHandler m_hoverHandler;

    // 用于记录已经连接过的editors
    QList<TextEditor::TextEditorWidget *> m_connectedEditors;
};

} // namespace CodeBooster::Internal

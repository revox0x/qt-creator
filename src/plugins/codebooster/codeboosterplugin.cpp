#include "codeboosterplugin.h"

#include "codeboosterclient.h"
#include "codeboosterconstants.h"
#include "codeboostericons.h"
#include "codeboosteroptionspage.h"
#include "codeboosterprojectpanel.h"
#include "codeboostersettings.h"
#include "codeboostersuggestion.h"
#include "codeboostertr.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/statusbarmanager.h>

#include <languageclient/languageclientmanager.h>

#include <projectexplorer/projectpanelfactory.h>

#include <texteditor/textdocumentlayout.h>
#include <texteditor/texteditor.h>

#include <QAction>
#include <QToolButton>

#include <QApplication>
#include <QTranslator>
#include <QTimeZone>

using namespace Utils;
using namespace Core;
using namespace ProjectExplorer;

namespace CodeBooster {
namespace Internal {

enum Direction { Previous, Next };
void cycleSuggestion(TextEditor::TextEditorWidget *editor, Direction direction)
{
    QTextBlock block = editor->textCursor().block();
    if (auto *suggestion = dynamic_cast<CodeBoosterSuggestion *>(
            TextEditor::TextDocumentLayout::suggestion(block))) {
        int index = suggestion->currentCompletion();
        if (direction == Previous)
            --index;
        else
            ++index;
        if (index < 0)
            index = suggestion->completions().count() - 1;
        else if (index >= suggestion->completions().count())
            index = 0;
        suggestion->reset();
        editor->insertSuggestion(std::make_unique<CodeBoosterSuggestion>(suggestion->completions(),
                                                                     editor->document(),
                                                                     index));
    }
}

void CodeBoosterPlugin::initialize()
{
    // 翻译文本
    QTranslator *translator=new QTranslator(QApplication::instance());
    QTimeZone localPosition = QDateTime::currentDateTime().timeZone();
    if(QLocale::Country::China==localPosition.country()){
        if(translator->load("CodeBooster_zh_CN",QApplication::applicationDirPath()+"/../share/qtcreator/translations")){
            QApplication::installTranslator(translator);
        }
    }
    // 兼容性
    // CodeBoosterSettings::instance().readSettings(ICore::settings());
    CodeBoosterSettings::instance().readSettings();

    // 初始化客户端
    restartClient();

    connect(&CodeBoosterSettings::instance(),
            &CodeBoosterSettings::applied,
            this,
            &CodeBoosterPlugin::restartClient);


    /// 注册一些功能按钮

    // 问题：这个按钮在哪里触发？
    QAction *requestAction = new QAction(this);
    requestAction->setText(Tr::tr("Request CodeBooster Suggestion"));
    requestAction->setToolTip(
        Tr::tr("Request CodeBooster suggestion at the current editor's cursor position."));

    connect(requestAction, &QAction::triggered, this, [this] {
        if (auto editor = TextEditor::TextEditorWidget::currentTextEditorWidget()) {
            if (m_client && m_client->reachable())
                m_client->requestCompletions(editor);
        }
    });

    ActionManager::registerAction(requestAction, Constants::CODEGEEX2_REQUEST_SUGGESTION);


    QAction *nextSuggestionAction = new QAction(this);
    nextSuggestionAction->setText(Tr::tr("Show next CodeBooster Suggestion"));
    nextSuggestionAction->setToolTip(Tr::tr(
        "Cycles through the received CodeBooster Suggestions showing the next available Suggestion."));

    connect(nextSuggestionAction, &QAction::triggered, this, [] {
        if (auto editor = TextEditor::TextEditorWidget::currentTextEditorWidget())
            cycleSuggestion(editor, Next);
    });

    ActionManager::registerAction(nextSuggestionAction, Constants::CODEGEEX2_NEXT_SUGGESTION);

    QAction *previousSuggestionAction = new QAction(this);
    previousSuggestionAction->setText(Tr::tr("Show previos CodeBooster Suggestion"));
    previousSuggestionAction->setToolTip(Tr::tr("Cycles through the received CodeBooster Suggestions "
                                                "showing the previous available Suggestion."));

    connect(previousSuggestionAction, &QAction::triggered, this, [] {
        if (auto editor = TextEditor::TextEditorWidget::currentTextEditorWidget())
            cycleSuggestion(editor, Previous);
    });

    ActionManager::registerAction(previousSuggestionAction, Constants::CODEGEEX2_PREVIOUS_SUGGESTION);

    QAction *disableAction = new QAction(this);
    disableAction->setText(Tr::tr("Disable CodeBooster"));
    disableAction->setToolTip(Tr::tr("Disable CodeBooster."));
    connect(disableAction, &QAction::triggered, this, [] {
        CodeBoosterSettings::instance().enableCodeBooster.setValue(true);
        CodeBoosterSettings::instance().apply();
    });
    ActionManager::registerAction(disableAction, Constants::CODEGEEX2_DISABLE);

    QAction *enableAction = new QAction(this);
    enableAction->setText(Tr::tr("Enable CodeBooster"));
    enableAction->setToolTip(Tr::tr("Enable CodeBooster."));
    connect(enableAction, &QAction::triggered, this, [] {
        CodeBoosterSettings::instance().enableCodeBooster.setValue(false);
        CodeBoosterSettings::instance().apply();
    });
    ActionManager::registerAction(enableAction, Constants::CODEGEEX2_ENABLE);

    QAction *toggleAction = new QAction(this);
    toggleAction->setText(Tr::tr("Toggle CodeBooster"));
    toggleAction->setCheckable(true);
    toggleAction->setChecked(CodeBoosterSettings::instance().enableCodeBooster.value());
    toggleAction->setIcon(CODEGEEX2_ICON.icon());
    connect(toggleAction, &QAction::toggled, this, [](bool checked) {
        CodeBoosterSettings::instance().enableCodeBooster.setValue(checked);
        CodeBoosterSettings::instance().apply();
    });

    ActionManager::registerAction(toggleAction, Constants::CODEGEEX2_TOGGLE);

    auto updateActions = [toggleAction, requestAction] {
        const bool enabled = CodeBoosterSettings::instance().enableCodeBooster.value();
        toggleAction->setToolTip(enabled ? Tr::tr("Disable CodeBooster.") : Tr::tr("Enable CodeBooster."));
        toggleAction->setChecked(enabled);
        requestAction->setEnabled(enabled);
    };

    connect(&CodeBoosterSettings::instance().enableCodeBooster,
            &BoolAspect::changed,
            this,
            updateActions);

    updateActions();

    auto toggleButton = new QToolButton;
    toggleButton->setDefaultAction(toggleAction);
    StatusBarManager::addStatusBarWidget(toggleButton, StatusBarManager::RightCorner);

    // 注册工程设置面板 
    setupCodeBoosterProjectPanel();
}

void CodeBoosterPlugin::extensionsInitialized()
{
    // 初始化偏好设置界面
    (void)CodeBoosterOptionsPage::instance();
}

void CodeBoosterPlugin::restartClient()
{
    LanguageClient::LanguageClientManager::shutdownClient(m_client);

    m_client = new CodeBoosterClient();
}

ExtensionSystem::IPlugin::ShutdownFlag CodeBoosterPlugin::aboutToShutdown()
{
    if (!m_client)
        return SynchronousShutdown;
    connect(m_client, &QObject::destroyed, this, &IPlugin::asynchronousShutdownFinished);
    return AsynchronousShutdown;
}

} // namespace Internal
} // namespace CodeBooster

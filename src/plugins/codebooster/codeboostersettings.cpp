#include "codeboostersettings.h"

#include <projectexplorer/project.h>
#include <QJsonObject>
#include "codeboosterconstants.h"
#include "codeboostertr.h"

namespace CodeBooster {

static void initEnableAspect(Utils::BoolAspect &enableCodeBooster)
{
    enableCodeBooster.setSettingsKey(Constants::ENABLE_CODEGEEX2);
    enableCodeBooster.setDisplayName(Tr::tr("Enable CodeBooster"));
    enableCodeBooster.setLabelText(Tr::tr("Enable CodeBooster"));
    enableCodeBooster.setToolTip(Tr::tr("Enables the CodeBooster integration."));
    enableCodeBooster.setDefaultValue(false);
}

CodeBoosterSettings::CodeBoosterSettings()
{
    setAutoApply(false);

    initEnableAspect(enableCodeBooster);

    autoComplete.setDisplayName(Tr::tr("Auto Complete"));
    autoComplete.setSettingsKey("CodeBooster.Autocomplete");
    autoComplete.setLabelText(Tr::tr("Request completions automatically"));
    autoComplete.setDefaultValue(true);
    autoComplete.setEnabler(&enableCodeBooster);
    autoComplete.setToolTip(Tr::tr("Automatically request suggestions for the current text cursor "
                                   "position after changes to the document."));

    url.setDefaultValue("http://127.0.0.1:7860/run/predict");
    url.setDisplayStyle(Utils::StringAspect::LineEditDisplay);
    url.setSettingsKey("CodeBooster.URL");
    url.setLabelText(Tr::tr("URL of CodeBooster API:"));
    url.setHistoryCompleter("CodeBooster.URL.History");
    url.setDisplayName(Tr::tr("CodeBooster API URL"));
    url.setEnabler(&enableCodeBooster);
    url.setToolTip(Tr::tr("Input URL of CodeBooster API."));

    contextLimit.setDefaultValue(8192);
    contextLimit.setRange(100,8192);
    contextLimit.setSettingsKey("CodeBooster.ContextLimit");
    contextLimit.setLabelText(Tr::tr("Context length limit:"));
    contextLimit.setDisplayName(Tr::tr("Context length limit"));
    contextLimit.setEnabler(&enableCodeBooster);
    contextLimit.setToolTip(Tr::tr("Maximum length of context send to server."));

    length.setDefaultValue(20);
    length.setRange(1,500);
    length.setSettingsKey("CodeBooster.Length");
    length.setLabelText(Tr::tr("Output sequence length:"));
    length.setDisplayName(Tr::tr("Output sequence length"));
    length.setEnabler(&enableCodeBooster);
    length.setToolTip(Tr::tr("Number of tokens to generate each time."));

    temperarure.setDefaultValue(0.2);
    temperarure.setRange(0.0,1.0);
    temperarure.setSettingsKey("CodeBooster.Temperarure");
    temperarure.setLabelText(Tr::tr("Temperature:"));
    temperarure.setDisplayName(Tr::tr("Temperature"));
    temperarure.setEnabler(&enableCodeBooster);
    temperarure.setToolTip(Tr::tr("Affects how \"random\" the model’s output is."));

    topK.setDefaultValue(0);
    topK.setRange(0,100);
    topK.setSettingsKey("CodeBooster.TopK");
    topK.setLabelText(Tr::tr("Top K:"));
    topK.setDisplayName(Tr::tr("Top K"));
    topK.setEnabler(&enableCodeBooster);
    topK.setToolTip(Tr::tr("Affects how \"random\" the model's output is."));

    topP.setDefaultValue(0.95);
    topP.setRange(0.0,1.0);
    topP.setSettingsKey("CodeBooster.TopP");
    topP.setLabelText(Tr::tr("Top P:"));
    topP.setDisplayName(Tr::tr("Top P"));
    topP.setEnabler(&enableCodeBooster);
    topP.setToolTip(Tr::tr("Affects how \"random\" the model's output is."));

    seed.setDefaultValue(8888);
    seed.setRange(0,65535);
    seed.setSettingsKey("CodeBooster.Seed");
    seed.setLabelText(Tr::tr("Seed:"));
    seed.setDisplayName(Tr::tr("Seed"));
    seed.setEnabler(&enableCodeBooster);
    seed.setToolTip(Tr::tr("Random number seed."));

    expandHeaders.setDisplayName(Tr::tr("Try to expand headers (experimnetal)"));
    expandHeaders.setSettingsKey("CodeBooster.ExpandHeaders");
    expandHeaders.setLabelText(Tr::tr("Try to expand headers (experimnetal)"));
    expandHeaders.setDefaultValue(true);
    expandHeaders.setEnabler(&enableCodeBooster);
    expandHeaders.setToolTip(Tr::tr("Try to expand headers when sending requests."));

    braceBalance.setDisplayName(Tr::tr("Brace balance (experimnetal)"));
    braceBalance.setSettingsKey("CodeBooster.BraceBalance");
    braceBalance.setLabelText(Tr::tr("Brace balance (experimnetal)"));
    braceBalance.setDefaultValue(true);
    braceBalance.setEnabler(&enableCodeBooster);
    braceBalance.setToolTip(Tr::tr("Stop suggestions from breaking brace balance."));
}

CodeBoosterSettings &CodeBoosterSettings::instance()
{
    static CodeBoosterSettings settings;
    return settings;
}

/**
 * @brief CodeBoosterSettings::completionRequestParams 自动代码补全请求参数
 * @return
 */
QJsonObject CodeBoosterSettings::completionRequestParams()
{
    QJsonObject params;
    params.insert("model", "glm-4-flash");
    params.insert("stream", false);
    params.insert("temperature", 0.1);
    params.insert("top_p", 0.7);
    params.insert("max_tokens", 512);

    return params;
}

QString CodeBoosterSettings::model()
{
    return "glm-4-flash";
}

CodeBoosterProjectSettings::CodeBoosterProjectSettings(ProjectExplorer::Project *project, QObject *parent)
{
    setAutoApply(true);

    useGlobalSettings.setSettingsKey(Constants::CODEGEEX2_USE_GLOBAL_SETTINGS);
    useGlobalSettings.setDefaultValue(true);

    initEnableAspect(enableCodeBooster);

    // QVariantMap map = project->namedSettings(Constants::CODEGEEX2_PROJECT_SETTINGS_ID).toMap();
    // 兼容性
    Utils::Store map = Utils::storeFromVariant(project->namedSettings(Constants::CODEGEEX2_PROJECT_SETTINGS_ID).toMap());
    fromMap(map);

    connect(&enableCodeBooster, &Utils::BoolAspect::changed, this, [this, project] { save(project); });
    connect(&useGlobalSettings, &Utils::BoolAspect::changed, this, [this, project] { save(project); });
}

void CodeBoosterProjectSettings::save(ProjectExplorer::Project *project)
{
    Utils::Store map;
    toMap(map);
    project->setNamedSettings(Constants::CODEGEEX2_PROJECT_SETTINGS_ID, variantFromStore(map));

    // This triggers a restart of the Copilot language server.
    CodeBoosterSettings::instance().apply();
}

void CodeBoosterProjectSettings::setUseGlobalSettings(bool useGlobal)
{
    useGlobalSettings.setValue(useGlobal);
}

bool CodeBoosterProjectSettings::isEnabled() const
{
    if (useGlobalSettings())
        return CodeBoosterSettings::instance().enableCodeBooster();
    return enableCodeBooster();
}

} // namespace CodeBooster

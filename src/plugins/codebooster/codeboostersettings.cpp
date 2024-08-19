#include "codeboostersettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <projectexplorer/project.h>
#include <coreplugin/messagemanager.h>

#include "codeboosterconstants.h"
#include "codeboostertr.h"
#include "codeboosterutils.h"

namespace CodeBooster {

static Q_LOGGING_CATEGORY(codeBooster, "qtc.codebooster.setting", QtWarningMsg)

CodeBoosterSettings::CodeBoosterSettings():
    mMaxAutoCompleteContextTokens(2048),
    mChatAttachedMessagesCount(0),
    mApplySucess(true)
{
    setAutoApply(false);

    autoComplete.setDisplayName(Tr::tr("Auto Complete"));
    autoComplete.setSettingsKey("CodeBooster.Autocomplete");
    autoComplete.setLabelText(Tr::tr("Request completions automatically"));
    autoComplete.setDefaultValue(true);
    autoComplete.setToolTip(Tr::tr("Automatically request suggestions for the current text cursor "
                                   "position after changes to the document."));

    // 初始化设置
    configJson.setDisplayName(Tr::tr("Model Config"));
    configJson.setSettingsKey("CodeBooster.ConfigJson");
    configJson.setLabelText(Tr::tr("Model Config"));
    configJson.setToolTip(Tr::tr("CodeBooster config write here"));
    configJson.setDisplayStyle(Utils::StringAspect::DisplayStyle::TextEditDisplay);
    configJson.setDefaultValue(defaultModelConfig());

    readSettings();

    QStringList errInfos;
    parseConfigSettings(configJson.value().trimmed(), errInfos);

    // 输出错误信息
    for (const QString &errInfo : errInfos)
    {
        // 当不启用自动补全时不提示补全模型的错误信息
        if (!autoComplete() && errInfo.contains("tabAutocompleteModel"))
            continue;
        CodeBooster::Internal::outputMessages({errInfo}, CodeBooster::Internal::Error);
    }
}

CodeBoosterSettings &CodeBoosterSettings::instance()
{
    static CodeBoosterSettings settings;
    return settings;
}

QJsonObject CodeBoosterSettings::buildRequestParamJson(const ModelParam &param, bool stream)
{
    QJsonObject requestJson;

    requestJson.insert("model", param.modelName);
    requestJson.insert("temperature", param.temperature);
    requestJson.insert("top_p", param.top_p);
    requestJson.insert("max_tokens", param.max_tokens);
    requestJson.insert("presence_penalty", param.presence_penalty);
    requestJson.insert("frequency_penalty", param.frequency_penalty);
    requestJson.insert("stream", stream);

    return requestJson;
}

int CodeBoosterSettings::chatAttachedMsgCount() const
{
    if ((mChatAttachedMessagesCount < 99) && (mChatAttachedMessagesCount >= 0))
        return mChatAttachedMessagesCount;

    return 0;
}

QString CodeBoosterSettings::defaultModelConfig()
{
    // TODO: 每个model都可以定义systemmsg，未定义时使用默认systemMsg
    // TODO: 是否添加自动补全延时debounceDelay？
    QString setting =R"(
{
  "tabAutocompleteModel": {
    "model": "",
    "apiKey": "",
    "apiUrl": "",
    "maxOutputTokens": 1024,
    "temperature": 0.1,
    "top_P": 0.7,
    "presence_penalty": 0,
    "frequency_penalty": 0,
    "maxContextTokens": 2048
  },
  "chatModels": [
    {
      "title": "",
      "model": "",
      "apiKey": "",
      "apiUrl": "",
      "maxOutputTokens": 2048,
      "temperature": 0.1,
      "top_P": 0.7,
      "presence_penalty": 0,
      "frequency_penalty": 0
    }
  ],
  "options": {
    "chatAttachedMessagesCount": 1
  }
}
)";

    return setting.trimmed();
}

QString CodeBoosterSettings::apiBaseToUrl(const QString &apiBase)
{
    //return QString("https://%1/chat/completions").arg(apiBase);
    return apiBase;
}

ModelParam CodeBoosterSettings::defaultAcmModelParam()
{
    ModelParam param;
    param.temperature = 0.1;
    param.top_p       = 0.7;
    param.max_tokens  = 1024;
    param.presence_penalty = 0;
    param.frequency_penalty = 0;

    return param;
}

ModelParam CodeBoosterSettings::defaultChatModelParam()
{
    ModelParam param;
    param.temperature = 0.1;
    param.top_p       = 0.7;
    param.max_tokens  = 2048;
    param.presence_penalty = 0;
    param.frequency_penalty = 0;

    return param;
}

int CodeBoosterSettings::defaultMaxAutoCompleteContextTokens()
{
    return 2048;
}

void CodeBoosterSettings::apply()
{
    // 记录原始配置
    ModelParam oldAcmParam = mAutoCompModelParam;
    QList<ModelParam> oldChatParams = mChatModelParams;
    int oldMaxAcmContextTokens = mMaxAutoCompleteContextTokens;

    // 初始化
    mAutoCompModelParam = ModelParam();
    mChatModelParams.clear();
    mMaxAutoCompleteContextTokens = 2048;

    // 读取设置
    AspectContainer::apply();
    QString configJsonStr = configJson.expandedValue();
    QStringList errInfos;

    parseConfigSettings(configJsonStr, errInfos);

    // 没有错误的情况下才应用设置
    if (errInfos.isEmpty())
    {
        mApplySucess = true;
        emit showModelConfigErrInfo(QStringList());
        emit modelConfigUpdated();
    }
    else
    {
        // 还原模型设置
        mAutoCompModelParam = oldAcmParam;
        mChatModelParams    = oldChatParams;
        mMaxAutoCompleteContextTokens = oldMaxAcmContextTokens;

        mApplySucess = false;

        // 显示错误信息
        emit showModelConfigErrInfo(errInfos);
    }
}

void CodeBoosterSettings::initConfigJsonSetting()
{
    if (QString(configJson.value()).trimmed().isEmpty())
    {
        configJson.setValue(defaultModelConfig(), BeQuiet);
    }
}

bool CodeBoosterSettings::applySucess() const
{
    return mApplySucess;
}

void CodeBoosterSettings::parseConfigSettings(const QString &configJsonStr, QStringList &errInfos)
{
    // 解析 JSON 字符串
    QJsonDocument jsonDoc = QJsonDocument::fromJson(configJsonStr.toUtf8());
    if (jsonDoc.isObject())
    {
        QJsonObject jsonObj = jsonDoc.object();

        // 解析 tabAutocompleteModel
        if (jsonObj.contains("tabAutocompleteModel") && jsonObj["tabAutocompleteModel"].isObject())
        {
            QJsonObject tabAutocompleteModelObj = jsonObj["tabAutocompleteModel"].toObject();

            // 必须字段
            if (tabAutocompleteModelObj.contains("model"))
                mAutoCompModelParam.modelName = tabAutocompleteModelObj["model"].toString();
            else
                errInfos << "缺失字段：tabAutocompleteModel.model";

            if (tabAutocompleteModelObj.contains("apiKey"))
                mAutoCompModelParam.apiKey = tabAutocompleteModelObj["apiKey"].toString();
            else
                errInfos << "缺失字段：tabAutocompleteModel.apiKey";

            if (tabAutocompleteModelObj.contains("apiUrl"))
                mAutoCompModelParam.apiUrl = apiBaseToUrl(tabAutocompleteModelObj["apiUrl"].toString());
            else
                errInfos << "缺失字段：tabAutocompleteModel.apiBase";

            // 非必须字段
            if (tabAutocompleteModelObj.contains("maxOutputTokens"))
                mAutoCompModelParam.max_tokens = tabAutocompleteModelObj["maxOutputTokens"].toInt();
            else
                mAutoCompModelParam.max_tokens = defaultAcmModelParam().max_tokens;

            if (tabAutocompleteModelObj.contains("temperature"))
                mAutoCompModelParam.temperature = tabAutocompleteModelObj["temperature"].toDouble();
            else
                mAutoCompModelParam.temperature = defaultAcmModelParam().temperature;

            if (tabAutocompleteModelObj.contains("top_P"))
                mAutoCompModelParam.top_p = tabAutocompleteModelObj["top_P"].toDouble();
            else
                mAutoCompModelParam.top_p = defaultAcmModelParam().top_p;

            if (tabAutocompleteModelObj.contains("presence_penalty"))
                mAutoCompModelParam.presence_penalty = tabAutocompleteModelObj["presence_penalty"].toInt();
            else
                mAutoCompModelParam.presence_penalty = defaultAcmModelParam().presence_penalty;

            if (tabAutocompleteModelObj.contains("frequency_penalty"))
                mAutoCompModelParam.frequency_penalty = tabAutocompleteModelObj["frequency_penalty"].toInt();
            else
                mAutoCompModelParam.frequency_penalty = defaultAcmModelParam().frequency_penalty;


            if (tabAutocompleteModelObj.contains("maxContextTokens"))
                mMaxAutoCompleteContextTokens = tabAutocompleteModelObj["maxContextTokens"].toInt();
            else
                mMaxAutoCompleteContextTokens = defaultMaxAutoCompleteContextTokens();
        }
        else
        {
            errInfos << "缺失字段：tabAutocompleteModel";
        }

        // 解析 models
        if (jsonObj.contains("chatModels") && jsonObj["chatModels"].isArray())
        {
            QJsonArray modelsArray = jsonObj["chatModels"].toArray();
            for (const QJsonValue& modelValue : modelsArray)
            {
                if (modelValue.isObject())
                {
                    QJsonObject modelObj = modelValue.toObject();
                    ModelParam modelParam;

                    // 必须字段
                    if (modelObj.contains("title"))
                        modelParam.title = modelObj["title"].toString();
                    else
                        errInfos << "缺失字段：chatModels.title";

                    if (modelObj.contains("model"))
                        modelParam.modelName = modelObj["model"].toString();
                    else
                        errInfos << "缺失字段：chatModels.model";

                    if (modelObj.contains("apiKey"))
                        modelParam.apiKey = modelObj["apiKey"].toString();
                    else
                        errInfos << "缺失字段：chatModels.apiKey";

                    if (modelObj.contains("apiUrl"))
                        modelParam.apiUrl = apiBaseToUrl(modelObj["apiUrl"].toString());
                    else
                        errInfos << "缺失字段：chatModels.apiBase";

                    // 非必须字段
                    if (modelObj.contains("maxOutputTokens"))
                        mAutoCompModelParam.max_tokens = modelObj["maxOutputTokens"].toInt();
                    else
                        mAutoCompModelParam.max_tokens = defaultChatModelParam().max_tokens;

                    if (modelObj.contains("temperature"))
                        mAutoCompModelParam.temperature = modelObj["temperature"].toDouble();
                    else
                        mAutoCompModelParam.temperature = defaultChatModelParam().temperature;

                    if (modelObj.contains("top_P"))
                        mAutoCompModelParam.top_p = modelObj["top_P"].toDouble();
                    else
                        mAutoCompModelParam.top_p = defaultChatModelParam().top_p;

                    if (modelObj.contains("presence_penalty"))
                        mAutoCompModelParam.presence_penalty = modelObj["presence_penalty"].toInt();
                    else
                        mAutoCompModelParam.presence_penalty = defaultChatModelParam().presence_penalty;

                    if (modelObj.contains("frequency_penalty"))
                        mAutoCompModelParam.frequency_penalty = modelObj["frequency_penalty"].toInt();
                    else
                        mAutoCompModelParam.frequency_penalty = defaultChatModelParam().frequency_penalty;

                    mChatModelParams.append(modelParam);
                }
            }
        }
        else
        {
            errInfos << "缺失字段：chatModels";
        }

        // 解析options
        if (jsonObj.contains("options") && jsonObj["options"].isObject())
        {
            QJsonObject optionsObj = jsonObj["options"].toObject();

            if (optionsObj.contains("chatAttachedMessagesCount"))
            {
                mChatAttachedMessagesCount = optionsObj["chatAttachedMessagesCount"].toInt();
            }
        }
        else
        {
            errInfos << "options";
        }
    }
    else
    {
        errInfos << QString("JSON 不是一个对象");
    }
}

CodeBoosterProjectSettings::CodeBoosterProjectSettings(ProjectExplorer::Project *project, QObject *parent)
{
    setAutoApply(true);

    useGlobalSettings.setSettingsKey(Constants::CODEBOOSTER_USE_GLOBAL_SETTINGS);
    useGlobalSettings.setDefaultValue(true);

    enableAutoComplete.setSettingsKey("CodeBooster.Autocomplete");
    enableAutoComplete.setDisplayName(Tr::tr("Enable CodeBooster AutoComplete"));
    enableAutoComplete.setLabelText(Tr::tr("Enable CodeBooster AutoComplete"));
    enableAutoComplete.setToolTip(Tr::tr("Enables the CodeBooster Intergation AutoComplete."));
    enableAutoComplete.setDefaultValue(false);

    Utils::Store map = Utils::storeFromVariant(project->namedSettings(Constants::CODEBOOSTER_PROJECT_SETTINGS_ID).toMap());
    fromMap(map);

    connect(&enableAutoComplete, &Utils::BoolAspect::changed, this, [this, project] { save(project); });
    connect(&useGlobalSettings, &Utils::BoolAspect::changed, this, [this, project] { save(project); });
}

void CodeBoosterProjectSettings::save(ProjectExplorer::Project *project)
{
    Utils::Store map;
    toMap(map);
    project->setNamedSettings(Constants::CODEBOOSTER_PROJECT_SETTINGS_ID, variantFromStore(map));

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
        return CodeBoosterSettings::instance().autoComplete();
    return enableAutoComplete();
}

} // namespace CodeBooster

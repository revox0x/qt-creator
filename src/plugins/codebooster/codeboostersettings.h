#ifndef CODEBOOSTER_CODEBOOSTERSETTINGS_H
#define CODEBOOSTER_CODEBOOSTERSETTINGS_H

#include <utils/aspects.h>
#include <QLoggingCategory>

namespace ProjectExplorer {
class Project;
}

namespace CodeBooster {
Q_DECLARE_LOGGING_CATEGORY(codeBooster)

struct ModelParam
{
    QString title;      ///< 模型参数名称
    QString modelName;  ///< 模型名称
    QString apiUrl;    ///< 请求地址
    QString apiKey;     ///< API KEY

    // 模型参数
    double temperature = 0.1;
    double top_p       = 0.7;
    int    max_tokens  = 2048;
    int    presence_penalty = 0;
    int    frequency_penalty = 0;

    void overrideParams(const QMap<QString, QVariant> &overrideParams)
    {
        for (auto it = overrideParams.constBegin(); it != overrideParams.constEnd(); ++it) {
            const QString& key = it.key();
            const QVariant& value = it.value();

            if (key == "temperature") {
                temperature = value.toDouble();
            } else if (key == "top_p") {
                top_p = value.toDouble();
            } else if (key == "max_tokens") {
                max_tokens = value.toInt();
            } else if (key == "presence_penalty") {
                presence_penalty = value.toInt();
            } else if (key == "frequency_penalty") {
                frequency_penalty = value.toInt();
            }
        }

        validate();
    }

    void validate()
    {
        if ((temperature < 0) || (temperature > 2.0))
        {
            temperature = 0.1;
        }

        if ((top_p < 0) || (top_p > 1))
        {
            top_p = 0.7;
        }

        if ((max_tokens < 0) || (max_tokens > 8192))
        {
            max_tokens = 2048;
        }

        if ((presence_penalty < -2.0) || (presence_penalty > 2.0))
        {
            presence_penalty = 0;
        }

        if ((frequency_penalty < -2.0) || (frequency_penalty > 2.0))
        {
            frequency_penalty = 0;
        }
    }
};

class CodeBoosterSettings : public Utils::AspectContainer
{
    Q_OBJECT
public:
    CodeBoosterSettings();

    /// 单一实例
    static CodeBoosterSettings &instance();

    static QJsonObject buildRequestParamJson(const ModelParam &param, bool stream);

    // 选项控件
    Utils::BoolAspect autoComplete{this};
    Utils::StringAspect configJson{this};
    // end

    ModelParam acmParam() const {return mAutoCompModelParam;}
    int acmMaxContextTokens() const {return mMaxAutoCompleteContextTokens;}
    QList<ModelParam> chatParams() const
    {
        return mChatModelParams;
    }
    int chatAttachedMsgCount() const;

public:
    void apply()        override;

    void initConfigJsonSetting();
    bool applySucess() const;

    // 自动代码补全中非模型参数且不需要开放给用户的选项设置
    bool   braceBalance()        {return true;}
    bool   expandHeaders()       {return true;}
    double prefixPercentage()    {return 0.5;}
    double maxSuffixPercentate() {return 0.5;}
    // end

public:
    // 不持久化的设置
    bool showEditorSelection = true;  ///< 显示编辑器选中文本
    // end

signals:
    void modelConfigUpdated();
    void showModelConfigErrInfo(const QStringList &errInfos);

private:
    void parseConfigSettings(const QString &configJsonStr, QStringList &errInfos);
    QString defaultModelConfig();
    QString apiBaseToUrl(const QString &apiBase);

    static ModelParam defaultAcmModelParam();
    static ModelParam defaultChatModelParam();
    static int defaultMaxAutoCompleteContextTokens();

private:
    // 设置信息
    ModelParam mAutoCompModelParam;     ///< 自动补全模型参数
    QList<ModelParam> mChatModelParams; ///< 对话模型参数
    int mMaxAutoCompleteContextTokens;  ///< 自动补全的最大上下文长度
    int mChatAttachedMessagesCount;     ///< 对话时附加的最大消息数量
    // end

    bool mApplySucess; ///< 上一次设置是否有效  
};

class CodeBoosterProjectSettings : public Utils::AspectContainer
{
public:
    CodeBoosterProjectSettings(ProjectExplorer::Project *project, QObject *parent = nullptr);

    void save(ProjectExplorer::Project *project);
    void setUseGlobalSettings(bool useGlobal);

    bool isEnabled() const;

    Utils::BoolAspect enableAutoComplete{this};
    Utils::BoolAspect useGlobalSettings{this};
};

} // namespace CodeBooster

#endif // CODEBOOSTER_CODEBOOSTERSETTINGS_H

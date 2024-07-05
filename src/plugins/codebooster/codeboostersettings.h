#ifndef CODEGEEX2_CODEGEEX2SETTINGS_H
#define CODEGEEX2_CODEGEEX2SETTINGS_H

#include <utils/aspects.h>

namespace ProjectExplorer {
class Project;
}

namespace CodeBooster {

class CodeBoosterSettings : public Utils::AspectContainer
{
public:
    CodeBoosterSettings();

    static CodeBoosterSettings &instance();

    Utils::BoolAspect enableCodeBooster{this};
    Utils::BoolAspect autoComplete{this};
    Utils::StringAspect url{this};
    Utils::IntegerAspect contextLimit{this};
    Utils::IntegerAspect length{this};
    Utils::DoubleAspect temperarure{this};
    Utils::IntegerAspect topK{this};
    Utils::DoubleAspect topP{this};
    Utils::IntegerAspect seed{this};
    Utils::BoolAspect expandHeaders{this};
    Utils::BoolAspect braceBalance{this};

    // TODO: 考虑是否开放设置选项
    double prefixPercentage() {return 0.5;}
    double maxSuffixPercentate() {return 0.5;}

    // TODO: 添加自定义系统prompt选项

    //TODO: 请求参数从设置中读取
    QJsonObject completionRequestParams();

    QString model();
};

class CodeBoosterProjectSettings : public Utils::AspectContainer
{
public:
    CodeBoosterProjectSettings(ProjectExplorer::Project *project, QObject *parent = nullptr);

    void save(ProjectExplorer::Project *project);
    void setUseGlobalSettings(bool useGlobal);

    bool isEnabled() const;

    Utils::BoolAspect enableCodeBooster{this};
    Utils::BoolAspect useGlobalSettings{this};
};

} // namespace CodeBooster

#endif // CODEGEEX2_CODEGEEX2SETTINGS_H

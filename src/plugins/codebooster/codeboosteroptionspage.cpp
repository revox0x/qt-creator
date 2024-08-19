#include "codeboosteroptionspage.h"

#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include "codeboosterconstants.h"
#include "codeboostersettings.h"

#include <coreplugin/minisplitter.h>

namespace CodeBooster {

class CodeBoosterOptionsPageWidget : public Core::IOptionsPageWidget
{
public:
    CodeBoosterOptionsPageWidget()
    {
        using namespace Layouting;

        QLabel *errorLabel = new QLabel(this);

        // ConfigEditorWidget *jsonConfigEditor = new ConfigEditorWidget(this);
        // 可以考虑使用自定义的widget显示，方便高亮json语法，参考clangformat.cpp
        Column {
            CodeBoosterSettings::instance().autoComplete, br,
            hr, br,
            CodeBoosterSettings::instance().configJson, br,
            errorLabel, br
        }.attachTo(this);

        setOnApply([] {
            CodeBoosterSettings::instance().apply();
            if (CodeBoosterSettings::instance().applySucess())
            {
                CodeBoosterSettings::instance().writeSettings();
            }
        });

        CodeBoosterSettings::instance().initConfigJsonSetting();

        errorLabel->setVisible(false);
        connect(&CodeBoosterSettings::instance(), &CodeBoosterSettings::showModelConfigErrInfo,
                this, [=](const QStringList &errInfos){
            if (errInfos.isEmpty())
            {
                errorLabel->setVisible(false);
            }
            else
            {
                errorLabel->setVisible(true);
                QString htmlStr = "<b><font color='red'>保存失败，配置格式错误：</font></b><br>";
                for (const QString &err : errInfos)
                {
                    htmlStr += QString("<font color='red'>%1</font><br>").arg(err);
                }
                // 去掉末尾换行
                htmlStr.chop(4);
                errorLabel->setText(htmlStr);
            }
        });
    }
};

CodeBoosterOptionsPage::CodeBoosterOptionsPage()
{
    setId(Constants::CODEBOOSTER_GENERAL_OPTIONS_ID);
    setDisplayName("CodeBooster");
    setCategory(Constants::CODEBOOSTER_GENERAL_OPTIONS_CATEGORY);
    setDisplayCategory(Constants::CODEBOOSTER_GENERAL_OPTIONS_DISPLAY_CATEGORY);
    setCategoryIconPath(":/codebooster/images/settingscategory_codebooster.png");
    setWidgetCreator([] { return new CodeBoosterOptionsPageWidget; });
}

CodeBoosterOptionsPage &CodeBoosterOptionsPage::instance()
{
    static CodeBoosterOptionsPage settingsPage;
    return settingsPage;
}


} // namespace CodeBooster

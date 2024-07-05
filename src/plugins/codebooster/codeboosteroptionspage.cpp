#include "codeboosteroptionspage.h"

#include <coreplugin/icore.h>
#include <utils/layoutbuilder.h>
#include "codeboosterconstants.h"
#include "codeboostersettings.h"

namespace CodeBooster {

class CodeBoosterOptionsPageWidget : public Core::IOptionsPageWidget
{
public:
    CodeBoosterOptionsPageWidget()
    {
        using namespace Layouting;

        Column {
            CodeBoosterSettings::instance().enableCodeBooster, br,
            CodeBoosterSettings::instance().autoComplete, br,
            CodeBoosterSettings::instance().url, br,
            CodeBoosterSettings::instance().contextLimit, br,
            CodeBoosterSettings::instance().length, br,
            CodeBoosterSettings::instance().temperarure, br,
            CodeBoosterSettings::instance().topK, br,
            CodeBoosterSettings::instance().topP, br,
            CodeBoosterSettings::instance().seed, br,
            CodeBoosterSettings::instance().expandHeaders, br,
            CodeBoosterSettings::instance().braceBalance, br,
            st
        }.attachTo(this);

        setOnApply([] {
            CodeBoosterSettings::instance().apply();
            // 兼容性
            //CodeBoosterSettings::instance().writeSettings(Core::ICore::settings());
            CodeBoosterSettings::instance().writeSettings();
        });
    }
};

CodeBoosterOptionsPage::CodeBoosterOptionsPage()
{
    setId(Constants::CODEGEEX2_GENERAL_OPTIONS_ID);
    setDisplayName("CodeBooster");
    setCategory(Constants::CODEGEEX2_GENERAL_OPTIONS_CATEGORY);
    setDisplayCategory(Constants::CODEGEEX2_GENERAL_OPTIONS_DISPLAY_CATEGORY);
    setCategoryIconPath(":/codebooster/images/settingscategory_codebooster.png");
    setWidgetCreator([] { return new CodeBoosterOptionsPageWidget; });
}

CodeBoosterOptionsPage &CodeBoosterOptionsPage::instance()
{
    static CodeBoosterOptionsPage settingsPage;
    return settingsPage;
}

} // namespace CodeBooster

#include "codeboosterprojectpanel.h"
#include "codeboosterconstants.h"
#include "codeboostersettings.h"


#include <projectexplorer/project.h>
#include <projectexplorer/projectpanelfactory.h>
#include <projectexplorer/projectsettingswidget.h>

#include <utils/layoutbuilder.h>

#include <QWidget>

using namespace ProjectExplorer;

namespace CodeBooster::Internal {

class CodeBoosterProjectSettingsWidget : public ProjectExplorer::ProjectSettingsWidget
{
public:
    CodeBoosterProjectSettingsWidget()
    {
        setGlobalSettingsId(Constants::CODEGEEX2_GENERAL_OPTIONS_ID);
        setUseGlobalSettingsCheckBoxVisible(true);
    }
};

ProjectSettingsWidget *createCodeBoosterProjectPanel(Project *project)
{
    using namespace Layouting;
    using namespace ProjectExplorer;

    auto widget = new CodeBoosterProjectSettingsWidget;
    auto settings = new CodeBoosterProjectSettings(project, widget);

    QObject::connect(widget,
                     &ProjectSettingsWidget::useGlobalSettingsChanged,
                     settings,
                     &CodeBoosterProjectSettings::setUseGlobalSettings);

    widget->setUseGlobalSettings(settings->useGlobalSettings());
    widget->setEnabled(!settings->useGlobalSettings());

    QObject::connect(widget,
                     &ProjectSettingsWidget::useGlobalSettingsChanged,
                     widget,
                     [widget](bool useGlobal) { widget->setEnabled(!useGlobal); });

    // clang-format off
    Column {
        settings->enableCodeBooster,
    }.attachTo(widget);
    // clang-format on

    return widget;
}

class CodeBoosterProjectPanelFactory final : public ProjectPanelFactory
{
public:
    CodeBoosterProjectPanelFactory()
    {
        setPriority(1000);
        setDisplayName(("CodeBooster"));
        setCreateWidgetFunction(&createCodeBoosterProjectPanel);
    }
};


void setupCodeBoosterProjectPanel()
{
    static CodeBoosterProjectPanelFactory theCodeBoosterProjectPanelFactory;
}

} // namespace CodeBooster::Internal

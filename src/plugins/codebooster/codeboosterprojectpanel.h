#pragma once

namespace ProjectExplorer {
class ProjectSettingsWidget;
class Project;
} // namespace ProjectExplorer

namespace CodeBooster::Internal {

ProjectExplorer::ProjectSettingsWidget *createCodeBoosterProjectPanel(ProjectExplorer::Project *project);

void setupCodeBoosterProjectPanel();

} // namespace CodeBooster::Internal

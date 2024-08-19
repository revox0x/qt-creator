// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customexecutablerunconfiguration.h"

#include "projectexplorerconstants.h"
#include "projectexplorertr.h"
#include "runconfigurationaspects.h"
#include "target.h"

#include <utils/processinterface.h>

using namespace Utils;

namespace ProjectExplorer {

// CustomExecutableRunConfiguration

CustomExecutableRunConfiguration::CustomExecutableRunConfiguration(Target *target)
    : CustomExecutableRunConfiguration(target, Constants::CUSTOM_EXECUTABLE_RUNCONFIG_ID)
{}

CustomExecutableRunConfiguration::CustomExecutableRunConfiguration(Target *target, Id id)
    : RunConfiguration(target, id)
{
    environment.setSupportForBuildEnvironment(target);

    executable.setDeviceSelector(target, ExecutableAspect::HostDevice);
    executable.setSettingsKey("ProjectExplorer.CustomExecutableRunConfiguration.Executable");
    executable.setReadOnly(false);
    executable.setHistoryCompleter("Qt.CustomExecutable.History");
    executable.setExpectedKind(PathChooser::ExistingCommand);
    executable.setEnvironment(environment.environment());

    arguments.setMacroExpander(macroExpander());

    workingDir.setMacroExpander(macroExpander());
    workingDir.setEnvironment(&environment);

    connect(&environment, &EnvironmentAspect::environmentChanged, this, [this]  {
         executable.setEnvironment(environment.environment());
    });

    setDefaultDisplayName(defaultDisplayName());
    setUsesEmptyBuildKeys();
}

bool CustomExecutableRunConfiguration::isEnabled(Id) const
{
    return true;
}

QString CustomExecutableRunConfiguration::defaultDisplayName() const
{
    if (executable().isEmpty())
        return Tr::tr("Custom Executable");
    return Tr::tr("Run %1").arg(executable().toUserOutput());
}

Tasks CustomExecutableRunConfiguration::checkForIssues() const
{
    Tasks tasks;
    if (executable().isEmpty()) {
        tasks << createConfigurationIssue(Tr::tr("You need to set an executable in the custom run "
                                             "configuration."));
    }
    return tasks;
}

// Factories

CustomExecutableRunConfigurationFactory::CustomExecutableRunConfigurationFactory() :
    FixedRunConfigurationFactory(Tr::tr("Custom Executable"))
{
    registerRunConfiguration<CustomExecutableRunConfiguration>(
        Constants::CUSTOM_EXECUTABLE_RUNCONFIG_ID);
}

CustomExecutableRunWorkerFactory::CustomExecutableRunWorkerFactory()
{
    setProduct<SimpleTargetRunner>();
    addSupportedRunMode(Constants::NORMAL_RUN_MODE);
    addSupportedRunConfig(Constants::CUSTOM_EXECUTABLE_RUNCONFIG_ID);
}

} // namespace ProjectExplorer

// Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "androidruncontrol.h"

#include "androidconstants.h"
#include "androidglobal.h"
#include "androidrunconfiguration.h"
#include "androidrunner.h"

#include <utils/utilsicons.h>

#include <projectexplorer/projectexplorerconstants.h>

using namespace ProjectExplorer;

namespace Android::Internal {

class AndroidRunSupport final : public AndroidRunner
{
public:
    explicit AndroidRunSupport(RunControl *runControl);
    ~AndroidRunSupport() override;
};

AndroidRunSupport::AndroidRunSupport(RunControl *runControl)
    : AndroidRunner(runControl)
{
    runControl->setIcon(Utils::Icons::RUN_SMALL_TOOLBAR);
}

AndroidRunSupport::~AndroidRunSupport()
{
    stop();
}

class AndroidRunWorkerFactory final : public RunWorkerFactory
{
public:
    AndroidRunWorkerFactory()
    {
        setProduct<AndroidRunSupport>();
        addSupportedRunMode(ProjectExplorer::Constants::NORMAL_RUN_MODE);
        addSupportedRunConfig(Constants::ANDROID_RUNCONFIG_ID);
    }
};

void setupAndroidRunWorker()
{
    static AndroidRunWorkerFactory theAndroidRunWorkerFactory;
}

} // Android::Internal

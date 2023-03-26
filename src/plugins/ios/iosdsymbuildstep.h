// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/abstractprocessstep.h>

namespace Ios::Internal {

class IosDsymBuildStepFactory : public ProjectExplorer::BuildStepFactory
{
public:
    explicit IosDsymBuildStepFactory();
};

} // Ios::Internal

// Copyright (C) 2016 Openismus GmbH.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/buildstep.h>

namespace AutotoolsProjectManager {
namespace Internal {

class AutogenStepFactory final : public ProjectExplorer::BuildStepFactory
{
public:
    AutogenStepFactory();
};

} // namespace Internal
} // namespace AutotoolsProjectManager

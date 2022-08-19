// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

#include "buildstep.h"

namespace ProjectExplorer {
namespace Internal {

class ProcessStepFactory final : public BuildStepFactory
{
public:
    ProcessStepFactory();
};

} // namespace Internal
} // namespace ProjectExplorer

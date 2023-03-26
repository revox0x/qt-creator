// Copyright (C) 2019 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filepath.h>

namespace Qdb::Internal {

enum class QdbTool {
    FlashingWizard,
    Qdb
};

Utils::FilePath findTool(QdbTool tool);
QString overridingEnvironmentVariable(QdbTool tool);
void showMessage(const QString &message, bool important = false);
QString settingsGroupKey();
QString settingsKey(QdbTool tool);

} // Qdb::Internal

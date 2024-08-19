// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.h"

#include <projectexplorer/projectmacro.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/runconfigurationaspects.h>

namespace CMakeProjectManager {

enum TargetType {
    ExecutableType,
    StaticLibraryType,
    DynamicLibraryType,
    ObjectLibraryType,
    UtilityType
};

using Backtrace = QVector<ProjectExplorer::FolderNode::LocationInfo>;
using Backtraces = QVector<Backtrace>;

class CMAKE_EXPORT CMakeBuildTarget
{
public:
    QString title;
    Utils::FilePath executable; // TODO: rename to output?
    QList<ProjectExplorer::Launcher> launchers;
    TargetType targetType = UtilityType;
    bool linksToQtGui = false;
    bool qtcRunnable = true;
    Utils::FilePath workingDirectory;
    Utils::FilePath sourceDirectory;
    Utils::FilePath makeCommand;
    Utils::FilePaths libraryDirectories;
    Utils::FilePaths sourceFiles;

    Backtrace backtrace;

    Backtraces dependencyDefinitions;
    Backtraces sourceDefinitions;
    Backtraces defineDefinitions;
    Backtraces includeDefinitions;
    Backtraces installDefinitions;

    // code model
    QList<Utils::FilePath> includeFiles;
    QStringList compilerOptions;
    ProjectExplorer::Macros macros;
    QList<Utils::FilePath> files;
};

} // namespace CMakeProjectManager

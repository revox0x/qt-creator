// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.h"

#include "cppprojectfile.h"

#include <projectexplorer/buildtargettype.h>
#include <projectexplorer/headerpath.h>
#include <projectexplorer/projectmacro.h>
#include <projectexplorer/rawprojectpart.h>

#include <cplusplus/Token.h>

#include <utils/cpplanguage_details.h>
#include <utils/filepath.h>
#include <utils/id.h>

#include <QString>
#include <QSharedPointer>

namespace ProjectExplorer { class Project; }

namespace CppEditor {

class CPPEDITOR_EXPORT ProjectPart
{
public:
    using ConstPtr = QSharedPointer<const ProjectPart>;

    static ConstPtr create(const Utils::FilePath &topLevelProject,
                      const ProjectExplorer::RawProjectPart &rpp = {},
                      const QString &displayName = {},
                      const ProjectFiles &files = {},
                      Utils::Language language = Utils::Language::Cxx,
                      Utils::LanguageExtensions languageExtensions = {},
                      const ProjectExplorer::RawProjectPartFlags &flags = {},
                      const ProjectExplorer::ToolchainInfo &tcInfo = {})
    {
        return ConstPtr(new ProjectPart(topLevelProject, rpp, displayName, files, language,
                                   languageExtensions, flags, tcInfo));
    }

    QString id() const;
    QString projectFileLocation() const;
    bool hasProject() const { return !topLevelProject.isEmpty(); }
    bool belongsToProject(const ProjectExplorer::Project *project) const;
    bool belongsToProject(const Utils::FilePath &project) const;
    ProjectExplorer::Project *project() const;

    static QByteArray readProjectConfigFile(const QString &projectConfigFile);

    const Utils::FilePath topLevelProject;
    const QString displayName;
    const QString projectFile;
    const QString projectConfigFile; // Generic Project Manager only

    const int projectFileLine = -1;
    const int projectFileColumn = -1;
    const QString callGroupId;

    // Versions, features and extensions
    const Utils::Language language = Utils::Language::Cxx;
    const Utils::LanguageVersion &languageVersion = m_macroReport.languageVersion;
    const Utils::LanguageExtensions languageExtensions = Utils::LanguageExtension::None;
    const Utils::QtMajorVersion qtVersion = Utils::QtMajorVersion::Unknown;

    // Files
    const ProjectFiles files;
    const QStringList includedFiles;
    const QStringList precompiledHeaders;
    const ProjectExplorer::HeaderPaths headerPaths;

    // Macros
    const ProjectExplorer::Macros projectMacros;
    const ProjectExplorer::Macros &toolchainMacros = m_macroReport.macros;

    // Build system
    const QString buildSystemTarget;
    const ProjectExplorer::BuildTargetType buildTargetType
        = ProjectExplorer::BuildTargetType::Unknown;
    const bool selectedForBuilding = true;

    // Toolchain
    const Utils::Id toolchainType;
    const bool isMsvc2015Toolchain = false;
    const QString toolchainTargetTriple;
    const bool targetTripleIsAuthoritative;
    const ProjectExplorer::Abi toolchainAbi = ProjectExplorer::Abi::hostAbi();
    const Utils::FilePath toolchainInstallDir;
    const Utils::FilePath compilerFilePath;
    const Utils::WarningFlags warningFlags = Utils::WarningFlags::Default;

    // Misc
    const QStringList extraCodeModelFlags;
    const QStringList compilerFlags;

private:
    ProjectPart(const Utils::FilePath &topLevelProject,
                const ProjectExplorer::RawProjectPart &rpp,
                const QString &displayName,
                const ProjectFiles &files,
                Utils::Language language,
                Utils::LanguageExtensions languageExtensions,
                const ProjectExplorer::RawProjectPartFlags &flags,
                const ProjectExplorer::ToolchainInfo &tcInfo);

    CPlusPlus::LanguageFeatures deriveLanguageFeatures() const;

    const ProjectExplorer::Toolchain::MacroInspectionReport m_macroReport;

public:
    // Must come last due to initialization order.
    const CPlusPlus::LanguageFeatures languageFeatures;
};

class ProjectPartInfo {
public:
    enum Hint {
        NoHint = 0,
        IsFallbackMatch  = 1 << 0,
        IsAmbiguousMatch = 1 << 1,
        IsPreferredMatch = 1 << 2,
        IsFromProjectMatch = 1 << 3,
        IsFromDependenciesMatch = 1 << 4,
    };
    Q_DECLARE_FLAGS(Hints, Hint)

    ProjectPartInfo() = default;
    ProjectPartInfo(const ProjectPart::ConstPtr &projectPart,
                    const QList<ProjectPart::ConstPtr> &projectParts,
                    Hints hints)
        : projectPart(projectPart)
        , projectParts(projectParts)
        , hints(hints)
    {
    }

    ProjectPart::ConstPtr projectPart;
    QList<ProjectPart::ConstPtr> projectParts; // The one above as first plus alternatives.
    Hints hints = NoHint;
};

} // namespace CppEditor

// Copyright (C) 2020 Alexis Jeandet.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "kitdata.h"
#include "mesoninfoparser.h"
#include "mesonoutputparser.h"
#include "mesonprojectnodes.h"
#include "mesontools.h"

#include <projectexplorer/buildsystem.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/rawprojectpart.h>

#include <utils/processinterface.h>

#include <QFuture>
#include <QQueue>

#include <utils/qtcprocess.h>

namespace MesonProjectManager::Internal {

class MesonProjectParser : public QObject
{
    Q_OBJECT

    enum class IntroDataType { file, stdo };
    struct ParserData
    {
        MesonInfoParser::Result data;
        std::unique_ptr<MesonProjectNode> rootNode;
    };

public:
    MesonProjectParser(const Utils::Id &meson,
                       const Utils::Environment &env,
                       ProjectExplorer::Project *project);

    bool configure(const Utils::FilePath &sourcePath,
                   const Utils::FilePath &buildPath,
                   const QStringList &args);
    bool wipe(const Utils::FilePath &sourcePath,
              const Utils::FilePath &buildPath,
              const QStringList &args);
    bool setup(const Utils::FilePath &sourcePath,
               const Utils::FilePath &buildPath,
               const QStringList &args,
               bool forceWipe = false);
    bool parse(const Utils::FilePath &sourcePath, const Utils::FilePath &buildPath);
    bool parse(const Utils::FilePath &sourcePath);

    std::unique_ptr<MesonProjectNode> takeProjectNode() { return std::move(m_rootNode); }

    const BuildOptionsList &buildOptions() const { return m_parserResult.buildOptions; };
    const TargetsList &targets() const { return m_parserResult.targets; }
    const QStringList &targetsNames() const { return m_targetsNames; }

    QList<ProjectExplorer::BuildTargetInfo> appsTargets() const;

    ProjectExplorer::RawProjectParts buildProjectParts(
        const ProjectExplorer::Toolchain *cxxToolchain,
        const ProjectExplorer::Toolchain *cToolchain);

    void setEnvironment(const Utils::Environment &environment) { m_env = environment; }

    void setQtVersion(Utils::QtMajorVersion v) { m_qtVersion = v; }

    bool matchesKit(const KitData &kit);

    bool usesSameMesonVersion(const Utils::FilePath &buildPath);

signals:
     void parsingCompleted(bool success);

private:
    bool startParser();
    static ParserData *extractParserResults(const Utils::FilePath &srcDir,
                                            MesonInfoParser::Result &&parserResult);
    void update(const QFuture<ParserData *> &data);
    ProjectExplorer::RawProjectPart buildRawPart(const Target &target,
                                                 const Target::SourceGroup &sources,
                                                 const ProjectExplorer::Toolchain *cxxToolchain,
                                                 const ProjectExplorer::Toolchain *cToolchain);

    MesonOutputParser m_outputParser;
    Utils::Environment m_env;
    Utils::Id m_meson;
    Utils::FilePath m_buildDir;
    Utils::FilePath m_srcDir;
    QFuture<ParserData *> m_parserFutureResult;
    bool m_configuring = false;
    IntroDataType m_introType = IntroDataType::file;
    MesonInfoParser::Result m_parserResult;
    QStringList m_targetsNames;
    Utils::QtMajorVersion m_qtVersion = Utils::QtMajorVersion::Unknown;
    std::unique_ptr<MesonProjectNode> m_rootNode; // <- project tree root node
    QString m_projectName;
    // maybe moving meson to build step could make this class simpler
    // also this should ease command dependencies
    QQueue<std::tuple<Utils::ProcessRunData, bool>> m_pendingCommands;

    bool run(const Utils::ProcessRunData &runData, const QString &projectName, bool captureStdo = false);

    void handleProcessDone();
    void setupProcess(const Utils::ProcessRunData &runData, const QString &projectName,
                      bool captureStdo);
    bool sanityCheck(const Utils::ProcessRunData &runData) const;

    void processStandardOutput();
    void processStandardError();

    std::unique_ptr<Utils::Process> m_process;
    QElapsedTimer m_elapsed;
    QByteArray m_stdo;
    QByteArray m_stderr;
};

} // namespace MesonProjectManager::Internal

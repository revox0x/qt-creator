// Copyright (C) 2020 Alexis Jeandet.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mesonprojectparser.h"

#include "common.h"
#include "mesoninfoparser.h"
#include "mesonpluginconstants.h"
#include "mesonprojectmanagertr.h"
#include "mesonprojectnodes.h"
#include "mesontools.h"

#include <coreplugin/messagemanager.h>
#include <coreplugin/progressmanager/processprogress.h>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/taskhub.h>

#include <utils/async.h>
#include <utils/environment.h>
#include <utils/fileinprojectfinder.h>
#include <utils/stringutils.h>

#include <optional>

using namespace Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace MesonProjectManager::Internal {

static Q_LOGGING_CATEGORY(mesonProcessLog, "qtc.meson.buildsystem", QtWarningMsg);

struct CompilerArgs
{
    QStringList args;
    QStringList includePaths;
    Macros macros;
};

static void buildTargetTree(std::unique_ptr<MesonProjectNode> &root, const Target &target)
{
    const auto path = FilePath::fromString(target.definedIn);
    for (const auto &group : target.sources) {
        for (const auto &file : group.sources) {
            root->addNestedNode(std::make_unique<FileNode>(FilePath::fromString(file),
                                                           FileType::Source));
        }
    }
    for (const auto &extraFile : target.extraFiles) {
        root->addNestedNode(std::make_unique<FileNode>(FilePath::fromString(extraFile),
                                                       FileType::Unknown));
    }
}

static void addTargetNode(std::unique_ptr<MesonProjectNode> &root, const Target &target)
{
    root->findNode([&root, &target, path = FilePath::fromString(target.definedIn)](Node *node) {
        if (node->filePath() == path.absolutePath()) {
            auto asFolder = dynamic_cast<FolderNode *>(node);
            if (asFolder) {
                auto targetNode = std::make_unique<MesonTargetNode>(
                    path.absolutePath().pathAppended(target.name),
                    Target::fullName(root->path(), target));
                targetNode->setDisplayName(target.name);
                asFolder->addNode(std::move(targetNode));
            }
            return true;
        }
        return false;
    });
}

static std::unique_ptr<MesonProjectNode> buildTree(const FilePath &srcDir,
                                                   const TargetsList &targets,
                                                   const FilePaths &bsFiles)
{
    std::set<FilePath> targetPaths;
    auto root = std::make_unique<MesonProjectNode>(srcDir);
    for (const Target &target : targets) {
        buildTargetTree(root, target);
        targetPaths.insert(FilePath::fromString(target.definedIn).absolutePath());
        addTargetNode(root, target);
    }
    for (FilePath bsFile : bsFiles) {
        if (!bsFile.toFileInfo().isAbsolute())
            bsFile = srcDir.pathAppended(bsFile.toString());
        root->addNestedNode(std::make_unique<FileNode>(bsFile, FileType::Project));
    }
    return root;
}

static std::optional<QString> extractValueIfMatches(const QString &arg,
                                                    const QStringList &candidates)
{
    for (const auto &flag : candidates) {
        if (arg.startsWith(flag))
            return arg.mid(flag.length());
    }
    return std::nullopt;
}

static std::optional<QString> extractInclude(const QString &arg)
{
    return extractValueIfMatches(arg, {"-I", "/I", "-isystem", "-imsvc", "/imsvc"});
}

static std::optional<Macro> extractMacro(const QString &arg)
{
    auto define = extractValueIfMatches(arg, {"-D", "/D"});
    if (define)
        return Macro::fromKeyValue(define->toLatin1());
    auto undef = extractValueIfMatches(arg, {"-U", "/U"});
    if (undef)
        return Macro(undef->toLatin1(), MacroType::Undefine);
    return std::nullopt;
}

static CompilerArgs splitArgs(const QStringList &args)
{
    CompilerArgs splited;
    for (const QString &arg : args) {
        auto inc = extractInclude(arg);
        if (inc) {
            splited.includePaths << *inc;
        } else {
            auto macro = extractMacro(arg);
            if (macro) {
                splited.macros << *macro;
            } else {
                splited.args << arg;
            }
        }
    }
    return splited;
}

static QStringList toAbsolutePath(const FilePath &refPath, QStringList &pathList)
{
    QStringList allAbs;
    std::transform(std::cbegin(pathList),
                   std::cend(pathList),
                   std::back_inserter(allAbs),
                   [refPath](const QString &path) {
                        return refPath.resolvePath(path).toString();
                   });
    return allAbs;
}

MesonProjectParser::MesonProjectParser(const Id &meson, const Environment &env, Project *project)
    : m_env{env}
    , m_meson{meson}
    , m_projectName{project->displayName()}
{
    // TODO re-think the way all BuildSystem/ProjectParser are tied
    // I take project info here, I also take build and src dir later from
    // functions args.
    auto fileFinder = new FileInProjectFinder;
    fileFinder->setProjectDirectory(project->projectDirectory());
    fileFinder->setProjectFiles(project->files(Project::AllFiles));
    m_outputParser.setFileFinder(fileFinder);
}

bool MesonProjectParser::configure(const FilePath &sourcePath,
                                   const FilePath &buildPath,
                                   const QStringList &args)
{
    m_introType = IntroDataType::file;
    m_srcDir = sourcePath;
    m_buildDir = buildPath;
    m_outputParser.setSourceDirectory(sourcePath);
    auto cmd = MesonTools::toolById(m_meson, ToolType::Meson)->configure(sourcePath, buildPath, args);
    cmd.environment = m_env;
    // see comment near m_pendingCommands declaration
    auto enqCmd = MesonTools::toolById(m_meson, ToolType::Meson)->regenerate(sourcePath, buildPath);
    enqCmd.environment = m_env;
    m_pendingCommands.enqueue(std::make_tuple(enqCmd, false));
    return run(cmd, m_projectName);
}

bool MesonProjectParser::wipe(const FilePath &sourcePath,
                              const FilePath &buildPath,
                              const QStringList &args)
{
    return setup(sourcePath, buildPath, args, true);
}

bool MesonProjectParser::setup(const FilePath &sourcePath,
                               const FilePath &buildPath,
                               const QStringList &args,
                               bool forceWipe)
{
    m_introType = IntroDataType::file;
    m_srcDir = sourcePath;
    m_buildDir = buildPath;
    m_outputParser.setSourceDirectory(sourcePath);
    auto cmdArgs = args;
    if (forceWipe || isSetup(buildPath))
        cmdArgs << "--wipe";
    auto cmd = MesonTools::toolById(m_meson, ToolType::Meson)->setup(sourcePath, buildPath, cmdArgs);
    cmd.environment = m_env;
    return run(cmd, m_projectName);
}

bool MesonProjectParser::parse(const FilePath &sourcePath, const FilePath &buildPath)
{
    m_srcDir = sourcePath;
    m_buildDir = buildPath;
    m_outputParser.setSourceDirectory(sourcePath);
    if (!isSetup(buildPath)) {
        return parse(sourcePath);
    } else {
        m_introType = IntroDataType::file;
        return startParser();
    }
}

bool MesonProjectParser::parse(const FilePath &sourcePath)
{
    m_srcDir = sourcePath;
    m_introType = IntroDataType::stdo;
    m_outputParser.setSourceDirectory(sourcePath);
    auto cmd = MesonTools::toolById(m_meson, ToolType::Meson)->introspect(sourcePath);
    cmd.environment = m_env;
    return run(cmd, m_projectName, true);
}

QList<BuildTargetInfo> MesonProjectParser::appsTargets() const
{
    QList<BuildTargetInfo> apps;
    for (const Target &target : m_parserResult.targets) {
        if (target.type == Target::Type::executable) {
            BuildTargetInfo bti;
            bti.displayName = target.name;
            bti.buildKey = Target::fullName(m_buildDir, target);
            bti.displayNameUniquifier = bti.buildKey;
            bti.targetFilePath = FilePath::fromString(target.fileName.first());
            bti.workingDirectory = FilePath::fromString(target.fileName.first()).absolutePath();
            bti.projectFilePath = FilePath::fromString(target.definedIn);
            bti.usesTerminal = true;
            apps.append(bti);
        }
    }
    return apps;
}

bool MesonProjectParser::startParser()
{
    m_parserFutureResult = Utils::asyncRun(ProjectExplorerPlugin::sharedThreadPool(),
        [processOutput = m_stdo, introType = m_introType, buildDir = m_buildDir, srcDir = m_srcDir] {
        if (introType == IntroDataType::file)
            return extractParserResults(srcDir, MesonInfoParser::parse(buildDir));
        else
            return extractParserResults(srcDir, MesonInfoParser::parse(processOutput));
    });
    Utils::onFinished(m_parserFutureResult, this, &MesonProjectParser::update);
    return true;
}

MesonProjectParser::ParserData *MesonProjectParser::extractParserResults(
    const FilePath &srcDir, MesonInfoParser::Result &&parserResult)
{
    auto rootNode = buildTree(srcDir, parserResult.targets, parserResult.buildSystemFiles);
    return new ParserData{std::move(parserResult), std::move(rootNode)};
}

static void addMissingTargets(QStringList &targetList)
{
    // Not all targets are listed in introspection data
    static const QString additionalTargets[] {
        Constants::Targets::all,
        Constants::Targets::clean,
        Constants::Targets::install,
        Constants::Targets::benchmark,
        Constants::Targets::scan_build
    };

    for (const QString &target : additionalTargets) {
        if (!targetList.contains(target))
            targetList.append(target);
    }
}

void MesonProjectParser::update(const QFuture<MesonProjectParser::ParserData *> &data)
{
    auto parserData = data.result();
    m_parserResult = std::move(parserData->data);
    m_rootNode = std::move(parserData->rootNode);
    m_targetsNames.clear();
    for (const Target &target : m_parserResult.targets) {
        m_targetsNames.push_back(Target::fullName(m_buildDir, target));
    }
    addMissingTargets(m_targetsNames);
    m_targetsNames.sort();
    delete parserData;
    emit parsingCompleted(true);
}

RawProjectPart MesonProjectParser::buildRawPart(
    const Target &target,
    const Target::SourceGroup &sources,
    const Toolchain *cxxToolchain,
    const Toolchain *cToolchain)
{
    RawProjectPart part;
    part.setDisplayName(target.name);
    part.setBuildSystemTarget(Target::fullName(m_buildDir, target));
    part.setFiles(sources.sources + sources.generatedSources);
    CompilerArgs flags = splitArgs(sources.parameters);
    part.setMacros(flags.macros);
    part.setIncludePaths(toAbsolutePath(m_buildDir, flags.includePaths));
    part.setProjectFileLocation(target.definedIn);
    if (sources.language == "cpp")
        part.setFlagsForCxx({cxxToolchain, flags.args, {}});
    else if (sources.language == "c")
        part.setFlagsForC({cToolchain, flags.args, {}});
    part.setQtVersion(m_qtVersion);
    return part;
}

RawProjectParts MesonProjectParser::buildProjectParts(
    const Toolchain *cxxToolchain, const Toolchain *cToolchain)
{
    RawProjectParts parts;
    for_each_source_group(m_parserResult.targets,
                          [&parts,
                           &cxxToolchain,
                           &cToolchain,
                           this](const Target &target, const Target::SourceGroup &sourceList) {
                              parts.push_back(
                                  buildRawPart(target, sourceList, cxxToolchain, cToolchain));
                          });
    return parts;
}

bool sourceGroupMatchesKit(const KitData &kit, const Target::SourceGroup &group)
{
    if (group.language == "c")
        return kit.cCompilerPath == group.compiler[0];
    if (group.language == "cpp")
        return kit.cxxCompilerPath == group.compiler[0];
    return true;
}

bool MesonProjectParser::matchesKit(const KitData &kit)
{
    bool matches = true;
    for_each_source_group(m_parserResult.targets,
                          [&matches, &kit](const Target &, const Target::SourceGroup &sourceGroup) {
                              matches = matches && sourceGroupMatchesKit(kit, sourceGroup);
                          });
    return matches;
}

static QVersionNumber versionNumber(const FilePath &buildDir)
{
    const FilePath jsonFile = buildDir / Constants::MESON_INFO_DIR / Constants::MESON_INFO;
    auto obj = load<QJsonObject>(jsonFile.toFSPathString());
    if (!obj)
        return {};
    auto version = obj->value("meson_version").toObject();
    return {version["major"].toInt(), version["minor"].toInt(), version["patch"].toInt()};
}

bool MesonProjectParser::usesSameMesonVersion(const FilePath &buildPath)
{
    auto version = versionNumber(buildPath);
    auto meson = MesonTools::toolById(m_meson, ToolType::Meson);
    return !version.isNull() && meson && version == meson->version();
}

bool MesonProjectParser::run(const ProcessRunData &runData, const QString &projectName,
                             bool captureStdo)
{
    if (!sanityCheck(runData))
        return false;
    m_stdo.clear();
    TaskHub::clearTasks(ProjectExplorer::Constants::TASK_CATEGORY_BUILDSYSTEM);
    setupProcess(runData, projectName, captureStdo);
    m_elapsed.start();
    m_process->start();
    qCDebug(mesonProcessLog()) << "Starting:" << runData.command.toUserOutput();
    return true;
}

void MesonProjectParser::handleProcessDone()
{
    if (m_process->result() != ProcessResult::FinishedWithSuccess)
        TaskHub::addTask(BuildSystemTask{Task::TaskType::Error, m_process->exitMessage()});

    m_stdo = m_process->readAllRawStandardOutput();
    m_stderr = m_process->readAllRawStandardError();
    const QString elapsedTime = formatElapsedTime(m_elapsed.elapsed());
    MessageManager::writeSilently(elapsedTime);

    if (m_process->exitCode() == 0 && m_process->exitStatus() == QProcess::NormalExit) {
        if (m_pendingCommands.isEmpty()) {
            startParser();
        } else {
            // see comment near m_pendingCommands declaration
            std::tuple<ProcessRunData, bool> args = m_pendingCommands.dequeue();
            run(std::get<0>(args), m_projectName, std::get<1>(args));
        }
    } else {
        if (m_introType == IntroDataType::stdo) {
            MessageManager::writeSilently(QString::fromLocal8Bit(m_stderr));
            m_outputParser.readStdo(m_stderr);
        }
        emit parsingCompleted(false);
    }
}

void MesonProjectParser::setupProcess(const ProcessRunData &runData, const QString &projectName,
                                      bool captureStdo)
{
    if (m_process)
        m_process.release()->deleteLater();
    m_process.reset(new Process);
    connect(m_process.get(), &Process::done, this, &MesonProjectParser::handleProcessDone);
    if (!captureStdo) {
        connect(m_process.get(), &Process::readyReadStandardOutput,
                this, &MesonProjectParser::processStandardOutput);
        connect(m_process.get(), &Process::readyReadStandardError,
                this, &MesonProjectParser::processStandardError);
    }

    MessageManager::writeFlashing(Tr::tr("Running %1 in %2.")
        .arg(runData.command.toUserOutput(), runData.workingDirectory.toUserOutput()));
    m_process->setRunData(runData);
    ProcessProgress *progress = new ProcessProgress(m_process.get());
    progress->setDisplayName(Tr::tr("Configuring \"%1\".").arg(projectName));
}

bool MesonProjectParser::sanityCheck(const ProcessRunData &runData) const
{
    const auto &exe = runData.command.executable();
    if (!exe.exists()) {
        //Should only reach this point if Meson exe is removed while a Meson project is opened
        TaskHub::addTask(
            BuildSystemTask{Task::TaskType::Error,
                            Tr::tr("Executable does not exist: %1").arg(exe.toUserOutput())});
        return false;
    }
    if (!exe.toFileInfo().isExecutable()) {
        TaskHub::addTask(
            BuildSystemTask{Task::TaskType::Error,
                            Tr::tr("Command is not executable: %1").arg(exe.toUserOutput())});
        return false;
    }
    return true;
}

void MesonProjectParser::processStandardOutput()
{
    const auto data = m_process->readAllRawStandardOutput();
    MessageManager::writeSilently(QString::fromLocal8Bit(data));
    m_outputParser.readStdo(data);
}

void MesonProjectParser::processStandardError()
{
    MessageManager::writeSilently(QString::fromLocal8Bit(m_process->readAllRawStandardError()));
}

} // namespace MesonProjectManager::Internal

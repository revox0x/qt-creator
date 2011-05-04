/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "debugginghelperbuildtask.h"
#include "qmldumptool.h"
#include "qmlobservertool.h"
#include "qmldebugginglibrary.h"
#include <qt4projectmanager/baseqtversion.h>
#include <qt4projectmanager/qt4projectmanagerconstants.h>
#include <projectexplorer/toolchainmanager.h>
#include <projectexplorer/debugginghelper.h>
#include <projectexplorer/abi.h>
#include <utils/qtcassert.h>

#include <QtCore/QCoreApplication>

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;
using ProjectExplorer::DebuggingHelperLibrary;

DebuggingHelperBuildTask::DebuggingHelperBuildTask(const BaseQtVersion *version, Tools tools) :
    m_tools(tools & availableTools(version))
{
    if (!version || !version->isValid())
        return;
    // allow type to be used in queued connections.
    qRegisterMetaType<DebuggingHelperBuildTask::Tools>("DebuggingHelperBuildTask::Tools");

    //
    // Extract all information we need from version, such that we don't depend on the existence
    // of the version pointer while compiling
    //
    m_qtId = version->uniqueId();
    m_qtInstallData = version->versionInfo().value("QT_INSTALL_DATA");
    if (m_qtInstallData.isEmpty()) {
        m_errorMessage
                = QCoreApplication::translate(
                    "QtVersion",
                    "Cannot determine the installation path for Qt version '%1'."
                    ).arg(version->displayName());
        return;
    }

    m_environment = Utils::Environment::systemEnvironment();
    version->addToEnvironment(m_environment);

    // TODO: the debugging helper doesn't comply to actual tool chain yet
    QList<ProjectExplorer::ToolChain *> tcList = ProjectExplorer::ToolChainManager::instance()->findToolChains(version->qtAbis().at(0));
    if (tcList.isEmpty()) {
        m_errorMessage =
                QCoreApplication::translate(
                    "QtVersion",
                    "The Qt Version has no tool chain.");
        return;
    }
    ProjectExplorer::ToolChain *tc = tcList.at(0);
    tc->addToEnvironment(m_environment);

    if (tc->targetAbi().os() == ProjectExplorer::Abi::LinuxOS
        && ProjectExplorer::Abi::hostAbi().os() == ProjectExplorer::Abi::WindowsOS)
        m_target = QLatin1String("-unix");
    m_qmakeCommand = version->qmakeCommand();
    m_makeCommand = tc->makeCommand();
    m_mkspec = version->mkspec();
}

DebuggingHelperBuildTask::~DebuggingHelperBuildTask()
{
}

DebuggingHelperBuildTask::Tools DebuggingHelperBuildTask::availableTools(const BaseQtVersion *version)
{
    QTC_ASSERT(version, return 0; )
    // Check the build requirements of the tools
    DebuggingHelperBuildTask::Tools tools = 0;
    // Gdb helpers are needed on Mac/gdb only.
    foreach (const ProjectExplorer::Abi &abi, version->qtAbis()) {
        if (abi.os() == ProjectExplorer::Abi::MacOS) {
            tools |= DebuggingHelperBuildTask::GdbDebugging;
            break;
        }
    }
    if (QmlDumpTool::canBuild(version))
        tools |= QmlDump;
    if (QmlDebuggingLibrary::canBuild(version)) {
        tools |= QmlDebugging;
        if (QmlObserverTool::canBuild(version))
            tools |= QmlObserver; // requires QML debugging.
    }
    return tools;
}

void DebuggingHelperBuildTask::run(QFutureInterface<void> &future)
{
    future.setProgressRange(0, 5);
    future.setProgressValue(1);

    QString output;

    bool success = false;
    if (m_errorMessage.isEmpty()) // might be already set in constructor
        success = buildDebuggingHelper(future, &output);

    if (success) {
        emit finished(m_qtId, output, m_tools);
    } else {
        qWarning("%s", qPrintable(m_errorMessage));
        emit finished(m_qtId, m_errorMessage, m_tools);
    }

    deleteLater();
}

bool DebuggingHelperBuildTask::buildDebuggingHelper(QFutureInterface<void> &future, QString *output)
{
    Utils::BuildableHelperLibrary::BuildHelperArguments arguments;
    arguments.makeCommand = m_makeCommand;
    arguments.qmakeCommand = m_qmakeCommand;
    arguments.targetMode = m_target;
    arguments.mkspec = m_mkspec;
    arguments.environment = m_environment;

    if (m_tools & GdbDebugging) {
        arguments.directory = DebuggingHelperLibrary::copy(m_qtInstallData, &m_errorMessage);
        if (arguments.directory.isEmpty())
            return false;

        if (!DebuggingHelperLibrary::build(arguments, output, &m_errorMessage))
            return false;
    }
    future.setProgressValue(2);

    if (m_tools & QmlDump) {
        arguments.directory = QmlDumpTool::copy(m_qtInstallData, &m_errorMessage);
        if (arguments.directory.isEmpty())
            return false;
        if (!QmlDumpTool::build(arguments, output, &m_errorMessage))
            return false;
    }
    future.setProgressValue(3);

    QString qmlDebuggingDirectory;
    if (m_tools & QmlDebugging) {
        qmlDebuggingDirectory = QmlDebuggingLibrary::copy(m_qtInstallData, &m_errorMessage);
        if (qmlDebuggingDirectory.isEmpty())
            return false;
        arguments.directory = qmlDebuggingDirectory;
        arguments.makeArguments += QLatin1String("all"); // build debug and release
        if (!QmlDebuggingLibrary::build(arguments, output, &m_errorMessage))
            return false;
        arguments.makeArguments.clear();
    }
    future.setProgressValue(4);

    if (m_tools & QmlObserver) {
        arguments.directory = QmlObserverTool::copy(m_qtInstallData, &m_errorMessage);
        if (arguments.directory.isEmpty())
            return false;

        arguments.qmakeArguments << QLatin1String("INCLUDEPATH+=\"\\\"") + qmlDebuggingDirectory + "include\\\"\"";
        arguments.qmakeArguments << QLatin1String("LIBS+=-L\"\\\"") + qmlDebuggingDirectory + QLatin1String("\\\"\"");

        if (!QmlObserverTool::build(arguments, output, &m_errorMessage))
            return false;
    }
    future.setProgressValue(5);
    return true;
}

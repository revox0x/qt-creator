// Copyright (C) 2016 Petar Perisin <petar.perisin@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "unstartedappwatcherdialog.h"

#include "debuggeritem.h"
#include "debuggerkitaspect.h"
#include "debuggertr.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/kitchooser.h>
#include <projectexplorer/kitaspects.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projecttree.h>
#include <projectexplorer/runconfiguration.h>
#include <projectexplorer/target.h>

#include <utils/fileutils.h>
#include <utils/pathchooser.h>
#include <utils/processinterface.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace ProjectExplorer;
using namespace Utils;

namespace Debugger::Internal {

static bool isLocal(RunConfiguration *runConfiguration)
{
    Target *target = runConfiguration ? runConfiguration->target() : nullptr;
    Kit *kit = target ? target->kit() : nullptr;
    return DeviceTypeKitAspect::deviceTypeId(kit) == ProjectExplorer::Constants::DESKTOP_DEVICE_TYPE;
}

/*!
    \class Debugger::Internal::UnstartedAppWatcherDialog

    \brief The UnstartedAppWatcherDialog class provides ability to wait for a certain application
           to be started, after what it will attach to it.

    This dialog can be useful in cases where automated scripts are used in order to execute some
    tests on application. In those cases application will be started from a script. This dialog
    allows user to attach to application in those cases in very short time after they are started.

    In order to attach, user needs to provide appropriate kit (for local debugging) and
    application path.

    After selecting start, dialog will check if selected application is started every
    10 miliseconds. As soon as application is started, QtCreator will attach to it.

    After user attaches, it is possible to keep dialog active and as soon as debugging
    session ends, it will start watching again. This is because sometimes automated test
    scripts can restart application several times during tests.
*/

UnstartedAppWatcherDialog::UnstartedAppWatcherDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(Tr::tr("Attach to Process Not Yet Started"));

    m_kitChooser = new KitChooser(this);
    m_kitChooser->setKitPredicate([](const Kit *k) {
        return ToolchainKitAspect::targetAbi(k).os() == Abi::hostAbi().os();
    });
    m_kitChooser->setShowIcons(true);
    m_kitChooser->populate();
    m_kitChooser->setVisible(true);

    Project *project = ProjectTree::currentProject();
    Target *activeTarget = project ? project->activeTarget() : nullptr;
    Kit *kit = activeTarget ? activeTarget->kit() : nullptr;

    if (kit)
        m_kitChooser->setCurrentKitId(kit->id());
    else if (KitManager::waitForLoaded() && KitManager::defaultKit())
        m_kitChooser->setCurrentKitId(KitManager::defaultKit()->id());

    auto pathLayout = new QHBoxLayout;
    m_pathChooser = new Utils::PathChooser(this);
    m_pathChooser->setExpectedKind(Utils::PathChooser::ExistingCommand);
    m_pathChooser->setHistoryCompleter("LocalExecutable", true);
    m_pathChooser->setMinimumWidth(400);

    auto resetExecutable = new QPushButton(Tr::tr("Reset"));
    resetExecutable->setEnabled(false);
    pathLayout->addWidget(m_pathChooser);
    pathLayout->addWidget(resetExecutable);
    if (activeTarget) {
        if (RunConfiguration *runConfig = activeTarget->activeRunConfiguration()) {
            const ProcessRunData runnable = runConfig->runnable();
            if (isLocal(runConfig)) {
                resetExecutable->setEnabled(true);
                connect(resetExecutable, &QPushButton::clicked, this, [this, runnable] {
                    m_pathChooser->setFilePath(runnable.command.executable());
                });
            }
        }
    }

    m_hideOnAttachCheckBox = new QCheckBox(Tr::tr("Reopen dialog when application finishes"), this);
    m_hideOnAttachCheckBox->setToolTip(Tr::tr("Reopens this dialog when application finishes."));

    m_hideOnAttachCheckBox->setChecked(false);
    m_hideOnAttachCheckBox->setVisible(true);

    m_continueOnAttachCheckBox = new QCheckBox(Tr::tr("Continue on attach"), this);
    m_continueOnAttachCheckBox->setToolTip(Tr::tr("Debugger does not stop the"
                                              " application after attach."));

    m_continueOnAttachCheckBox->setChecked(true);
    m_continueOnAttachCheckBox->setVisible(true);

    m_waitingLabel = new QLabel(QString(), this);
    m_waitingLabel->setAlignment(Qt::AlignCenter);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_watchingPushButton = buttonBox->addButton(Tr::tr("Start Watching"), QDialogButtonBox::ActionRole);
    m_watchingPushButton->setCheckable(true);
    m_watchingPushButton->setChecked(false);
    m_watchingPushButton->setEnabled(false);
    m_watchingPushButton->setDefault(true);

    auto mainLayout = new QFormLayout(this);
    mainLayout->addRow(new QLabel(Tr::tr("Kit: "), this), m_kitChooser);
    mainLayout->addRow(new QLabel(Tr::tr("Executable: "), this), pathLayout);
    mainLayout->addRow(m_hideOnAttachCheckBox);
    mainLayout->addRow(m_continueOnAttachCheckBox);
    mainLayout->addRow(m_waitingLabel);
    mainLayout->addItem(new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    mainLayout->addRow(buttonBox);
    setLayout(mainLayout);

    connect(m_pathChooser, &Utils::PathChooser::beforeBrowsing,
            this, &UnstartedAppWatcherDialog::selectExecutable);
    connect(m_watchingPushButton, &QAbstractButton::toggled,
            this, &UnstartedAppWatcherDialog::startStopWatching);
    connect(m_pathChooser, &Utils::PathChooser::textChanged,
            this, &UnstartedAppWatcherDialog::stopAndCheckExecutable);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(&m_timer, &QTimer::timeout,
            this, &UnstartedAppWatcherDialog::findProcess);
    connect(m_kitChooser, &KitChooser::currentIndexChanged,
            this, &UnstartedAppWatcherDialog::kitChanged);
    kitChanged();
    m_pathChooser->setFocus();

    setWaitingState(checkExecutableString() ? NotWatchingState : InvalidWacherState);
}

bool UnstartedAppWatcherDialog::event(QEvent *e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        auto ke = static_cast<QKeyEvent *>(e);
        if (ke->key() == Qt::Key_Escape && !ke->modifiers()) {
            ke->accept();
            return true;
        }
    }
    return QDialog::event(e);
}

void UnstartedAppWatcherDialog::selectExecutable()
{
    Utils::FilePath path;

    Project *project = ProjectTree::currentProject();
    Target *activeTarget = project ? project->activeTarget() : nullptr;

    if (activeTarget) {
        if (RunConfiguration *runConfig = activeTarget->activeRunConfiguration()) {
            const ProcessRunData runnable = runConfig->runnable();
            if (isLocal(runConfig))
                path = runnable.command.executable().parentDir();
        }
    }

    if (path.isEmpty()) {
        if (activeTarget && activeTarget->activeBuildConfiguration())
            path = activeTarget->activeBuildConfiguration()->buildDirectory();
        else if (project)
            path = project->projectDirectory();
    }
    m_pathChooser->setInitialBrowsePathBackup(path);
}

void UnstartedAppWatcherDialog::startWatching()
{
    show();
    if (checkExecutableString()) {
        setWaitingState(WatchingState);
        startStopTimer(true);
    } else {
        setWaitingState(InvalidWacherState);
    }
}

void UnstartedAppWatcherDialog::pidFound(const ProcessInfo &p)
{
    setWaitingState(FoundState);
    startStopTimer(false);
    m_process = p;

    if (hideOnAttach())
        hide();
    else
        accept();

    emit processFound();
}

void UnstartedAppWatcherDialog::startStopWatching(bool start)
{
    setWaitingState(start ? WatchingState : NotWatchingState);
    m_watchingPushButton->setText(start ? Tr::tr("Stop Watching") : Tr::tr("Start Watching"));
    startStopTimer(start);
}

void UnstartedAppWatcherDialog::startStopTimer(bool start)
{
    if (start)
        m_timer.start(10);
    else
        m_timer.stop();
}

void UnstartedAppWatcherDialog::findProcess()
{
    const QString &appName = m_pathChooser->filePath().normalizedPathName().toString();
    ProcessInfo fallback;
    const QList<ProcessInfo> processInfoList = ProcessInfo::processInfoList();
    for (const ProcessInfo &processInfo : processInfoList) {
        if (m_excluded.contains(processInfo.processId))
            continue;
        if (Utils::FileUtils::normalizedPathName(processInfo.executable) == appName) {
            pidFound(processInfo);
            return;
        }
        if (processInfo.commandLine.startsWith(appName))
            fallback = processInfo;
    }
    if (fallback.processId != 0)
        pidFound(fallback);
}

void UnstartedAppWatcherDialog::stopAndCheckExecutable()
{
    startStopTimer(false);
    setWaitingState(checkExecutableString() ? NotWatchingState : InvalidWacherState);
}

void UnstartedAppWatcherDialog::kitChanged()
{
    const DebuggerItem *debugger = DebuggerKitAspect::debugger(m_kitChooser->currentKit());
    if (!debugger)
        return;
    if (debugger->engineType() == Debugger::CdbEngineType) {
        m_continueOnAttachCheckBox->setEnabled(false);
        m_continueOnAttachCheckBox->setChecked(true);
    } else {
        m_continueOnAttachCheckBox->setEnabled(true);
    }
}

bool UnstartedAppWatcherDialog::checkExecutableString() const
{
    if (!m_pathChooser->filePath().toString().isEmpty()) {
        QFileInfo fileInfo(m_pathChooser->filePath().toString());
        return (fileInfo.exists() && fileInfo.isFile());
    }
    return false;
}

Kit *UnstartedAppWatcherDialog::currentKit() const
{
    return m_kitChooser->currentKit();
}

ProcessInfo UnstartedAppWatcherDialog::currentProcess() const
{
    return m_process;
}

bool UnstartedAppWatcherDialog::hideOnAttach() const
{
    return m_hideOnAttachCheckBox->isChecked();
}

bool UnstartedAppWatcherDialog::continueOnAttach() const
{
    return m_continueOnAttachCheckBox->isEnabled() && m_continueOnAttachCheckBox->isChecked();
}

void UnstartedAppWatcherDialog::setWaitingState(UnstartedAppWacherState state)
{
    switch (state) {
    case InvalidWacherState:
        m_waitingLabel->setText(Tr::tr("Select valid executable."));
        m_watchingPushButton->setEnabled(false);
        m_watchingPushButton->setChecked(false);
        m_pathChooser->setEnabled(true);
        m_kitChooser->setEnabled(true);
        break;

    case NotWatchingState:
        m_waitingLabel->setText(Tr::tr("Not watching."));
        m_watchingPushButton->setEnabled(true);
        m_watchingPushButton->setChecked(false);
        m_pathChooser->setEnabled(true);
        m_kitChooser->setEnabled(true);
        break;

    case WatchingState: {
        m_waitingLabel->setText(Tr::tr("Waiting for process to start..."));
        m_watchingPushButton->setEnabled(true);
        m_watchingPushButton->setChecked(true);
        m_pathChooser->setEnabled(false);
        m_kitChooser->setEnabled(false);
        m_excluded.clear();
        const QList<ProcessInfo> processInfoList = ProcessInfo::processInfoList();
        for (const ProcessInfo &processInfo : processInfoList)
            m_excluded.insert(processInfo.processId);
        break;
    }

    case FoundState:
        m_waitingLabel->setText(Tr::tr("Attach"));
        m_watchingPushButton->setEnabled(false);
        m_watchingPushButton->setChecked(true);
        m_pathChooser->setEnabled(false);
        m_kitChooser->setEnabled(true);
        break;
    }
}

} // Debugger::Internal

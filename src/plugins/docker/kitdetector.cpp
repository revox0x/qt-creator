// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "kitdetector.h"

#include <cmakeprojectmanager/cmakeprojectconstants.h>

#include <extensionsystem/pluginmanager.h>

#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/projectexplorertr.h>
#include <projectexplorer/toolchain.h>
#include <projectexplorer/toolchainmanager.h>

#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtkitaspect.h>
#include <qtsupport/qtversionfactory.h>
#include <qtsupport/qtversionmanager.h>

#include <utils/filepath.h>
#include <utils/qtcassert.h>
#include <utils/algorithm.h>

#include <QApplication>

using namespace ProjectExplorer;
using namespace QtSupport;
using namespace Utils;

namespace Docker::Internal {

class KitDetectorPrivate
{
public:
    KitDetectorPrivate(KitDetector *parent, const IDevice::ConstPtr &device)
        : q(parent)
        , m_device(device)
    {}

    void autoDetect();
    void undoAutoDetect() const;
    void listAutoDetected() const;

    void setSharedId(const QString &sharedId) { m_sharedId = sharedId; }
    void setSearchPaths(const FilePaths &searchPaths) { m_searchPaths = searchPaths; }

private:
    QtVersions autoDetectQtVersions() const;
    QList<Toolchain *> autoDetectToolchains();
    void autoDetectPython();
    QList<Id> autoDetectCMake();
    void autoDetectDebugger();

    KitDetector *q;
    IDevice::ConstPtr m_device;
    QString m_sharedId;
    FilePaths m_searchPaths;
};

KitDetector::KitDetector(const IDevice::ConstPtr &device)
    : d(new KitDetectorPrivate(this, device))
{}

KitDetector::~KitDetector()
{
    delete d;
}

void KitDetector::autoDetect(const QString &sharedId, const FilePaths &searchPaths) const
{
    d->setSharedId(sharedId);
    d->setSearchPaths(searchPaths);
    d->autoDetect();
}

void KitDetector::undoAutoDetect(const QString &sharedId) const
{
    d->setSharedId(sharedId);
    d->undoAutoDetect();
}

void KitDetector::listAutoDetected(const QString &sharedId) const
{
    d->setSharedId(sharedId);
    d->listAutoDetected();
}

void KitDetectorPrivate::undoAutoDetect() const
{
    emit q->logOutput(ProjectExplorer::Tr::tr("Start removing auto-detected items associated with this docker image."));

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Removing kits..."));
    for (Kit *kit : KitManager::kits()) {
        if (kit->autoDetectionSource() == m_sharedId) {
            emit q->logOutput(ProjectExplorer::Tr::tr("Removed \"%1\"").arg(kit->displayName()));
            KitManager::deregisterKit(kit);
        }
    };

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Removing Qt version entries..."));
    for (QtVersion *qtVersion : QtVersionManager::versions()) {
        if (qtVersion->detectionSource() == m_sharedId) {
            emit q->logOutput(ProjectExplorer::Tr::tr("Removed \"%1\"").arg(qtVersion->displayName()));
            QtVersionManager::removeVersion(qtVersion);
        }
    };

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Removing toolchain entries..."));
    const Toolchains toDeregister = Utils::filtered(
        ToolchainManager::toolchains(), Utils::equal(&Toolchain::detectionSource, m_sharedId));
    for (Toolchain * toolchain : toDeregister)
        emit q->logOutput(ProjectExplorer::Tr::tr("Removed \"%1\"").arg(toolchain->displayName()));
    ToolchainManager::deregisterToolchains(toDeregister);

    if (auto cmakeManager = ExtensionSystem::PluginManager::getObjectByName("CMakeToolManager")) {
        QString logMessage;
        const bool res = QMetaObject::invokeMethod(cmakeManager,
                                                   "removeDetectedCMake",
                                                   Q_ARG(QString, m_sharedId),
                                                   Q_ARG(QString *, &logMessage));
        QTC_CHECK(res);
        emit q->logOutput('\n' + logMessage);
    }

    if (auto debuggerPlugin = ExtensionSystem::PluginManager::getObjectByName("DebuggerPlugin")) {
        QString logMessage;
        const bool res = QMetaObject::invokeMethod(debuggerPlugin,
                                                   "removeDetectedDebuggers",
                                                   Q_ARG(QString, m_sharedId),
                                                   Q_ARG(QString *, &logMessage));
        QTC_CHECK(res);
        emit q->logOutput('\n' + logMessage);
    }

    if (auto pythonSettings = ExtensionSystem::PluginManager::getObjectByName("PythonSettings")) {
        QString logMessage;
        const bool res = QMetaObject::invokeMethod(pythonSettings,
                                                   "removeDetectedPython",
                                                   Q_ARG(QString, m_sharedId),
                                                   Q_ARG(QString *, &logMessage));
        QTC_CHECK(res);
        emit q->logOutput('\n' + logMessage);
    }

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Removal of previously auto-detected kit items finished.") + "\n\n");
}

void KitDetectorPrivate::listAutoDetected() const
{
    emit q->logOutput(ProjectExplorer::Tr::tr("Start listing auto-detected items associated with this docker image."));

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Kits:"));
    for (Kit *kit : KitManager::kits()) {
        if (kit->autoDetectionSource() == m_sharedId)
            emit q->logOutput(kit->displayName());
    }

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Qt versions:"));
    for (QtVersion *qtVersion : QtVersionManager::versions()) {
        if (qtVersion->detectionSource() == m_sharedId)
            emit q->logOutput(qtVersion->displayName());
    }

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Toolchains:"));
    for (Toolchain *toolchain : ToolchainManager::toolchains()) {
        if (toolchain->detectionSource() == m_sharedId)
            emit q->logOutput(toolchain->displayName());
    }

    if (QObject *cmakeManager = ExtensionSystem::PluginManager::getObjectByName(
            "CMakeToolManager")) {
        QString logMessage;
        const bool res = QMetaObject::invokeMethod(cmakeManager,
                                                   "listDetectedCMake",
                                                   Q_ARG(QString, m_sharedId),
                                                   Q_ARG(QString *, &logMessage));
        QTC_CHECK(res);
        emit q->logOutput('\n' + logMessage);
    }

    if (QObject *debuggerPlugin = ExtensionSystem::PluginManager::getObjectByName(
            "DebuggerPlugin")) {
        QString logMessage;
        const bool res = QMetaObject::invokeMethod(debuggerPlugin,
                                                   "listDetectedDebuggers",
                                                   Q_ARG(QString, m_sharedId),
                                                   Q_ARG(QString *, &logMessage));
        QTC_CHECK(res);
        emit q->logOutput('\n' + logMessage);
    }

    if (auto pythonSettings = ExtensionSystem::PluginManager::getObjectByName("PythonSettings")) {
        QString logMessage;
        const bool res = QMetaObject::invokeMethod(pythonSettings,
                                                   "listDetectedPython",
                                                   Q_ARG(QString, m_sharedId),
                                                   Q_ARG(QString *, &logMessage));
        QTC_CHECK(res);
        emit q->logOutput('\n' + logMessage);
    }

    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Listing of previously auto-detected kit items finished.") + "\n\n");
}

QtVersions KitDetectorPrivate::autoDetectQtVersions() const
{
    QtVersions qtVersions;

    QString error;

    const auto handleQmake = [this, &qtVersions, &error](const FilePath &qmake) {
        if (QtVersion *qtVersion = QtVersionFactory::createQtVersionFromQMakePath(qmake,
                                                                            false,
                                                                            m_sharedId,
                                                                            &error)) {
            if (qtVersion->isValid()) {
                if (!Utils::anyOf(qtVersions,
                                 [qtVersion](QtVersion* other) {
                                     return qtVersion->mkspecPath() == other->mkspecPath();
                                 })) {
                    qtVersions.append(qtVersion);
                    QtVersionManager::addVersion(qtVersion);
                    emit q->logOutput(
                        ProjectExplorer::Tr::tr("Found \"%1\"").arg(qtVersion->qmakeFilePath().toUserOutput()));
                }
            }
        }
        return IterationPolicy::Continue;
    };

    emit q->logOutput(ProjectExplorer::Tr::tr("Searching for qmake executables..."));

    const QStringList candidates = {"qmake6", "qmake-qt6", "qmake-qt5", "qmake"};
    for (const FilePath &searchPath : m_searchPaths) {
        searchPath.iterateDirectory(handleQmake,
                                    {candidates,
                                     QDir::Files | QDir::Executable,
                                     QDirIterator::Subdirectories});
    }

    if (!error.isEmpty())
        emit q->logOutput(ProjectExplorer::Tr::tr("Error: %1.").arg(error));
    if (qtVersions.isEmpty())
        emit q->logOutput(ProjectExplorer::Tr::tr("No Qt installation found."));
    return qtVersions;
}

Toolchains KitDetectorPrivate::autoDetectToolchains()
{
    const QList<ToolchainFactory *> factories = ToolchainFactory::allToolchainFactories();

    Toolchains alreadyKnown = ToolchainManager::toolchains();
    Toolchains allNewToolchains;
    QApplication::processEvents();
    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Searching toolchains..."));
    for (ToolchainFactory *factory : factories) {
        emit q->logOutput(ProjectExplorer::Tr::tr("Searching toolchains of type %1").arg(factory->displayName()));
        const ToolchainDetector detector(alreadyKnown, m_device, m_searchPaths);
        const Toolchains newToolchains = factory->autoDetect(detector);
        for (Toolchain *toolchain : newToolchains) {
            emit q->logOutput(ProjectExplorer::Tr::tr("Found \"%1\"")
                                  .arg(toolchain->compilerCommand().toUserOutput()));
            toolchain->setDetectionSource(m_sharedId);
        }
        const QList<ToolchainBundle> bundles
            = ToolchainBundle::collectBundles(newToolchains, ToolchainBundle::AutoRegister::On);
        alreadyKnown.append(newToolchains);
        allNewToolchains.append(newToolchains);
    }
    emit q->logOutput(ProjectExplorer::Tr::tr("%1 new toolchains found.").arg(allNewToolchains.size()));

    return allNewToolchains;
}

void KitDetectorPrivate::autoDetectPython()
{
    QObject *pythonSettings = ExtensionSystem::PluginManager::getObjectByName("PythonSettings");
    if (!pythonSettings)
        return;

    QString logMessage;
    const bool res = QMetaObject::invokeMethod(pythonSettings,
                                               "detectPythonOnDevice",
                                               Q_ARG(Utils::FilePaths, m_searchPaths),
                                               Q_ARG(QString, m_device->displayName()),
                                               Q_ARG(QString, m_sharedId),
                                               Q_ARG(QString *, &logMessage));
    QTC_CHECK(res);
    emit q->logOutput('\n' + logMessage);
}

QList<Id> KitDetectorPrivate::autoDetectCMake()
{
    QList<Id> result;
    QObject *cmakeManager = ExtensionSystem::PluginManager::getObjectByName("CMakeToolManager");
    if (!cmakeManager)
        return {};

    QString logMessage;
    const bool res = QMetaObject::invokeMethod(cmakeManager,
                                               "autoDetectCMakeForDevice",
                                               Q_RETURN_ARG(QList<Utils::Id>, result),
                                               Q_ARG(Utils::FilePaths, m_searchPaths),
                                               Q_ARG(QString, m_sharedId),
                                               Q_ARG(QString *, &logMessage));
    QTC_CHECK(res);
    emit q->logOutput('\n' + logMessage);

    return result;
}

void KitDetectorPrivate::autoDetectDebugger()
{
    QObject *debuggerPlugin = ExtensionSystem::PluginManager::getObjectByName("DebuggerPlugin");
    if (!debuggerPlugin)
        return;

    QString logMessage;
    const bool res = QMetaObject::invokeMethod(debuggerPlugin,
                                               "autoDetectDebuggersForDevice",
                                               Q_ARG(Utils::FilePaths, m_searchPaths),
                                               Q_ARG(QString, m_sharedId),
                                               Q_ARG(QString *, &logMessage));
    QTC_CHECK(res);
    emit q->logOutput('\n' + logMessage);
}

void KitDetectorPrivate::autoDetect()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    undoAutoDetect();

    emit q->logOutput(ProjectExplorer::Tr::tr("Starting auto-detection. This will take a while..."));

    autoDetectToolchains();
    const QtVersions qtVersions = autoDetectQtVersions();

    const QList<Id> cmakeIds = autoDetectCMake();
    const Id cmakeId = cmakeIds.empty() ? Id() : cmakeIds.first();
    autoDetectDebugger();
    autoDetectPython();

    const auto initializeKit = [this, qtVersions, cmakeId](Kit *k) {
        k->setAutoDetected(false);
        k->setAutoDetectionSource(m_sharedId);
        k->setUnexpandedDisplayName("%{Device:Name}");

        if (cmakeId.isValid())
            k->setValue(CMakeProjectManager::Constants::TOOL_ID, cmakeId.toSetting());

        DeviceTypeKitAspect::setDeviceTypeId(k, m_device->type());
        DeviceKitAspect::setDevice(k, m_device);
        BuildDeviceKitAspect::setDevice(k, m_device);

        const Toolchains toolchainCandidates = ToolchainManager::toolchains(
            [this](const Toolchain *tc) { return tc->detectionSource() == m_sharedId; });
        const QList<ToolchainBundle> bundles
            = ToolchainBundle::collectBundles(toolchainCandidates, ToolchainBundle::AutoRegister::On);

        // Try to find a matching Qt/Toolchain pair.
        bool match = false;
        for (const ToolchainBundle &bundle : bundles) {
            if (match)
                break;
            for (QtVersion * const qt : qtVersions) {
                if (Utils::contains(qt->qtAbis(), [&bundle](const Abi &abi) {
                        return bundle.targetAbi().isCompatibleWith(abi);
                    })) {
                    match = true;
                    ToolchainKitAspect::setBundle(k, bundle);
                    QtKitAspect::setQtVersion(k, qt);
                    break;
                }
            }
        }

        // Otherwise, just set a random toolchain.
        if (!match && !bundles.isEmpty())
            ToolchainKitAspect::setBundle(k, bundles.first());

        if (cmakeId.isValid())
            k->setSticky(CMakeProjectManager::Constants::TOOL_ID, true);

        k->setSticky(ToolchainKitAspect::id(), true);
        k->setSticky(QtSupport::QtKitAspect::id(), true);
        k->setSticky(DeviceKitAspect::id(), true);
        k->setSticky(DeviceTypeKitAspect::id(), true);
        k->setSticky(BuildDeviceKitAspect::id(), true);
    };

    Kit *kit = KitManager::registerKit(initializeKit);
    emit q->logOutput('\n' + ProjectExplorer::Tr::tr("Registered kit %1").arg(kit->displayName()));

    QApplication::restoreOverrideCursor();
}

} // Docker::Internal

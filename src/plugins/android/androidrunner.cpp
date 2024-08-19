// Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "androidrunner.h"

#include "androidavdmanager.h"
#include "androiddevice.h"
#include "androidmanager.h"
#include "androidrunnerworker.h"
#include "androidtr.h"

#include <projectexplorer/projectexplorersettings.h>
#include <projectexplorer/target.h>
#include <qtsupport/qtkitaspect.h>
#include <utils/url.h>

#include <QHostAddress>
#include <QLoggingCategory>

namespace {
static Q_LOGGING_CATEGORY(androidRunnerLog, "qtc.android.run.androidrunner", QtWarningMsg)
}

using namespace ProjectExplorer;
using namespace Tasking;
using namespace Utils;

namespace Android::Internal {

AndroidRunner::AndroidRunner(RunControl *runControl)
    : RunWorker(runControl)
    , m_target(runControl->target())
{
    setId("AndroidRunner");
    static const int metaTypes[] = {
        qRegisterMetaType<QList<QStringList>>("QList<QStringList>"),
        qRegisterMetaType<Utils::Port>("Utils::Port"),
        qRegisterMetaType<AndroidDeviceInfo>("Android::AndroidDeviceInfo")
    };
    Q_UNUSED(metaTypes)

    m_worker = new AndroidRunnerWorker(this);
    m_worker->moveToThread(&m_thread);
    QObject::connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(this, &AndroidRunner::asyncStart, m_worker, &AndroidRunnerWorker::asyncStart);
    connect(this, &AndroidRunner::asyncStop, m_worker, &AndroidRunnerWorker::asyncStop);
    connect(this, &AndroidRunner::androidDeviceInfoChanged,
            m_worker, &AndroidRunnerWorker::setAndroidDeviceInfo);

    connect(m_worker, &AndroidRunnerWorker::remoteProcessStarted,
            this, &AndroidRunner::handleRemoteProcessStarted);
    connect(m_worker, &AndroidRunnerWorker::remoteProcessFinished,
            this, &AndroidRunner::handleRemoteProcessFinished);
    connect(m_worker, &AndroidRunnerWorker::remoteOutput, this, &AndroidRunner::remoteOutput);
    connect(m_worker, &AndroidRunnerWorker::remoteErrorOutput,
            this, &AndroidRunner::remoteErrorOutput);

    connect(&m_outputParser, &QmlDebug::QmlOutputParser::waitingForConnectionOnPort,
            this, &AndroidRunner::qmlServerPortReady);

    m_thread.start();
}

AndroidRunner::~AndroidRunner()
{
    m_thread.quit();
    m_thread.wait();
}

void AndroidRunner::start()
{
    if (!projectExplorerSettings().deployBeforeRun && m_target && m_target->project()) {
        qCDebug(androidRunnerLog) << "Run without deployment";

        const IDevice::ConstPtr device = DeviceKitAspect::device(m_target->kit());
        AndroidDeviceInfo info = AndroidDevice::androidDeviceInfoFromIDevice(device.get());
        AndroidManager::setDeviceSerialNumber(m_target, info.serialNumber);
        emit androidDeviceInfoChanged(info);

        if (!info.avdName.isEmpty()) {
            const Storage<QString> serialNumberStorage;

            const Group recipe {
                serialNumberStorage,
                AndroidAvdManager::startAvdRecipe(info.avdName, serialNumberStorage)
            };

            m_startAvdRunner.start(recipe, {}, [this](DoneWith result) {
                if (result == DoneWith::Success)
                    emit asyncStart();
            });
            return;
        }
    }
    emit asyncStart();
}

void AndroidRunner::stop()
{
    if (m_startAvdRunner.isRunning()) {
        m_startAvdRunner.reset();
        appendMessage("\n\n" + Tr::tr("\"%1\" terminated.").arg(AndroidManager::packageName(m_target)),
                      Utils::NormalMessageFormat);
        return;
    }
    emit asyncStop();
}

void AndroidRunner::qmlServerPortReady(Port port)
{
    // FIXME: Note that the passed is nonsense, as the port is on the
    // device side. It only happens to work since we redirect
    // host port n to target port n via adb.
    QUrl serverUrl;
    serverUrl.setHost(QHostAddress(QHostAddress::LocalHost).toString());
    serverUrl.setPort(port.number());
    serverUrl.setScheme(urlTcpScheme());
    qCDebug(androidRunnerLog) << "Qml Server port ready"<< serverUrl;
    emit qmlServerReady(serverUrl);
}

void AndroidRunner::remoteOutput(const QString &output)
{
    appendMessage(output, Utils::StdOutFormat);
    m_outputParser.processOutput(output);
}

void AndroidRunner::remoteErrorOutput(const QString &output)
{
    appendMessage(output, Utils::StdErrFormat);
    m_outputParser.processOutput(output);
}

void AndroidRunner::handleRemoteProcessStarted(Utils::Port debugServerPort,
                                               const QUrl &qmlServer, qint64 pid)
{
    m_pid = ProcessHandle(pid);
    m_debugServerPort = debugServerPort;
    m_qmlServer = qmlServer;
    reportStarted();
}

void AndroidRunner::handleRemoteProcessFinished(const QString &errString)
{
    appendMessage(errString, Utils::NormalMessageFormat);
    if (runControl()->isRunning())
        runControl()->initiateStop();
    reportStopped();
}

} // namespace Android::Internal

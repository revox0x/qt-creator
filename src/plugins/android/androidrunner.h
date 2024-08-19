// Copyright (C) 2016 BogDan Vatra <bog_dan_ro@yahoo.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "androidconfigurations.h"

#include <projectexplorer/runcontrol.h>

#include <qmldebug/qmldebugcommandlinearguments.h>
#include <qmldebug/qmloutputparser.h>

#include <solutions/tasking/tasktreerunner.h>

#include <QThread>

namespace Android::Internal {

class AndroidRunnerWorker;

class AndroidRunner : public ProjectExplorer::RunWorker
{
    Q_OBJECT

public:
    explicit AndroidRunner(ProjectExplorer::RunControl *runControl);
    ~AndroidRunner() override;

    Utils::Port debugServerPort() const { return m_debugServerPort; } // GDB or LLDB
    QUrl qmlServer() const { return m_qmlServer; }
    Utils::ProcessHandle pid() const { return m_pid; }

    void start() override;
    void stop() override;

signals:
    void asyncStart();
    void asyncStop();
    void qmlServerReady(const QUrl &serverUrl);
    void androidDeviceInfoChanged(const Android::AndroidDeviceInfo &deviceInfo);
    void avdDetected();

private:
    void qmlServerPortReady(Utils::Port port);
    void remoteOutput(const QString &output);
    void remoteErrorOutput(const QString &output);
    void gotRemoteOutput(const QString &output);
    void handleRemoteProcessStarted(Utils::Port debugServerPort, const QUrl &qmlServer, qint64 pid);
    void handleRemoteProcessFinished(const QString &errString = QString());

    QThread m_thread;
    AndroidRunnerWorker *m_worker = nullptr;
    QPointer<ProjectExplorer::Target> m_target;
    Utils::Port m_debugServerPort;
    QUrl m_qmlServer;
    Utils::ProcessHandle m_pid;
    QmlDebug::QmlOutputParser m_outputParser;
    Tasking::TaskTreeRunner m_startAvdRunner;
};

} // namespace Android::Internal

#pragma once

#include "codeboosterclient.h"

#include <extensionsystem/iplugin.h>
#include <QPointer>

#include "askcodeboostertaskhandler.h"

namespace CodeBooster {
namespace Internal {

class CodeBoosterPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "CodeBooster.json")

public:
    static CodeBoosterPlugin *instance();

public:
    void initialize() override;
    void extensionsInitialized() override;
    void restartClient();
    ShutdownFlag aboutToShutdown() override;

private slots:
    void onHandleAskCodeBossterTask(const QString &sysMsg, const QString &userMsg);

signals:
    void documentSelectionChanged(const QString &fileName, const QString &text);
    void askCompileError(const QString &sysMsg, const QString &userMsg);



private:
    QPointer<CodeBoosterClient> m_client;
    AskCodeBoosterTaskHandler mAskCompileErrorHandler;
};

} // namespace Internal
} // namespace CodeBooster

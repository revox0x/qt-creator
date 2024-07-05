#pragma once

#include "codeboosterclient.h"

#include <extensionsystem/iplugin.h>

#include <QPointer>

namespace CodeBooster {
namespace Internal {

class CodeBoosterPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "CodeBooster.json")

public:
    void initialize() override;
    void extensionsInitialized() override;
    void restartClient();
    ShutdownFlag aboutToShutdown() override;

private:
    QPointer<CodeBoosterClient> m_client;
};

} // namespace Internal
} // namespace CodeBooster

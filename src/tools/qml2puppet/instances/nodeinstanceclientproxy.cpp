// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "nodeinstanceclientproxy.h"

#include <QLocalSocket>
#include <QVariant>
#include <QCoreApplication>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>

#include "nodeinstanceserverinterface.h"

#include "captureddatacommand.h"
#include "changeauxiliarycommand.h"
#include "changebindingscommand.h"
#include "changefileurlcommand.h"
#include "changeidscommand.h"
#include "changelanguagecommand.h"
#include "changenodesourcecommand.h"
#include "changepreviewimagesizecommand.h"
#include "changeselectioncommand.h"
#include "changestatecommand.h"
#include "changevaluescommand.h"
#include "childrenchangedcommand.h"
#include "clearscenecommand.h"
#include "completecomponentcommand.h"
#include "componentcompletedcommand.h"
#include "createinstancescommand.h"
#include "createscenecommand.h"
#include "debugoutputcommand.h"
#include "endpuppetcommand.h"
#include "imagecontainer.h"
#include "informationchangedcommand.h"
#include "inputeventcommand.h"
#include "instancecontainer.h"
#include "pixmapchangedcommand.h"
#include "propertyabstractcontainer.h"
#include "propertybindingcontainer.h"
#include "propertyvaluecontainer.h"
#include "puppetalivecommand.h"
#include "puppettocreatorcommand.h"
#include "removeinstancescommand.h"
#include "removepropertiescommand.h"
#include "removesharedmemorycommand.h"
#include "reparentinstancescommand.h"
#include "scenecreatedcommand.h"
#include "statepreviewimagechangedcommand.h"
#include "synchronizecommand.h"
#include "tokencommand.h"
#include "update3dviewstatecommand.h"
#include "valueschangedcommand.h"
#include "view3dactioncommand.h"
#include "requestmodelnodepreviewimagecommand.h"
#include "nanotracecommand.h"

// Nanotrace headers are not exported to build dir at all if the feature is disabled, so
// runtime puppet build can't find them.
#if NANOTRACE_DESIGNSTUDIO_ENABLED
#include "nanotrace/nanotrace.h"
#else
#define NANOTRACE_INIT(process, thread, filepath)
#define NANOTRACE_SHUTDOWN()
#define NANOTRACE_SCOPE_ARGS(cat, name, ...)
#endif

namespace QmlDesigner {

void (QLocalSocket::*LocalSocketErrorFunction)(QLocalSocket::LocalSocketError)
    = &QLocalSocket::errorOccurred;

NodeInstanceClientProxy::NodeInstanceClientProxy(QObject *parent)
    : QObject(parent)
    , m_inputIoDevice(nullptr)
    , m_outputIoDevice(nullptr)
    , m_localSocket(nullptr)
    , m_writeCommandCounter(0)
    , m_synchronizeId(-1)
{
    connect(&m_puppetAliveTimer, &QTimer::timeout, this, &NodeInstanceClientProxy::sendPuppetAliveCommand);
    m_puppetAliveTimer.setInterval(2000);
    m_puppetAliveTimer.start();
}

NodeInstanceClientProxy::~NodeInstanceClientProxy() = default;

void NodeInstanceClientProxy::initializeSocket()
{
    QLocalSocket *localSocket = new QLocalSocket(this);
    connect(localSocket, &QIODevice::readyRead, this, &NodeInstanceClientProxy::readDataStream);
    connect(localSocket, LocalSocketErrorFunction,
            QCoreApplication::instance(), &QCoreApplication::quit);
    connect(localSocket, &QLocalSocket::disconnected, QCoreApplication::instance(), &QCoreApplication::quit);
    localSocket->connectToServer(QCoreApplication::arguments().at(1), QIODevice::ReadWrite | QIODevice::Unbuffered);
    localSocket->waitForConnected(-1);

    m_inputIoDevice = localSocket;
    m_outputIoDevice = localSocket;
    m_localSocket = localSocket;
}

void NodeInstanceClientProxy::initializeCapturedStream(const QString &fileName)
{
    m_inputIoDevice = new QFile(fileName, this);
    bool inputStreamCanBeOpened = m_inputIoDevice->open(QIODevice::ReadOnly);
    if (!inputStreamCanBeOpened) {
        qDebug() << "Input stream file cannot be opened: " << fileName;
        exit(-1);
    }

    if (QCoreApplication::arguments().count() == 3) {
        QFileInfo inputFileInfo(fileName);
        m_outputIoDevice = new QFile(inputFileInfo.path()+ "/" + inputFileInfo.baseName() + ".commandcontrolstream", this);
        bool outputStreamCanBeOpened = m_outputIoDevice->open(QIODevice::WriteOnly);
        if (!outputStreamCanBeOpened) {
            qDebug() << "Output stream file cannot be opened";
            exit(-1);
        }
    } else if (QCoreApplication::arguments().count() == 4) {
        m_controlStream.setFileName(QCoreApplication::arguments().at(3));
        bool controlStreamCanBeOpened = m_controlStream.open(QIODevice::ReadOnly);
        if (!controlStreamCanBeOpened) {
            qDebug() << "Control stream file cannot be opened";
            exit(-1);
        }
    }

}

bool compareCommands(const QVariant &command, const QVariant &controlCommand)
{
    static const int informationChangedCommandType = QMetaType::fromName("InformationChangedCommand").id();
    static const int valuesChangedCommandType = QMetaType::fromName("ValuesChangedCommand").id();
    static const int valuesModifiedCommandType = QMetaType::fromName("ValuesModifiedCommand").id();
    static const int pixmapChangedCommandType = QMetaType::fromName("PixmapChangedCommand").id();
    static const int childrenChangedCommandType = QMetaType::fromName("ChildrenChangedCommand").id();
    static const int statePreviewImageChangedCommandType = QMetaType::fromName("StatePreviewImageChangedCommand").id();
    static const int componentCompletedCommandType = QMetaType::fromName("ComponentCompletedCommand").id();
    static const int synchronizeCommandType = QMetaType::fromName("SynchronizeCommand").id();
    static const int tokenCommandType = QMetaType::fromName("TokenCommand").id();
    static const int debugOutputCommandType = QMetaType::fromName("DebugOutputCommand").id();
    static const int changeSelectionCommandType = QMetaType::fromName("ChangeSelectionCommand").id();

    if (command.typeId() == controlCommand.typeId()) {
        if (command.typeId() == informationChangedCommandType)
            return command.value<InformationChangedCommand>() == controlCommand.value<InformationChangedCommand>();
        else if (command.typeId() == valuesChangedCommandType)
            return command.value<ValuesChangedCommand>() == controlCommand.value<ValuesChangedCommand>();
        else if (command.typeId() == valuesModifiedCommandType)
            return command.value<ValuesModifiedCommand>() == controlCommand.value<ValuesModifiedCommand>();
        else if (command.typeId() == pixmapChangedCommandType)
            return command.value<PixmapChangedCommand>() == controlCommand.value<PixmapChangedCommand>();
        else if (command.typeId() == childrenChangedCommandType)
            return command.value<ChildrenChangedCommand>() == controlCommand.value<ChildrenChangedCommand>();
        else if (command.typeId() == statePreviewImageChangedCommandType)
            return command.value<StatePreviewImageChangedCommand>() == controlCommand.value<StatePreviewImageChangedCommand>();
        else if (command.typeId() == componentCompletedCommandType)
            return command.value<ComponentCompletedCommand>() == controlCommand.value<ComponentCompletedCommand>();
        else if (command.typeId() == synchronizeCommandType)
            return command.value<SynchronizeCommand>() == controlCommand.value<SynchronizeCommand>();
        else if (command.typeId() == tokenCommandType)
            return command.value<TokenCommand>() == controlCommand.value<TokenCommand>();
        else if (command.typeId() == debugOutputCommandType)
            return command.value<DebugOutputCommand>() == controlCommand.value<DebugOutputCommand>();
        else if (command.typeId() == changeSelectionCommandType)
            return command.value<ChangeSelectionCommand>()
                   == controlCommand.value<ChangeSelectionCommand>();
    }

    return false;
}

void NodeInstanceClientProxy::writeCommand(const QVariant &command)
{
    if (m_controlStream.isReadable()) {
        static quint32 readCommandCounter = 0;
        static quint32 blockSize = 0;

        QVariant controlCommand = readCommandFromIOStream(&m_controlStream, &readCommandCounter, &blockSize);

        if (!compareCommands(command, controlCommand)) {
            qDebug() << "Commands differ!";
            exit(-1);
        }
    } else if (m_outputIoDevice) {
#ifdef NANOTRACE_DESIGNSTUDIO_ENABLED
        if (command.typeId() != QMetaType::fromName("PuppetAliveCommand").id()) {
            if (command.typeId() == QMetaType::fromName("SyncNanotraceCommand").id()) {
                SyncNanotraceCommand cmd = command.value<SyncNanotraceCommand>();
                NANOTRACE_INSTANT_ARGS("Sync", "writeCommand",
                    {"name", cmd.name().toStdString()},
                    {"counter", int64_t(m_writeCommandCounter)});

            } else {
                NANOTRACE_INSTANT_ARGS("Update", "writeCommand",
                    {"name", command.typeName()},
                    {"counter", int64_t(m_writeCommandCounter)});
            }
        }
#endif
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_4_8);
        out << quint32(0);
        out << quint32(m_writeCommandCounter);
        m_writeCommandCounter++;
        out << command;
        out.device()->seek(0);
        out << quint32(block.size() - sizeof(quint32));

        m_outputIoDevice->write(block);
    }
}

void NodeInstanceClientProxy::informationChanged(const InformationChangedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::valuesChanged(const ValuesChangedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::valuesModified(const ValuesModifiedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::pixmapChanged(const PixmapChangedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::childrenChanged(const ChildrenChangedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::statePreviewImagesChanged(const StatePreviewImageChangedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::componentCompleted(const ComponentCompletedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::token(const TokenCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::debugOutput(const DebugOutputCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::puppetAlive(const PuppetAliveCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::selectionChanged(const ChangeSelectionCommand &command)
{
     writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::handlePuppetToCreatorCommand(const PuppetToCreatorCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::capturedData(const CapturedDataCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::sceneCreated(const SceneCreatedCommand &command)
{
    writeCommand(QVariant::fromValue(command));
}

void NodeInstanceClientProxy::flush()
{
    if (m_localSocket)
        m_localSocket->flush();
}

void NodeInstanceClientProxy::synchronizeWithClientProcess()
{
    if (m_synchronizeId >= 0) {
        SynchronizeCommand synchronizeCommand(m_synchronizeId);
        writeCommand(QVariant::fromValue(synchronizeCommand));
    }
}

qint64 NodeInstanceClientProxy::bytesToWrite() const
{
    return m_inputIoDevice->bytesToWrite();
}

QVariant NodeInstanceClientProxy::readCommandFromIOStream(QIODevice *ioDevice, quint32 *readCommandCounter, quint32 *blockSize)
{
    QDataStream in(ioDevice);
    in.setVersion(QDataStream::Qt_4_8);

    if (*blockSize == 0) {
        in >> *blockSize;
    }

    if (ioDevice->bytesAvailable() < *blockSize)
        return QVariant();

    quint32 commandCounter;
    in >> commandCounter;
    bool commandLost = !((commandCounter == 0 && *readCommandCounter == 0) || (*readCommandCounter + 1 == commandCounter));
    if (commandLost)
        qDebug() << "client command lost: " << *readCommandCounter <<  commandCounter;
    *readCommandCounter = commandCounter;

    QVariant command;
    in >> command;
    *blockSize = 0;

    if (in.status() != QDataStream::Ok) {
        qWarning() << "Stream is not OK";
        exit(1);
    }

    return command;
}

void NodeInstanceClientProxy::inputEvent(const InputEventCommand &command)
{
    nodeInstanceServer()->inputEvent(command);
}

void NodeInstanceClientProxy::view3DAction(const View3DActionCommand &command)
{
    nodeInstanceServer()->view3DAction(command);
}

void NodeInstanceClientProxy::requestModelNodePreviewImage(const RequestModelNodePreviewImageCommand &command)
{
    nodeInstanceServer()->requestModelNodePreviewImage(command);
}

void NodeInstanceClientProxy::changeLanguage(const ChangeLanguageCommand &command)
{
    nodeInstanceServer()->changeLanguage(command);
}

void NodeInstanceClientProxy::changePreviewImageSize(const ChangePreviewImageSizeCommand &command)
{
    nodeInstanceServer()->changePreviewImageSize(command);
}

void NodeInstanceClientProxy::startNanotrace(const StartNanotraceCommand &command)
{
    QString name = qApp->arguments().at(2);
    std::string directory = command.path().toStdString();
    std::string processName = name.toStdString();
    std::string fullFilePath =
        directory + std::string("/nanotrace_qmlpuppet_") + processName + std::string(".json");

    for (int i=0; i<name.length(); ++i) {
        if (i==0 || name[i]=='m')
            name[i] = name.at(i).toUpper();
    }
    processName = name.toStdString() + std::string("Puppet");

    NANOTRACE_INIT(processName.c_str(), "MainThread", fullFilePath);

    writeCommand(QVariant::fromValue(SyncNanotraceCommand(name)));
}

void NodeInstanceClientProxy::readDataStream()
{
    QList<QVariant> commandList;

    while (!m_inputIoDevice->atEnd()) {
        if (m_inputIoDevice->bytesAvailable() < int(sizeof(quint32)))
            break;

        static quint32 readCommandCounter = 0;
        static quint32 blockSize = 0;

        QVariant command = readCommandFromIOStream(m_inputIoDevice, &readCommandCounter, &blockSize);
#ifdef NANOTRACE_DESIGNSTUDIO_ENABLED
        if (command.typeId() != QMetaType::fromName("EndNanotraceCommand").id()) {
            if (command.typeId() == QMetaType::fromName("SyncNanotraceCommand").id()) {
                SyncNanotraceCommand cmd = command.value<SyncNanotraceCommand>();
                NANOTRACE_INSTANT_ARGS("Sync", "readCommand",
                    {"name", cmd.name().toStdString()},
                    {"counter", int64_t(readCommandCounter)});
                // Do not dispatch this command.
                continue;

            } else {
                NANOTRACE_INSTANT_ARGS("Update", "readCommand",
                    {"name", command.typeName()},
                    {"counter", int64_t(readCommandCounter)});
            }
        }
#endif
        if (command.isValid())
            commandList.append(command);
        else
            break;
    }

    for (const QVariant &command : std::as_const(commandList))
        dispatchCommand(command);
}

void NodeInstanceClientProxy::sendPuppetAliveCommand()
{
    puppetAlive(PuppetAliveCommand());
}

NodeInstanceServerInterface *NodeInstanceClientProxy::nodeInstanceServer() const
{
    return m_nodeInstanceServer.get();
}

void NodeInstanceClientProxy::setNodeInstanceServer(
    std::unique_ptr<NodeInstanceServerInterface> nodeInstanceServer)
{
    m_nodeInstanceServer = std::move(nodeInstanceServer);
}

void NodeInstanceClientProxy::createInstances(const CreateInstancesCommand &command)
{
    nodeInstanceServer()->createInstances(command);
}

void NodeInstanceClientProxy::changeFileUrl(const ChangeFileUrlCommand &command)
{
    nodeInstanceServer()->changeFileUrl(command);
}

void NodeInstanceClientProxy::createScene(const CreateSceneCommand &command)
{
    nodeInstanceServer()->createScene(command);
}

void NodeInstanceClientProxy::update3DViewState(const Update3dViewStateCommand &command)
{
    nodeInstanceServer()->update3DViewState(command);
}

void NodeInstanceClientProxy::clearScene(const ClearSceneCommand &command)
{
    nodeInstanceServer()->clearScene(command);
}

void NodeInstanceClientProxy::removeInstances(const RemoveInstancesCommand &command)
{
    nodeInstanceServer()->removeInstances(command);
}

void NodeInstanceClientProxy::removeProperties(const RemovePropertiesCommand &command)
{
    nodeInstanceServer()->removeProperties(command);
}

void NodeInstanceClientProxy::changePropertyBindings(const ChangeBindingsCommand &command)
{
    nodeInstanceServer()->changePropertyBindings(command);
}

void NodeInstanceClientProxy::changePropertyValues(const ChangeValuesCommand &command)
{
    nodeInstanceServer()->changePropertyValues(command);
}

void NodeInstanceClientProxy::changeAuxiliaryValues(const ChangeAuxiliaryCommand &command)
{
    nodeInstanceServer()->changeAuxiliaryValues(command);
}

void NodeInstanceClientProxy::reparentInstances(const ReparentInstancesCommand &command)
{
    nodeInstanceServer()->reparentInstances(command);
}

void NodeInstanceClientProxy::changeIds(const ChangeIdsCommand &command)
{
    nodeInstanceServer()->changeIds(command);
}

void NodeInstanceClientProxy::changeState(const ChangeStateCommand &command)
{
    nodeInstanceServer()->changeState(command);
}

void NodeInstanceClientProxy::completeComponent(const CompleteComponentCommand &command)
{
    nodeInstanceServer()->completeComponent(command);
}

void NodeInstanceClientProxy::changeNodeSource(const ChangeNodeSourceCommand &command)
{
    nodeInstanceServer()->changeNodeSource(command);
}

void NodeInstanceClientProxy::removeSharedMemory(const RemoveSharedMemoryCommand &command)
{
    nodeInstanceServer()->removeSharedMemory(command);
}
void NodeInstanceClientProxy::redirectToken(const TokenCommand &command)
{
    nodeInstanceServer()->token(command);
}

void NodeInstanceClientProxy::redirectToken(const EndPuppetCommand & /*command*/)
{
    if (m_outputIoDevice && m_outputIoDevice->isOpen())
        m_outputIoDevice->close();

    if (m_inputIoDevice && m_inputIoDevice->isOpen())
        m_inputIoDevice->close();

    if (m_controlStream.isOpen())
        m_controlStream.close();

    qDebug() << "End Process: " << QCoreApplication::applicationPid();
    QCoreApplication::exit();
}

void NodeInstanceClientProxy::changeSelection(const ChangeSelectionCommand &command)
{
    nodeInstanceServer()->changeSelection(command);
}

void NodeInstanceClientProxy::dispatchCommand(const QVariant &command)
{
    NANOTRACE_SCOPE_ARGS("Update", "dispatchCommand", {"name", command.typeName()});

    static const int createInstancesCommandType = QMetaType::fromName("CreateInstancesCommand").id();
    static const int update3dViewStateCommand = QMetaType::fromName("Update3dViewStateCommand").id();
    static const int changeFileUrlCommandType = QMetaType::fromName("ChangeFileUrlCommand").id();
    static const int createSceneCommandType = QMetaType::fromName("CreateSceneCommand").id();
    static const int clearSceneCommandType = QMetaType::fromName("ClearSceneCommand").id();
    static const int removeInstancesCommandType = QMetaType::fromName("RemoveInstancesCommand").id();
    static const int removePropertiesCommandType = QMetaType::fromName("RemovePropertiesCommand").id();
    static const int changeBindingsCommandType = QMetaType::fromName("ChangeBindingsCommand").id();
    static const int changeValuesCommandType = QMetaType::fromName("ChangeValuesCommand").id();
    static const int changeAuxiliaryCommandType = QMetaType::fromName("ChangeAuxiliaryCommand").id();
    static const int reparentInstancesCommandType = QMetaType::fromName("ReparentInstancesCommand").id();
    static const int changeIdsCommandType = QMetaType::fromName("ChangeIdsCommand").id();
    static const int changeStateCommandType = QMetaType::fromName("ChangeStateCommand").id();
    static const int completeComponentCommandType = QMetaType::fromName("CompleteComponentCommand").id();
    static const int synchronizeCommandType = QMetaType::fromName("SynchronizeCommand").id();
    static const int changeNodeSourceCommandType = QMetaType::fromName("ChangeNodeSourceCommand").id();
    static const int removeSharedMemoryCommandType = QMetaType::fromName("RemoveSharedMemoryCommand").id();
    static const int tokenCommandType = QMetaType::fromName("TokenCommand").id();
    static const int endPuppetCommandType = QMetaType::fromName("EndPuppetCommand").id();
    static const int changeSelectionCommandType = QMetaType::fromName("ChangeSelectionCommand").id();
    static const int inputEventCommandType = QMetaType::fromName("InputEventCommand").id();
    static const int view3DActionCommandType = QMetaType::fromName("View3DActionCommand").id();
    static const int requestModelNodePreviewImageCommandType = QMetaType::fromName("RequestModelNodePreviewImageCommand").id();
    static const int changeLanguageCommand = QMetaType::fromName("ChangeLanguageCommand").id();
    static const int changePreviewImageSizeCommand = QMetaType::fromName(
        "ChangePreviewImageSizeCommand").id();
    static const int startNanotraceCommandType = QMetaType::fromName("StartNanotraceCommand").id();
    static const int endNanotraceCommandType = QMetaType::fromName("EndNanotraceCommand").id();

    const int commandType = command.typeId();

    if (commandType == inputEventCommandType)
        inputEvent(command.value<InputEventCommand>());
    else if (commandType == createInstancesCommandType)
        createInstances(command.value<CreateInstancesCommand>());
    else if (commandType == update3dViewStateCommand)
        update3DViewState(command.value<Update3dViewStateCommand>());
    else if (commandType == changeFileUrlCommandType)
        changeFileUrl(command.value<ChangeFileUrlCommand>());
    else if (commandType == createSceneCommandType)
        createScene(command.value<CreateSceneCommand>());
    else if (commandType == clearSceneCommandType)
        clearScene(command.value<ClearSceneCommand>());
    else if (commandType == removeInstancesCommandType)
        removeInstances(command.value<RemoveInstancesCommand>());
    else if (commandType == removePropertiesCommandType)
        removeProperties(command.value<RemovePropertiesCommand>());
    else if (commandType == changeBindingsCommandType)
        changePropertyBindings(command.value<ChangeBindingsCommand>());
    else if (commandType == changeValuesCommandType)
        changePropertyValues(command.value<ChangeValuesCommand>());
    else if (commandType == changeAuxiliaryCommandType)
        changeAuxiliaryValues(command.value<ChangeAuxiliaryCommand>());
    else if (commandType == reparentInstancesCommandType)
        reparentInstances(command.value<ReparentInstancesCommand>());
    else if (commandType == changeIdsCommandType)
        changeIds(command.value<ChangeIdsCommand>());
    else if (commandType == changeStateCommandType)
        changeState(command.value<ChangeStateCommand>());
    else if (commandType == completeComponentCommandType)
        completeComponent(command.value<CompleteComponentCommand>());
    else if (commandType == changeNodeSourceCommandType)
        changeNodeSource(command.value<ChangeNodeSourceCommand>());
    else if (commandType == removeSharedMemoryCommandType)
        removeSharedMemory(command.value<RemoveSharedMemoryCommand>());
    else if (commandType == tokenCommandType)
        redirectToken(command.value<TokenCommand>());
    else if (commandType == endPuppetCommandType)
        redirectToken(command.value<EndPuppetCommand>());
    else if (commandType == view3DActionCommandType)
        view3DAction(command.value<View3DActionCommand>());
    else if (commandType == requestModelNodePreviewImageCommandType)
        requestModelNodePreviewImage(command.value<RequestModelNodePreviewImageCommand>());
    else if (commandType == synchronizeCommandType) {
        SynchronizeCommand synchronizeCommand = command.value<SynchronizeCommand>();
        m_synchronizeId = synchronizeCommand.synchronizeId();
    } else if (commandType == changeSelectionCommandType) {
        ChangeSelectionCommand changeSelectionCommand = command.value<ChangeSelectionCommand>();
        changeSelection(changeSelectionCommand);
    } else if (command.typeId() == changeLanguageCommand) {
        changeLanguage(command.value<ChangeLanguageCommand>());
    } else if (command.typeId() == changePreviewImageSizeCommand) {
        changePreviewImageSize(command.value<ChangePreviewImageSizeCommand>());
    } else if (command.typeId() == startNanotraceCommandType) {
        startNanotrace(command.value<StartNanotraceCommand>());
    } else if (command.typeId() == endNanotraceCommandType) {
        NANOTRACE_SHUTDOWN();
    } else {
        Q_ASSERT(false);
    }
}
} // namespace QmlDesigner

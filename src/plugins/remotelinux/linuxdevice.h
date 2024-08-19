// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "remotelinux_export.h"

#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/devicesupport/idevicefactory.h>

namespace RemoteLinux {

class REMOTELINUX_EXPORT LinuxDevice : public ProjectExplorer::IDevice
{
public:
    using Ptr = std::shared_ptr<LinuxDevice>;
    using ConstPtr = std::shared_ptr<const LinuxDevice>;

    ~LinuxDevice();

    static Ptr create() { return Ptr(new LinuxDevice); }

    ProjectExplorer::IDeviceWidget *createWidget() override;

    bool canCreateProcessModel() const override { return true; }
    bool hasDeviceTester() const override { return true; }
    ProjectExplorer::DeviceTester *createDeviceTester() const override;
    ProjectExplorer::DeviceProcessSignalOperation::Ptr signalOperation() const override;
    bool usableAsBuildDevice() const override;

    QString userAtHost() const;
    QString userAtHostAndPort() const;

    Utils::FilePath rootPath() const override;

    bool handlesFile(const Utils::FilePath &filePath) const override;

    Utils::ProcessInterface *createProcessInterface() const override;
    ProjectExplorer::FileTransferInterface *createFileTransferInterface(
            const ProjectExplorer::FileTransferSetupData &setup) const override;

    class LinuxDevicePrivate *connectionAccess() const;
    void checkOsType() override;

    DeviceState deviceState() const override;
    QString deviceStateToString() const override;

    bool isDisconnected() const;
    bool tryToConnect();

protected:
    LinuxDevice();

    Utils::BoolAspect disconnected{this};

    void _setOsType(Utils::OsType osType);

    class LinuxDevicePrivate *d;
    friend class LinuxDevicePrivate;
};

namespace Internal { void setupLinuxDevice(); }

} // namespace RemoteLinux

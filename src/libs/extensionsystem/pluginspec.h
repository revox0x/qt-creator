// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.h"

#include "iplugin.h"

#include <utils/expected.h>
#include <utils/filepath.h>

#include <QHash>
#include <QStaticPlugin>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
class QRegularExpression;
QT_END_NAMESPACE

class tst_PluginSpec;

namespace ExtensionSystem {

namespace Internal {

class OptionsParser;
class PluginSpecImplPrivate;
class PluginManagerPrivate;
class PluginSpecPrivate;

} // Internal

class PluginView;

struct EXTENSIONSYSTEM_EXPORT PluginDependency
{
    enum Type { Required, Optional, Test };

    PluginDependency() : type(Required) {}
    PluginDependency(const QString &name, const QString &version, Type type = Required)
        : name(name)
        , version(version)
        , type(type)
    {}

    QString name;
    QString version;
    Type type;
    bool operator==(const PluginDependency &other) const;
    QString toString() const;
};

size_t EXTENSIONSYSTEM_EXPORT qHash(const PluginDependency &value);

struct EXTENSIONSYSTEM_EXPORT PluginArgumentDescription
{
    QString name;
    QString parameter;
    QString description;
};

struct EXTENSIONSYSTEM_EXPORT PerformanceData
{
    qint64 load = 0;
    qint64 initialize = 0;
    qint64 extensionsInitialized = 0;
    qint64 delayedInitialize = 0;

    qint64 total() const { return load + initialize + extensionsInitialized + delayedInitialize; }
    QString summary() const
    {
        return QString("l: %1ms, i: %2ms, x: %3ms, d: %4ms")
            .arg(load, 3)
            .arg(initialize, 3)
            .arg(extensionsInitialized, 3)
            .arg(delayedInitialize, 3);
    }
};

using PluginSpecs = QList<class PluginSpec *>;

class EXTENSIONSYSTEM_EXPORT PluginSpec
{
    friend class ::tst_PluginSpec;
    friend class Internal::PluginManagerPrivate;
    friend class Internal::OptionsParser;

public:
    PluginSpec();
    virtual ~PluginSpec();

    using PluginArgumentDescriptions = QVector<PluginArgumentDescription>;
    enum State { Invalid, Read, Resolved, Loaded, Initialized, Running, Stopped, Deleted};

    // information read from the plugin, valid after 'Read' state is reached
    virtual QString name() const;
    virtual QString version() const;
    virtual QString compatVersion() const;
    virtual QString vendor() const;
    virtual QString copyright() const;
    virtual QString license() const;
    virtual QString description() const;
    virtual QString longDescription() const;
    virtual QString url() const;
    virtual QString documentationUrl() const;
    virtual QString category() const;
    virtual QString revision() const;
    virtual QRegularExpression platformSpecification() const;

    virtual bool isAvailableForHostPlatform() const;
    virtual bool isRequired() const;
    virtual bool isExperimental() const;
    virtual bool isDeprecated() const;
    virtual bool isEnabledByDefault() const;
    virtual bool isEnabledBySettings() const;
    virtual bool isEffectivelyEnabled() const;
    virtual bool isEnabledIndirectly() const;
    virtual bool isForceEnabled() const;
    virtual bool isForceDisabled() const;
    virtual bool isSoftLoadable() const;

    virtual QVector<PluginDependency> dependencies() const;
    virtual QJsonObject metaData() const;
    virtual PerformanceData &performanceData() const;
    virtual PluginArgumentDescriptions argumentDescriptions() const;
    virtual Utils::FilePath location() const;
    virtual Utils::FilePath filePath() const;
    virtual QStringList arguments() const;
    virtual void setArguments(const QStringList &arguments);
    virtual void addArgument(const QString &argument);
    virtual QHash<PluginDependency, PluginSpec *> dependencySpecs() const;

    virtual bool provides(PluginSpec *spec, const PluginDependency &dependency) const;
    virtual bool requiresAny(const QSet<PluginSpec *> &plugins) const;
    virtual PluginSpecs enableDependenciesIndirectly(bool enableTestDependencies);
    virtual bool resolveDependencies(const PluginSpecs &pluginSpecs);

    virtual IPlugin *plugin() const = 0;
    virtual State state() const;
    virtual bool hasError() const;
    virtual QString errorString() const;

    static bool isValidVersion(const QString &version);
    static int versionCompare(const QString &version1, const QString &version2);

    virtual void setEnabledBySettings(bool value);

    virtual Utils::FilePath installLocation(bool inUserFolder) const = 0;

protected:
    virtual void setEnabledByDefault(bool value);
    virtual void setEnabledIndirectly(bool value);
    virtual void setForceDisabled(bool value);
    virtual void setForceEnabled(bool value);

    virtual bool loadLibrary() = 0;
    virtual bool initializePlugin() = 0;
    virtual bool initializeExtensions() = 0;
    virtual bool delayedInitialize() = 0;
    virtual IPlugin::ShutdownFlag stop() = 0;
    virtual void kill() = 0;

    virtual void setError(const QString &errorString);

protected:
    virtual void setState(State state);

    virtual void setLocation(const Utils::FilePath &location);
    virtual void setFilePath(const Utils::FilePath &filePath);
    virtual Utils::expected_str<void> readMetaData(const QJsonObject &metaData);
    Utils::expected_str<void> reportError(const QString &error);

private:
    std::unique_ptr<Internal::PluginSpecPrivate> d;
};

using PluginFromArchiveFactory = std::function<QList<PluginSpec *>(const Utils::FilePath &path)>;
EXTENSIONSYSTEM_EXPORT QList<PluginFromArchiveFactory> &pluginSpecsFromArchiveFactories();
EXTENSIONSYSTEM_EXPORT QList<PluginSpec *> pluginSpecsFromArchive(const Utils::FilePath &path);

EXTENSIONSYSTEM_EXPORT Utils::expected_str<PluginSpec *> readCppPluginSpec(
    const Utils::FilePath &filePath);
EXTENSIONSYSTEM_EXPORT Utils::expected_str<PluginSpec *> readCppPluginSpec(
    const QStaticPlugin &plugin);

class EXTENSIONSYSTEM_TEST_EXPORT CppPluginSpec : public PluginSpec
{
    friend EXTENSIONSYSTEM_EXPORT Utils::expected_str<PluginSpec *> readCppPluginSpec(
        const Utils::FilePath &filePath);
    friend EXTENSIONSYSTEM_EXPORT Utils::expected_str<PluginSpec *> readCppPluginSpec(
        const QStaticPlugin &plugin);

public:
    ~CppPluginSpec() override;

    // linked plugin instance, valid after 'Loaded' state is reached
    IPlugin *plugin() const override;

    bool loadLibrary() override;
    bool initializePlugin() override;
    bool initializeExtensions() override;
    bool delayedInitialize() override;
    IPlugin::ShutdownFlag stop() override;
    void kill() override;

    Utils::expected_str<void> readMetaData(const QJsonObject &pluginMetaData) override;

    Utils::FilePath installLocation(bool inUserFolder) const override;

protected:
    CppPluginSpec();

private:
    std::unique_ptr<Internal::PluginSpecImplPrivate> d;
    friend class PluginView;
    friend class Internal::OptionsParser;
    friend class Internal::PluginManagerPrivate;
    friend class Internal::PluginSpecImplPrivate;
    friend class ::tst_PluginSpec;
};

} // namespace ExtensionSystem

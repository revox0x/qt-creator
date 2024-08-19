// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "gerritparameters.h"
#include "gerritplugin.h"
#include "../gitclient.h"

#include <coreplugin/icore.h>

#include <utils/commandline.h>
#include <utils/datafromprocess.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/pathchooser.h>
#include <utils/qtcsettings.h>

#include <QDir>
#include <QStandardPaths>

using namespace Core;
using namespace Utils;

namespace Gerrit::Internal {

const char settingsGroupC[] = "Gerrit";
const char hostKeyC[] = "Host";
const char userKeyC[] = "User";
const char portKeyC[] = "Port";
const char portFlagKeyC[] = "PortFlag";
const char sshKeyC[] = "Ssh";
const char curlKeyC[] = "Curl";
const char httpsKeyC[] = "Https";
const char savedQueriesKeyC[] = "SavedQueries";

const char defaultPortFlag[] = "-p";

static FilePath detectApp(const QString &defaultExe)
{
    const QString defaultApp = HostOsInfo::withExecutableSuffix(defaultExe);
    const QString app = QStandardPaths::findExecutable(defaultApp);
    if (!app.isEmpty() || !HostOsInfo::isWindowsHost())
        return FilePath::fromString(app);
    // Windows: Use app.exe from git if it cannot be found.
    const FilePath gitBinDir = Git::Internal::gitClient().gitBinDirectory();
    if (gitBinDir.isEmpty())
        return {};
    FilePath path = gitBinDir.pathAppended(defaultApp);
    if (path.exists())
        return path;

    // If app was not found, and git bin is Git/usr/bin (Git for Windows),
    // search also in Git/mingw{32,64}/bin
    if (!gitBinDir.endsWith("/usr/bin"))
        return {};
    path = gitBinDir.parentDir().parentDir();
    const FilePaths entries = path.dirEntries({{"mingw*"}});
    if (entries.isEmpty())
        return {};
    path = entries.first() / "bin" / defaultApp;
    if (path.exists())
        return path;
    return {};
}

static FilePath detectSsh()
{
    const QString gitSsh = qtcEnvironmentVariable("GIT_SSH");
    if (!gitSsh.isEmpty())
        return FilePath::fromString(gitSsh);
    return detectApp("ssh");
}

void GerritParameters::setPortFlagBySshType()
{
    bool isPlink = false;
    if (!ssh.isEmpty()) {
        DataFromProcess<QString>::Parameters params({ssh, {"-V"}},
                                                    [](const QString &output) { return output; });
        using namespace std::chrono_literals;
        params.timeout = 1s;
        if (const auto version = DataFromProcess<QString>::getData(params))
            isPlink = version->contains("plink", Qt::CaseInsensitive);
    }
    portFlag = QLatin1String(isPlink ? "-P" : defaultPortFlag);
}

GerritParameters::GerritParameters()
    : portFlag(defaultPortFlag)
{
}

void GerritParameters::toSettings() const
{
    QtcSettings *s = ICore::settings();
    s->beginGroup(settingsGroupC);
    s->setValue(hostKeyC, server.host);
    s->setValue(userKeyC, server.user.userName);
    s->setValue(portKeyC, server.port);
    s->setValue(portFlagKeyC, portFlag);
    s->setValue(sshKeyC, ssh.toSettings());
    s->setValue(curlKeyC, curl.toSettings());
    s->setValue(httpsKeyC, https);
    s->endGroup();
}

void GerritParameters::saveQueries() const
{
    QtcSettings *s = ICore::settings();
    s->beginGroup(settingsGroupC);
    s->setValue(savedQueriesKeyC, savedQueries.join(','));
    s->endGroup();
}

void GerritParameters::fromSettings()
{
    QtcSettings *s = ICore::settings();
    const Key rootKey = Key(settingsGroupC) + '/';
    server.host = s->value(rootKey + hostKeyC, GerritServer::defaultHost()).toString();
    server.user.userName = s->value(rootKey + userKeyC, QString()).toString();
    ssh = FilePath::fromSettings(s->value(rootKey + sshKeyC, QString()));
    curl = FilePath::fromSettings(s->value(rootKey + curlKeyC));
    server.port = ushort(s->value(rootKey + portKeyC, QVariant(GerritServer::defaultPort)).toInt());
    portFlag = s->value(rootKey + portFlagKeyC, defaultPortFlag).toString();
    savedQueries = s->value(rootKey + savedQueriesKeyC, QString()).toString()
            .split(',');
    https = s->value(rootKey + httpsKeyC, QVariant(true)).toBool();
    if (ssh.isEmpty() || !ssh.exists())
        ssh = detectSsh();
    if (curl.isEmpty() || !curl.exists())
        curl = detectApp("curl");
}

bool GerritParameters::isValid() const
{
    return !server.host.isEmpty() && !server.user.userName.isEmpty() && !ssh.isEmpty();
}

GerritParameters &gerritSettings()
{
    static GerritParameters theGerritSettings;
    return theGerritSettings;
}

} // Gerrit::Internal

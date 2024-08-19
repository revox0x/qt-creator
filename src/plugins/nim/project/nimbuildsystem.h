// Copyright (C) Filippo Cucchetto <filippocucchetto@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/buildsystem.h>
#include <projectexplorer/treescanner.h>

#include <utils/filesystemwatcher.h>

namespace ProjectExplorer { class Kit; }

namespace Nim {

Utils::FilePath nimPathFromKit(ProjectExplorer::Kit *kit);
Utils::FilePath nimblePathFromKit(ProjectExplorer::Kit *kit);

class NimProjectScanner : public QObject
{
    Q_OBJECT

public:
    explicit NimProjectScanner(ProjectExplorer::Project *project);

    void startScan();
    void watchProjectFilePath();

    void setExcludedFiles(const QStringList &list);
    QStringList excludedFiles() const;

    bool addFiles(const QStringList &filePaths);
    ProjectExplorer::RemovedFilesFromProject removeFiles(const QStringList &filePaths);
    bool renameFile(const QString &from, const QString &to);

signals:
    void finished();
    void requestReparse();
    void directoryChanged(const QString &path);
    void fileChanged(const QString &path);

private:
    void loadSettings();
    void saveSettings();

    ProjectExplorer::Project *m_project = nullptr;
    ProjectExplorer::TreeScanner m_scanner;
    Utils::FileSystemWatcher m_directoryWatcher;
};

ProjectExplorer::BuildSystem *createNimBuildSystem(ProjectExplorer::Target *target);

} // namespace Nim

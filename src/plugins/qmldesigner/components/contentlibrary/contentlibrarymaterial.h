// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "nodeinstanceglobal.h"

#include <QObject>
#include <QUrl>

namespace QmlDesigner {

class ContentLibraryMaterial : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString bundleMaterialName MEMBER m_name CONSTANT)
    Q_PROPERTY(QUrl bundleMaterialIcon MEMBER m_icon CONSTANT)
    Q_PROPERTY(bool bundleMaterialVisible MEMBER m_visible NOTIFY materialVisibleChanged)
    Q_PROPERTY(bool bundleItemImported READ imported WRITE setImported NOTIFY materialImportedChanged)
    Q_PROPERTY(QString bundleMaterialBaseWebUrl MEMBER m_baseWebUrl CONSTANT)
    Q_PROPERTY(QString bundleMaterialDirPath READ dirPath CONSTANT)
    Q_PROPERTY(QStringList bundleMaterialFiles READ allFiles CONSTANT)
    Q_PROPERTY(QString itemType MEMBER m_itemType CONSTANT)

public:
    ContentLibraryMaterial(QObject *parent,
                           const QString &name,
                           const QString &qml,
                           const TypeName &type,
                           const QUrl &icon,
                           const QStringList &files,
                           const QString &downloadPath,
                           const QString &baseWebUrl = {});

    bool filter(const QString &searchText);

    Q_INVOKABLE bool isDownloaded() const;

    QString name() const;
    QUrl icon() const;
    QString qml() const;
    TypeName type() const;
    QStringList files() const;
    bool visible() const;
    QString qmlFilePath() const;

    bool setImported(bool imported);
    bool imported() const;
    QString dirPath() const;
    QStringList allFiles() const;

signals:
    void materialVisibleChanged();
    void materialImportedChanged();

private:
    QString m_name;
    QString m_qml;
    TypeName m_type;
    QUrl m_icon;
    QStringList m_files;

    bool m_visible = true;
    bool m_imported = false;

    QString m_downloadPath;
    QString m_baseWebUrl;
    QStringList m_allFiles;
    const QString m_itemType = "material";
};

} // namespace QmlDesigner

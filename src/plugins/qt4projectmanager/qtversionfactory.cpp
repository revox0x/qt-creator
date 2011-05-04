/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "qtversionfactory.h"
#include "profilereader.h"
#include "qtversionmanager.h"

#include <extensionsystem/pluginmanager.h>

using namespace Qt4ProjectManager;
using namespace Qt4ProjectManager::Internal;

QtVersionFactory::QtVersionFactory(QObject *parent) :
    QObject(parent)
{

}

QtVersionFactory::~QtVersionFactory()
{

}

bool sortByPriority(QtVersionFactory *a, QtVersionFactory *b)
{
    return a->priority() > b->priority();
}

BaseQtVersion *QtVersionFactory::createQtVersionFromQMakePath(const QString &qmakePath, bool isAutoDetected, const QString &autoDetectionSource)
{
    QHash<QString, QString> versionInfo;
    bool success = BaseQtVersion::queryQMakeVariables(qmakePath, &versionInfo);
    if (!success)
        return 0;
    QString mkspec = BaseQtVersion::mkspecFromVersionInfo(versionInfo);

    ProFileOption option;
    option.properties = versionInfo;
    ProMessageHandler msgHandler(true);
    ProFileCacheManager::instance()->incRefCount();
    ProFileParser parser(ProFileCacheManager::instance()->cache(), &msgHandler);
    ProFileEvaluator evaluator(&option, &parser, &msgHandler);
    if (ProFile *pro = parser.parsedProFile(mkspec + "/qmake.conf")) {
        evaluator.setCumulative(false);
        evaluator.accept(pro, ProFileEvaluator::LoadProOnly);
        pro->deref();
    }

    QList<QtVersionFactory *> factories = ExtensionSystem::PluginManager::instance()->getObjects<QtVersionFactory>();
    qSort(factories.begin(), factories.end(), &sortByPriority);

    foreach (QtVersionFactory *factory, factories) {
        BaseQtVersion *ver = factory->create(qmakePath, &evaluator, isAutoDetected, autoDetectionSource);
        if (ver) {
            ProFileCacheManager::instance()->decRefCount();
            return ver;
        }
    }
    ProFileCacheManager::instance()->decRefCount();
    return 0;
}

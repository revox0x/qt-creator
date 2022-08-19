// Copyright (C) 2020 Miklos Marton <martonmiklosqdev@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#include "vcsoutputformatter.h"

#include <coreplugin/iversioncontrol.h>
#include <coreplugin/vcsmanager.h>

#include <utils/qtcassert.h>
#include <utils/stringutils.h>

#include <QDesktopServices>
#include <QMenu>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QUrl>

using namespace Utils;

namespace VcsBase {

VcsOutputLineParser::VcsOutputLineParser() :
    m_regexp(
        "(https?://\\S*)"                             // https://codereview.org/c/1234
        "|(v[0-9]+\\.[0-9]+\\.[0-9]+[\\-A-Za-z0-9]*)" // v0.1.2-beta3
        "|([0-9a-f]{6,}(?:\\.{2,3}[0-9a-f]{6,}"       // 789acf or 123abc..456cde
        "|\\^+|~\\d+)?)")                             // or 789acf^ or 123abc~99
{
}

OutputLineParser::Result VcsOutputLineParser::handleLine(const QString &text, OutputFormat format)
{
    Q_UNUSED(format);
    QRegularExpressionMatchIterator it = m_regexp.globalMatch(text);
    if (!it.hasNext())
        return Status::NotHandled;
    LinkSpecs linkSpecs;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int startPos = match.capturedStart();
        QStringView url = match.capturedView();
        while (url.rbegin()->isPunct())
            url.chop(1);
        linkSpecs << LinkSpec(startPos, url.length(), url.toString());
    }
    return {Status::Done, linkSpecs};
}

bool VcsOutputLineParser::handleVcsLink(const FilePath &workingDirectory, const QString &href)
{
    using namespace Core;
    QTC_ASSERT(!href.isEmpty(), return false);
    if (href.startsWith("http://") || href.startsWith("https://")) {
        QDesktopServices::openUrl(QUrl(href));
        return true;
    }
    if (IVersionControl *vcs = VcsManager::findVersionControlForDirectory(workingDirectory))
        return vcs->handleLink(workingDirectory, href);
    return false;
}

void VcsOutputLineParser::fillLinkContextMenu(
        QMenu *menu, const FilePath &workingDirectory, const QString &href)
{
    QTC_ASSERT(!href.isEmpty(), return);
    if (href.startsWith("http://") || href.startsWith("https://")) {
        QAction *action = menu->addAction(
                    tr("&Open \"%1\"").arg(href),
                    [href] { QDesktopServices::openUrl(QUrl(href)); });
        menu->setDefaultAction(action);
        menu->addAction(tr("&Copy to clipboard: \"%1\"").arg(href),
                    [href] { setClipboardAndSelection(href); });
        return;
    }
    if (Core::IVersionControl *vcs = Core::VcsManager::findVersionControlForDirectory(workingDirectory))
        vcs->fillLinkContextMenu(menu, workingDirectory, href);
}

} // VcsBase

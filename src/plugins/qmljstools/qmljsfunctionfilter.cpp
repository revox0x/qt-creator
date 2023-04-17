// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmljsfunctionfilter.h"
#include "qmljslocatordata.h"
#include "qmljstoolstr.h"

#include <extensionsystem/pluginmanager.h>

#include <utils/algorithm.h>
#include <utils/asynctask.h>

#include <QRegularExpression>

using namespace Core;
using namespace QmlJSTools::Internal;
using namespace Utils;

Q_DECLARE_METATYPE(LocatorData::Entry)

FunctionFilter::FunctionFilter(LocatorData *data, QObject *parent)
    : ILocatorFilter(parent)
    , m_data(data)
{
    setId("Functions");
    setDisplayName(Tr::tr("QML Functions"));
    setDescription(Tr::tr("Locates QML functions in any open project."));
    setDefaultShortcutString("m");
    setDefaultIncludedByDefault(false);
}

static void matches(QPromise<void> &promise, const LocatorStorage &storage,
                    const QHash<FilePath, QList<LocatorData::Entry>> &locatorEntries)
{
    const QString input = storage.input();
    const Qt::CaseSensitivity caseSensitivityForPrefix = ILocatorFilter::caseSensitivity(input);
    const QRegularExpression regexp = ILocatorFilter::createRegExp(input);
    if (!regexp.isValid())
        return;

    LocatorFilterEntries entries[int(ILocatorFilter::MatchLevel::Count)];
    for (const QList<LocatorData::Entry> &items : locatorEntries) {
        for (const LocatorData::Entry &info : items) {
            if (promise.isCanceled())
                return;

            if (info.type != LocatorData::Function)
                continue;

            const QRegularExpressionMatch match = regexp.match(info.symbolName);
            if (match.hasMatch()) {
                LocatorFilterEntry filterEntry;
                filterEntry.displayName = info.displayName;
                filterEntry.linkForEditor = {info.fileName, info.line, info.column};
                filterEntry.extraInfo = info.extraInfo;
                filterEntry.highlightInfo = ILocatorFilter::highlightInfo(match);

                if (filterEntry.displayName.startsWith(input, caseSensitivityForPrefix))
                    entries[int(ILocatorFilter::MatchLevel::Best)].append(filterEntry);
                else if (filterEntry.displayName.contains(input, caseSensitivityForPrefix))
                    entries[int(ILocatorFilter::MatchLevel::Better)].append(filterEntry);
                else
                    entries[int(ILocatorFilter::MatchLevel::Good)].append(filterEntry);
            }
        }
    }

    for (auto &entry : entries) {
        if (promise.isCanceled())
            return;

        if (entry.size() < 1000)
            Utils::sort(entry, LocatorFilterEntry::compareLexigraphically);
    }
    storage.reportOutput(std::accumulate(std::begin(entries), std::end(entries),
                                         LocatorFilterEntries()));
}

LocatorMatcherTasks FunctionFilter::matchers()
{
    using namespace Tasking;

    TreeStorage<LocatorStorage> storage;

    const auto onSetup = [storage, entries = m_data->entries()](AsyncTask<void> &async) {
        async.setFutureSynchronizer(ExtensionSystem::PluginManager::futureSynchronizer());
        async.setConcurrentCallData(matches, *storage, entries);
    };

    return {{Async<void>(onSetup), storage}};
}

QList<LocatorFilterEntry> FunctionFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future,
                                                     const QString &entry)
{
    QList<LocatorFilterEntry> entries[int(MatchLevel::Count)];
    const Qt::CaseSensitivity caseSensitivityForPrefix = caseSensitivity(entry);

    const QRegularExpression regexp = createRegExp(entry);
    if (!regexp.isValid())
        return {};

    const QHash<Utils::FilePath, QList<LocatorData::Entry>> locatorEntries = m_data->entries();
    for (const QList<LocatorData::Entry> &items : locatorEntries) {
        if (future.isCanceled())
            break;

        for (const LocatorData::Entry &info : items) {
            if (info.type != LocatorData::Function)
                continue;

            const QRegularExpressionMatch match = regexp.match(info.symbolName);
            if (match.hasMatch()) {
                LocatorFilterEntry filterEntry;
                filterEntry.displayName = info.displayName;
                filterEntry.linkForEditor = {info.fileName, info.line, info.column};
                filterEntry.extraInfo = info.extraInfo;
                filterEntry.highlightInfo = highlightInfo(match);

                if (filterEntry.displayName.startsWith(entry, caseSensitivityForPrefix))
                    entries[int(MatchLevel::Best)].append(filterEntry);
                else if (filterEntry.displayName.contains(entry, caseSensitivityForPrefix))
                    entries[int(MatchLevel::Better)].append(filterEntry);
                else
                    entries[int(MatchLevel::Good)].append(filterEntry);
            }
        }
    }

    for (auto &entry : entries) {
        if (entry.size() < 1000)
            Utils::sort(entry, LocatorFilterEntry::compareLexigraphically);
    }
    return std::accumulate(std::begin(entries), std::end(entries), QList<LocatorFilterEntry>());
}

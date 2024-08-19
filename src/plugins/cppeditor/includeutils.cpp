// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "includeutils.h"

#include <cplusplus/pp-engine.h>
#include <cplusplus/PreprocessorClient.h>
#include <cplusplus/PreprocessorEnvironment.h>

#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>

#ifdef WITH_TESTS
#include "cppmodelmanager.h"
#include "cppsourceprocessertesthelper.h"
#include "cppsourceprocessor.h"
#include <QtTest>
#endif // WITH_TESTS

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>

using namespace CPlusPlus;
using namespace Utils;

namespace CppEditor::Internal {

using Include = CPlusPlus::Document::Include;
using IncludeType = CPlusPlus::Client::IncludeType;

class IncludeGroup
{
public:
    static QList<IncludeGroup> detectIncludeGroupsByNewLines(QList<Include> &includes);
    static QList<IncludeGroup> detectIncludeGroupsByIncludeDir(const QList<Include> &includes);
    static QList<IncludeGroup> detectIncludeGroupsByIncludeType(const QList<Include> &includes);

    static QList<IncludeGroup> filterMixedIncludeGroups(const QList<IncludeGroup> &groups);
    static QList<IncludeGroup> filterIncludeGroups(const QList<IncludeGroup> &groups,
                                                   CPlusPlus::Client::IncludeType includeType);

public:
    explicit IncludeGroup(const QList<Include> &includes) : m_includes(includes) {}

    QList<Include> includes() const { return m_includes; }
    Include first() const { return m_includes.first(); }
    Include last() const { return m_includes.last(); }
    int size() const { return m_includes.size(); }
    bool isEmpty() const { return m_includes.isEmpty(); }

    QString commonPrefix() const;
    QString commonIncludeDir() const; /// only valid if hasCommonDir() == true
    bool hasCommonIncludeDir() const;
    bool hasOnlyIncludesOfType(CPlusPlus::Client::IncludeType includeType) const;
    bool isSorted() const; /// name-wise

    int lineForNewInclude(const QString &newIncludeFileName,
                          CPlusPlus::Client::IncludeType newIncludeType) const;

private:
    QStringList filesNames() const;

    QList<Include> m_includes;
};

static bool includeFileNamelessThen(const Include & left, const Include & right)
{
    return left.unresolvedFileName() < right.unresolvedFileName();
}

static int lineForAppendedIncludeGroup(const QList<IncludeGroup> &groups,
                                unsigned *newLinesToPrepend)
{
    if (newLinesToPrepend)
        *newLinesToPrepend += 1;
    return groups.last().last().line() + 1;
}

static int lineForPrependedIncludeGroup(const QList<IncludeGroup> &groups,
                                 unsigned *newLinesToAppend)
{
    if (newLinesToAppend)
        *newLinesToAppend += 1;
    return groups.first().first().line();
}

static QString includeDir(const QString &include)
{
    QString dirPrefix = QFileInfo(include).dir().path();
    if (dirPrefix == QLatin1String("."))
        return QString();
    dirPrefix.append(QLatin1Char('/'));
    return dirPrefix;
}

static int lineAfterFirstComment(const QTextDocument *textDocument)
{
    int insertLine = -1;

    QTextBlock block = textDocument->firstBlock();
    while (block.isValid()) {
        const QString trimmedText = block.text().trimmed();

        // Only skip the first comment!
        if (trimmedText.startsWith(QLatin1String("/*"))) {
            do {
                const int pos = block.text().indexOf(QLatin1String("*/"));
                if (pos > -1) {
                    insertLine = block.blockNumber() + 2;
                    break;
                }
                block = block.next();
            } while (block.isValid());
            break;
        } else if (trimmedText.startsWith(QLatin1String("//"))) {
            block = block.next();
            while (block.isValid()) {
                if (!block.text().trimmed().startsWith(QLatin1String("//"))) {
                    insertLine = block.blockNumber() + 1;
                    break;
                }
                block = block.next();
            }
            break;
        }

        if (!trimmedText.isEmpty())
            break;
        block = block.next();
    }

    return insertLine;
}

class LineForNewIncludeDirective
{
public:
    LineForNewIncludeDirective(const FilePath &filePath, const QTextDocument *textDocument,
                               const CPlusPlus::Document::Ptr cppDocument,
                               MocIncludeMode mocIncludeMode,
                               IncludeStyle includeStyle);

    /// Returns the line (1-based) at which the include directive should be inserted.
    /// On error, -1 is returned.
    int run(const QString &newIncludeFileName, unsigned *newLinesToPrepend = nullptr,
                   unsigned *newLinesToAppend = nullptr);

private:
    int findInsertLineForVeryFirstInclude(unsigned *newLinesToPrepend, unsigned *newLinesToAppend);
    QList<IncludeGroup> getGroupsByIncludeType(const QList<IncludeGroup> &groups,
                                               IncludeType includeType);

    const FilePath m_filePath;
    const QTextDocument *m_textDocument;
    const CPlusPlus::Document::Ptr m_cppDocument;

    IncludeStyle m_includeStyle;
    QList<Include> m_includes;
};

LineForNewIncludeDirective::LineForNewIncludeDirective(const FilePath &filePath,
                                                       const QTextDocument *textDocument,
                                                       const Document::Ptr cppDocument,
                                                       MocIncludeMode mocIncludeMode,
                                                       IncludeStyle includeStyle)
    : m_filePath(filePath)
    , m_textDocument(textDocument)
    , m_cppDocument(cppDocument)
    , m_includeStyle(includeStyle)
{
    const QList<Document::Include> includes = Utils::sorted(
                cppDocument->resolvedIncludes() + cppDocument->unresolvedIncludes(),
                &Include::line);

    // Ignore *.moc includes if requested
    if (mocIncludeMode == IgnoreMocIncludes) {
        for (const Document::Include &include : includes) {
            if (!include.unresolvedFileName().endsWith(QLatin1String(".moc")))
                m_includes << include;
        }
    } else {
        m_includes = includes;
    }

    // Detect include style
    if (m_includeStyle == AutoDetect) {
        unsigned timesIncludeStyleChanged = 0;
        if (m_includes.isEmpty() || m_includes.size() == 1) {
            m_includeStyle = LocalBeforeGlobal; // Fallback
        } else {
            for (int i = 1, size = m_includes.size(); i < size; ++i) {
                if (m_includes.at(i - 1).type() != m_includes.at(i).type()) {
                    if (++timesIncludeStyleChanged > 1)
                        break;
                }
            }
            if (timesIncludeStyleChanged == 1) {
                m_includeStyle = m_includes.first().type() == Client::IncludeLocal
                    ? LocalBeforeGlobal
                    : GlobalBeforeLocal;
            } else {
                m_includeStyle = LocalBeforeGlobal; // Fallback
            }
        }
    }
}

int LineForNewIncludeDirective::findInsertLineForVeryFirstInclude(unsigned *newLinesToPrepend,
                                                                  unsigned *newLinesToAppend)
{
    int insertLine = 1;

    const auto appendAndPrependNewline = [&] {
        if (newLinesToPrepend)
            *newLinesToPrepend = 1;
        if (newLinesToAppend)
            *newLinesToAppend += 1;
    };

    // If there is an include guard or a "#pragma once", insert right after that one
    if (const int pragmaOnceLine = m_cppDocument->pragmaOnceLine(); pragmaOnceLine != -1) {
        appendAndPrependNewline();
        insertLine = pragmaOnceLine + 1;
    } else if (const QByteArray includeGuardMacroName = m_cppDocument->includeGuardMacroName();
               !includeGuardMacroName.isEmpty()) {
        for (const Macro &definedMacro :  m_cppDocument->definedMacros()) {
            if (definedMacro.name() == includeGuardMacroName) {
                appendAndPrependNewline();
                insertLine = definedMacro.line() + 1;
            }
        }
        QTC_CHECK(insertLine != 1);
    } else {
        // Otherwise, if there is a comment, insert right after it
        insertLine = lineAfterFirstComment(m_textDocument);
        if (insertLine != -1) {
            if (newLinesToPrepend)
                *newLinesToPrepend = 1;

        // Otherwise, insert at top of file
        } else {
            if (newLinesToAppend)
                *newLinesToAppend += 1;
            insertLine = 1;
        }
    }

    return insertLine;
}

int LineForNewIncludeDirective::run(const QString &newIncludeFileName,
                                    unsigned *newLinesToPrepend,
                                    unsigned *newLinesToAppend)
{
    if (newLinesToPrepend)
        *newLinesToPrepend = false;
    if (newLinesToAppend)
        *newLinesToAppend = false;

    const QString pureIncludeFileName = newIncludeFileName.mid(1, newIncludeFileName.length() - 2);
    const Client::IncludeType newIncludeType =
        newIncludeFileName.startsWith(QLatin1Char('"')) ? Client::IncludeLocal
                                                        : Client::IncludeGlobal;

    // Handle no includes
    if (m_includes.empty())
        return findInsertLineForVeryFirstInclude(newLinesToPrepend, newLinesToAppend);

    using IncludeGroups = QList<IncludeGroup>;

    IncludeGroups groupsNewline = IncludeGroup::detectIncludeGroupsByNewLines(m_includes);

    // If the first group consists only of the header(s) for the including source file,
    // then it must stay as it is.
    if (groupsNewline.first().size() <= 2) {
        bool firstGroupIsSpecial = true;
        const QString baseName = m_filePath.baseName();
        const QString privBaseName = baseName + "_p";
        for (const auto &include : groupsNewline.first().includes()) {
            const QString inclBaseName = FilePath::fromString(include.unresolvedFileName())
                                             .baseName();
            if (inclBaseName != baseName && inclBaseName != privBaseName) {
                firstGroupIsSpecial = false;
                break;
            }
        }
        if (firstGroupIsSpecial) {
            if (groupsNewline.size() == 1) {
                *newLinesToPrepend = 1;
                return groupsNewline.first().last().line() + 1;
            }
            groupsNewline.removeFirst();
        }
    }

    const bool includeAtTop
        = (newIncludeType == Client::IncludeLocal && m_includeStyle == LocalBeforeGlobal)
            || (newIncludeType == Client::IncludeGlobal && m_includeStyle == GlobalBeforeLocal);
    IncludeGroup bestGroup = includeAtTop ? groupsNewline.first() : groupsNewline.last();

    IncludeGroups groupsMatchingIncludeType = getGroupsByIncludeType(groupsNewline, newIncludeType);
    if (groupsMatchingIncludeType.isEmpty()) {
        const IncludeGroups groupsMixedIncludeType
            = IncludeGroup::filterMixedIncludeGroups(groupsNewline);
        // case: The new include goes into an own include group
        if (groupsMixedIncludeType.isEmpty()) {
            return includeAtTop
                ? lineForPrependedIncludeGroup(groupsNewline, newLinesToAppend)
                : lineForAppendedIncludeGroup(groupsNewline, newLinesToPrepend);
        // case: add to mixed group
        } else {
            const IncludeGroup bestMixedGroup = groupsMixedIncludeType.last(); // TODO: flaterize
            const IncludeGroups groupsIncludeType
                = IncludeGroup::detectIncludeGroupsByIncludeType(bestMixedGroup.includes());
            groupsMatchingIncludeType = getGroupsByIncludeType(groupsIncludeType, newIncludeType);
            // Avoid extra new lines for include groups which are not separated by new lines
            newLinesToPrepend = nullptr;
            newLinesToAppend = nullptr;
        }
    }

    IncludeGroups groupsSameIncludeDir;
    IncludeGroups groupsMixedIncludeDirs;
    for (const IncludeGroup &group : std::as_const(groupsMatchingIncludeType)) {
        if (group.hasCommonIncludeDir())
            groupsSameIncludeDir << group;
        else
            groupsMixedIncludeDirs << group;
    }

    IncludeGroups groupsMatchingIncludeDir;
    for (const IncludeGroup &group : std::as_const(groupsSameIncludeDir)) {
        if (group.commonIncludeDir() == includeDir(pureIncludeFileName))
            groupsMatchingIncludeDir << group;
    }

    // case: There are groups with a matching include dir, insert the new include
    //       at the best position of the best group
    if (!groupsMatchingIncludeDir.isEmpty()) {
        // The group with the longest common matching prefix is the best group
        int longestPrefixSoFar = 0;
        for (const IncludeGroup &group : std::as_const(groupsMatchingIncludeDir)) {
            const int groupPrefixLength = group.commonPrefix().length();
            if (groupPrefixLength >= longestPrefixSoFar) {
                bestGroup = group;
                longestPrefixSoFar = groupPrefixLength;
            }
        }
    } else {
        // case: The new include goes into an own include group
        if (groupsMixedIncludeDirs.isEmpty()) {
            if (includeAtTop) {
                return groupsSameIncludeDir.isEmpty()
                    ? lineForPrependedIncludeGroup(groupsNewline, newLinesToAppend)
                    : lineForAppendedIncludeGroup(groupsSameIncludeDir, newLinesToPrepend);
            } else {
                return lineForAppendedIncludeGroup(groupsNewline, newLinesToPrepend);
            }
        // case: The new include is inserted at the best position of the best
        //       group with mixed include dirs
        } else {
            IncludeGroups groupsIncludeDir;
            for (const IncludeGroup &group : std::as_const(groupsMixedIncludeDirs)) {
                groupsIncludeDir.append(
                            IncludeGroup::detectIncludeGroupsByIncludeDir(group.includes()));
            }
            IncludeGroup localBestIncludeGroup = IncludeGroup(QList<Include>());
            for (const IncludeGroup &group : std::as_const(groupsIncludeDir)) {
                if (group.commonIncludeDir() == includeDir(pureIncludeFileName))
                    localBestIncludeGroup = group;
            }
            if (!localBestIncludeGroup.isEmpty())
                bestGroup = localBestIncludeGroup;
            else
                bestGroup = groupsMixedIncludeDirs.last();
        }
    }

    return bestGroup.lineForNewInclude(pureIncludeFileName, newIncludeType);
}

QList<IncludeGroup> LineForNewIncludeDirective::getGroupsByIncludeType(
        const QList<IncludeGroup> &groups, IncludeType includeType)
{
    return includeType == Client::IncludeLocal
        ? IncludeGroup::filterIncludeGroups(groups, Client::IncludeLocal)
        : IncludeGroup::filterIncludeGroups(groups, Client::IncludeGlobal);
}

int lineForNewIncludeDirective(const Utils::FilePath &filePath, const QTextDocument *textDocument,
                               const CPlusPlus::Document::Ptr cppDocument,
                               MocIncludeMode mocIncludeMode,
                               IncludeStyle includeStyle,
                               const QString &newIncludeFileName,
                               unsigned *newLinesToPrepend,
                               unsigned *newLinesToAppend)
{
    return LineForNewIncludeDirective(filePath, textDocument, cppDocument,
                                      mocIncludeMode, includeStyle)
          .run(newIncludeFileName, newLinesToPrepend, newLinesToAppend);
}

/// includes will be modified!
QList<IncludeGroup> IncludeGroup::detectIncludeGroupsByNewLines(QList<Document::Include> &includes)
{
    // Create groups
    QList<IncludeGroup> result;
    int lastLine = 0;
    QList<Include> currentIncludes;
    bool isFirst = true;
    for (const Include &include : std::as_const(includes)) {
        // First include...
        if (isFirst) {
            isFirst = false;
            currentIncludes << include;
        // Include belongs to current group
        } else if (lastLine + 1 == include.line()) {
            currentIncludes << include;
        // Include is member of new group
        } else {
            result << IncludeGroup(currentIncludes);
            currentIncludes.clear();
            currentIncludes << include;
        }

        lastLine = include.line();
    }

    if (!currentIncludes.isEmpty())
        result << IncludeGroup(currentIncludes);

    return result;
}

QList<IncludeGroup> IncludeGroup::detectIncludeGroupsByIncludeDir(const QList<Include> &includes)
{
    // Create sub groups
    QList<IncludeGroup> result;
    QString lastDir;
    QList<Include> currentIncludes;
    bool isFirst = true;
    for (const Include &include : includes) {
        const QString currentDirPrefix = includeDir(include.unresolvedFileName());

        // First include...
        if (isFirst) {
            isFirst = false;
            currentIncludes << include;
        // Include belongs to current group
        } else if (lastDir == currentDirPrefix) {
            currentIncludes << include;
        // Include is member of new group
        } else {
            result << IncludeGroup(currentIncludes);
            currentIncludes.clear();
            currentIncludes << include;
        }

        lastDir = currentDirPrefix;
    }

    if (!currentIncludes.isEmpty())
        result << IncludeGroup(currentIncludes);

    return result;
}

QList<IncludeGroup> IncludeGroup::detectIncludeGroupsByIncludeType(const QList<Include> &includes)
{
    // Create sub groups
    QList<IncludeGroup> result;
    Client::IncludeType lastIncludeType = Client::IncludeLocal;
    QList<Include> currentIncludes;
    bool isFirst = true;
    for (const Include &include : includes) {
        const Client::IncludeType currentIncludeType = include.type();

        // First include...
        if (isFirst) {
            isFirst = false;
            currentIncludes << include;
        // Include belongs to current group
        } else if (lastIncludeType == currentIncludeType) {
            currentIncludes << include;
        // Include is member of new group
        } else {
            result << IncludeGroup(currentIncludes);
            currentIncludes.clear();
            currentIncludes << include;
        }

        lastIncludeType = currentIncludeType;
    }

    if (!currentIncludes.isEmpty())
        result << IncludeGroup(currentIncludes);

    return result;
}

/// returns groups that solely contains includes of the given include type
QList<IncludeGroup> IncludeGroup::filterIncludeGroups(const QList<IncludeGroup> &groups,
                                                      Client::IncludeType includeType)
{
    QList<IncludeGroup> result;
    for (const IncludeGroup &group : groups) {
        if (group.hasOnlyIncludesOfType(includeType))
            result << group;
    }
    return result;
}

/// returns groups that contains includes with local and globale include type
QList<IncludeGroup> IncludeGroup::filterMixedIncludeGroups(const QList<IncludeGroup> &groups)
{
    QList<IncludeGroup> result;
    for (const IncludeGroup &group : groups) {
        if (!group.hasOnlyIncludesOfType(Client::IncludeLocal)
            && !group.hasOnlyIncludesOfType(Client::IncludeGlobal)) {
            result << group;
        }
    }
    return result;
}

bool IncludeGroup::hasOnlyIncludesOfType(Client::IncludeType includeType) const
{
    for (const Include &include : std::as_const(m_includes)) {
        if (include.type() != includeType)
            return false;
    }
    return true;
}

bool IncludeGroup::isSorted() const
{
    const QStringList names = filesNames();
    if (names.isEmpty() || names.size() == 1)
        return true;
    for (int i = 1, total = names.size(); i < total; ++i) {
        if (names.at(i) < names.at(i - 1))
            return false;
    }
    return true;
}

int IncludeGroup::lineForNewInclude(const QString &newIncludeFileName,
                                    Client::IncludeType newIncludeType) const
{
    if (m_includes.empty())
        return -1;

    if (isSorted()) {
        const Include newInclude(newIncludeFileName, FilePath(), 0, newIncludeType);
        const QList<Include>::const_iterator it = std::lower_bound(m_includes.begin(),
            m_includes.end(), newInclude, includeFileNamelessThen);
        if (it == m_includes.end())
            return m_includes.last().line() + 1;
        else
            return (*it).line();
    } else {
        return m_includes.last().line() + 1;
    }

    return -1;
}

QStringList IncludeGroup::filesNames() const
{
    QStringList names;
    for (const Include &include : std::as_const(m_includes))
        names << include.unresolvedFileName();
    return names;
}

QString IncludeGroup::commonPrefix() const
{
    const QStringList files = filesNames();
    if (files.size() <= 1)
        return QString(); // no prefix for single item groups
    return Utils::commonPrefix(files);
}

QString IncludeGroup::commonIncludeDir() const
{
    if (m_includes.isEmpty())
        return QString();
    return includeDir(m_includes.first().unresolvedFileName());
}

bool IncludeGroup::hasCommonIncludeDir() const
{
    if (m_includes.isEmpty())
        return false;

    const QString candidate = includeDir(m_includes.first().unresolvedFileName());
    for (int i = 1, size = m_includes.size(); i < size; ++i) {
        if (includeDir(m_includes.at(i).unresolvedFileName()) != candidate)
            return false;
    }
    return true;
}

} // CppEditor::Internal

#ifdef WITH_TESTS

namespace CppEditor::Internal {

using namespace Tests;
using Tests::Internal::TestIncludePaths;

class IncludeGroupsTest : public QObject
{
    Q_OBJECT

private slots:
    void testDetectIncludeGroupsByNewLines();
    void testDetectIncludeGroupsByIncludeDir();
    void testDetectIncludeGroupsByIncludeType();
};

static QList<Include> includesForSource(const FilePath &filePath)
{
    CppModelManager::GC();
    QScopedPointer<CppSourceProcessor> sourceProcessor(CppModelManager::createSourceProcessor());
    sourceProcessor->setHeaderPaths({ProjectExplorer::HeaderPath::makeUser(
                                     TestIncludePaths::globalIncludePath())});
    sourceProcessor->run(filePath);

    Document::Ptr document = CppModelManager::document(filePath);
    return document->resolvedIncludes();
}

void IncludeGroupsTest::testDetectIncludeGroupsByNewLines()
{
    const FilePath testFilePath = TestIncludePaths::testFilePath(
                QLatin1String("test_main_detectIncludeGroupsByNewLines.cpp"));

    QList<Include> includes = includesForSource(testFilePath);
    QCOMPARE(includes.size(), 17);
    QList<IncludeGroup> includeGroups
        = IncludeGroup::detectIncludeGroupsByNewLines(includes);
    QCOMPARE(includeGroups.size(), 8);

    QCOMPARE(includeGroups.at(0).size(), 1);
    QVERIFY(includeGroups.at(0).commonPrefix().isEmpty());
    QVERIFY(includeGroups.at(0).hasOnlyIncludesOfType(Client::IncludeLocal));
    QVERIFY(includeGroups.at(0).isSorted());

    QCOMPARE(includeGroups.at(1).size(), 2);
    QVERIFY(!includeGroups.at(1).commonPrefix().isEmpty());
    QVERIFY(includeGroups.at(1).hasOnlyIncludesOfType(Client::IncludeLocal));
    QVERIFY(includeGroups.at(1).isSorted());

    QCOMPARE(includeGroups.at(2).size(), 2);
    QVERIFY(!includeGroups.at(2).commonPrefix().isEmpty());
    QVERIFY(includeGroups.at(2).hasOnlyIncludesOfType(Client::IncludeGlobal));
    QVERIFY(!includeGroups.at(2).isSorted());

    QCOMPARE(includeGroups.at(6).size(), 3);
    QVERIFY(includeGroups.at(6).commonPrefix().isEmpty());
    QVERIFY(includeGroups.at(6).hasOnlyIncludesOfType(Client::IncludeGlobal));
    QVERIFY(!includeGroups.at(6).isSorted());

    QCOMPARE(includeGroups.at(7).size(), 3);
    QVERIFY(includeGroups.at(7).commonPrefix().isEmpty());
    QVERIFY(!includeGroups.at(7).hasOnlyIncludesOfType(Client::IncludeLocal));
    QVERIFY(!includeGroups.at(7).hasOnlyIncludesOfType(Client::IncludeGlobal));
    QVERIFY(!includeGroups.at(7).isSorted());

    QCOMPARE(IncludeGroup::filterIncludeGroups(includeGroups, Client::IncludeLocal).size(), 4);
    QCOMPARE(IncludeGroup::filterIncludeGroups(includeGroups, Client::IncludeGlobal).size(), 3);
    QCOMPARE(IncludeGroup::filterMixedIncludeGroups(includeGroups).size(), 1);
}

void IncludeGroupsTest::testDetectIncludeGroupsByIncludeDir()
{
    const FilePath testFilePath = TestIncludePaths::testFilePath(
                QLatin1String("test_main_detectIncludeGroupsByIncludeDir.cpp"));

    QList<Include> includes = includesForSource(testFilePath);
    QCOMPARE(includes.size(), 9);
    QList<IncludeGroup> includeGroups
        = IncludeGroup::detectIncludeGroupsByIncludeDir(includes);
    QCOMPARE(includeGroups.size(), 4);

    QCOMPARE(includeGroups.at(0).size(), 2);
    QVERIFY(includeGroups.at(0).commonIncludeDir().isEmpty());

    QCOMPARE(includeGroups.at(1).size(), 2);
    QCOMPARE(includeGroups.at(1).commonIncludeDir(), QLatin1String("lib/"));

    QCOMPARE(includeGroups.at(2).size(), 2);
    QCOMPARE(includeGroups.at(2).commonIncludeDir(), QLatin1String("otherlib/"));

    QCOMPARE(includeGroups.at(3).size(), 3);
    QCOMPARE(includeGroups.at(3).commonIncludeDir(), QLatin1String(""));
}

void IncludeGroupsTest::testDetectIncludeGroupsByIncludeType()
{
    const FilePath testFilePath = TestIncludePaths::testFilePath(
                QLatin1String("test_main_detectIncludeGroupsByIncludeType.cpp"));

    QList<Include> includes = includesForSource(testFilePath);
    QCOMPARE(includes.size(), 9);
    QList<IncludeGroup> includeGroups
        = IncludeGroup::detectIncludeGroupsByIncludeDir(includes);
    QCOMPARE(includeGroups.size(), 4);

    QCOMPARE(includeGroups.at(0).size(), 2);
    QVERIFY(includeGroups.at(0).hasOnlyIncludesOfType(Client::IncludeLocal));

    QCOMPARE(includeGroups.at(1).size(), 2);
    QVERIFY(includeGroups.at(1).hasOnlyIncludesOfType(Client::IncludeGlobal));

    QCOMPARE(includeGroups.at(2).size(), 2);
    QVERIFY(includeGroups.at(2).hasOnlyIncludesOfType(Client::IncludeLocal));

    QCOMPARE(includeGroups.at(3).size(), 3);
    QVERIFY(includeGroups.at(3).hasOnlyIncludesOfType(Client::IncludeGlobal));
}

QObject *createIncludeGroupsTest()
{
    return new IncludeGroupsTest;
}

} // CppEditor::Internal

#include "includeutils.moc"

#endif // WITH_TESTS

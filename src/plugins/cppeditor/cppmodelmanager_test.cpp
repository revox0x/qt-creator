// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppmodelmanager_test.h"

#include "baseeditordocumentprocessor.h"
#include "builtineditordocumentparser.h"
#include "cpptoolstestcase.h"
#include "editordocumenthandle.h"
#include "modelmanagertesthelper.h"
#include "projectinfo.h"

#include <coreplugin/documentmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/fileutils.h>
#include <coreplugin/testdatadir.h>

#include <cplusplus/LookupContext.h>

#include <projectexplorer/kitmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/projectnodes.h>

#include <utils/fileutils.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>

#include <QDebug>
#include <QScopeGuard>
#include <QtTest>

#include <memory>

#define VERIFY_DOCUMENT_REVISION(document, expectedRevision) \
    QVERIFY(document); \
    QCOMPARE(document->revision(), expectedRevision);

using namespace ProjectExplorer;
using namespace Utils;

using CPlusPlus::Document;

// FIXME: Clean up the namespaces
using CppEditor::Tests::ModelManagerTestHelper;
using CppEditor::Tests::ProjectOpenerAndCloser;
using CppEditor::Tests::SourceFilesRefreshGuard;
using CppEditor::Tests::TemporaryCopiedDir;
using CppEditor::Tests::TemporaryDir;
using CppEditor::Tests::TestCase;
using CppEditor::Internal::Tests::VerifyCleanCppModelManager;

Q_DECLARE_METATYPE(CppEditor::ProjectFile)

namespace CppEditor::Internal {
namespace {

inline QString _(const QByteArray &ba) { return QString::fromLatin1(ba, ba.size()); }

class MyTestDataDir : public Core::Tests::TestDataDir
{
public:
    explicit MyTestDataDir(const QString &dir)
        : TestDataDir(_(SRCDIR "/../../../tests/cppmodelmanager/") + dir)
    {}

    QString includeDir(bool cleaned = true) const
    { return directory(_("include"), cleaned); }

    QString frameworksDir(bool cleaned = true) const
    { return directory(_("frameworks"), cleaned); }

    FilePath fileFromSourcesDir(const QString &fileName) const
    {
        return FilePath::fromString(directory(_("sources"))).pathAppended(fileName);
    }

    FilePath filePath(const QString &p) const
    {
        return FilePath::fromString(TestDataDir::file(p));
    }
};

FilePaths toAbsolutePaths(const QStringList &relativePathList,
                          const TemporaryCopiedDir &temporaryDir)
{
    FilePaths result;
    for (const QString &file : relativePathList)
        result << temporaryDir.absolutePath(file);
    return result;
}

// TODO: When possible, use this helper class in all tests
class ProjectCreator
{
public:
    explicit ProjectCreator(ModelManagerTestHelper *modelManagerTestHelper)
        : modelManagerTestHelper(modelManagerTestHelper)
    {}

    /// 'files' is expected to be a list of file names that reside in 'dir'.
    void create(const QString &name, const QString &dir, const QStringList &files)
    {
        const MyTestDataDir projectDir(dir);
        for (const QString &file : files)
            projectFiles << projectDir.filePath(file);

        RawProjectPart rpp;
        rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
        const ProjectFiles rppFiles = Utils::transform<ProjectFiles>(projectFiles,
                [](const FilePath &file) {
            return ProjectFile(file, ProjectFile::classify(file.toString()));
        });
        const auto project = modelManagerTestHelper->createProject(
                    name, Utils::FilePath::fromString(dir).pathAppended(name + ".pro"));

        const auto part = ProjectPart::create(project->projectFilePath(), rpp, {}, rppFiles);
        projectInfo = ProjectInfo::create(ProjectUpdateInfo(project, KitInfo(nullptr), {}, {}),
                                          {part});
    }

    ModelManagerTestHelper *modelManagerTestHelper;
    ProjectInfo::ConstPtr projectInfo;
    FilePaths projectFiles;
};

/// Changes a file on the disk and restores its original contents on destruction
class FileChangerAndRestorer
{
public:
    explicit FileChangerAndRestorer(const Utils::FilePath &filePath)
        : m_filePath(filePath)
    {
    }

    ~FileChangerAndRestorer()
    {
        restoreContents();
    }

    /// Saves the contents also internally so it can be restored on destruction
    bool readContents(QByteArray *contents)
    {
        Utils::FileReader fileReader;
        const bool isFetchOk = fileReader.fetch(m_filePath);
        if (isFetchOk) {
            m_originalFileContents = fileReader.data();
            if (contents)
                *contents = m_originalFileContents;
        }
        return isFetchOk;
    }

    bool writeContents(const QByteArray &contents) const
    {
        return TestCase::writeFile(m_filePath, contents);
    }

private:
    void restoreContents() const
    {
        TestCase::writeFile(m_filePath, m_originalFileContents);
    }

    QByteArray m_originalFileContents;
    const Utils::FilePath m_filePath;
};

} // anonymous namespace

static ProjectPart::ConstPtr projectPartOfEditorDocument(const FilePath &filePath)
{
    auto *editorDocument = CppModelManager::cppEditorDocument(filePath);
    QTC_ASSERT(editorDocument, return ProjectPart::ConstPtr());
    return editorDocument->processor()->parser()->projectPartInfo().projectPart;
}

/// Check: The preprocessor cleans include and framework paths.
void ModelManagerTest::testPathsAreClean()
{
    ModelManagerTestHelper helper;

    const MyTestDataDir testDataDir(_("testdata"));

    const auto project = helper.createProject(_("test_modelmanager_paths_are_clean"),
                                              Utils::FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
    rpp.setMacros({ProjectExplorer::Macro("OH_BEHAVE", "-1")});
    rpp.setHeaderPaths({HeaderPath::makeUser(testDataDir.includeDir(false)),
                        HeaderPath::makeFramework(testDataDir.frameworksDir(false))});
    const auto part = ProjectPart::create(project->projectFilePath(), rpp);
    const auto pi = ProjectInfo::create(ProjectUpdateInfo(project, KitInfo(nullptr), {}, {}),
                                        {part});

    CppModelManager::updateProjectInfo(pi);

    ProjectExplorer::HeaderPaths headerPaths = CppModelManager::headerPaths();
    QCOMPARE(headerPaths.size(), 2);
    QVERIFY(headerPaths.contains(HeaderPath::makeUser(testDataDir.includeDir())));
    QVERIFY(headerPaths.contains(HeaderPath::makeFramework(testDataDir.frameworksDir())));
}

/// Check: Frameworks headers are resolved.
void ModelManagerTest::testFrameworkHeaders()
{
    if (Utils::HostOsInfo::isWindowsHost())
        QSKIP("Can't resolve framework soft links on Windows.");

    ModelManagerTestHelper helper;

    const MyTestDataDir testDataDir(_("testdata"));

    const auto project = helper.createProject(_("test_modelmanager_framework_headers"),
                                              Utils::FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
    rpp.setMacros({{"OH_BEHAVE", "-1"}});
    rpp.setHeaderPaths({HeaderPath::makeUser(testDataDir.includeDir(false)),
                        HeaderPath::makeFramework(testDataDir.frameworksDir(false))});
    const FilePath source =
            testDataDir.fileFromSourcesDir("test_modelmanager_framework_headers.cpp");
    const auto part = ProjectPart::create(project->projectFilePath(), rpp, {},
                                          {ProjectFile(source, ProjectFile::CXXSource)});
    const auto pi = ProjectInfo::create(ProjectUpdateInfo(project, KitInfo(nullptr), {}, {}),
                                        {part});

    CppModelManager::updateProjectInfo(pi).waitForFinished();
    QCoreApplication::processEvents();

    QVERIFY(CppModelManager::snapshot().contains(source));
    Document::Ptr doc = CppModelManager::document(source);
    QVERIFY(!doc.isNull());
    CPlusPlus::Namespace *ns = doc->globalNamespace();
    QVERIFY(ns);
    QVERIFY(ns->memberCount() > 0);
    for (unsigned i = 0, ei = ns->memberCount(); i < ei; ++i) {
        CPlusPlus::Symbol *s = ns->memberAt(i);
        QVERIFY(s);
        QVERIFY(s->name());
        const CPlusPlus::Identifier *id = s->name()->asNameId();
        QVERIFY(id);
        QByteArray chars = id->chars();
        QVERIFY(chars.startsWith("success"));
    }
}

/// QTCREATORBUG-9056
/// Check: If the project configuration changes, all project files and their
///        includes have to be reparsed.
void ModelManagerTest::testRefreshAlsoIncludesOfProjectFiles()
{
    ModelManagerTestHelper helper;

    const MyTestDataDir testDataDir(_("testdata"));

    const FilePath testCpp = testDataDir.fileFromSourcesDir(_("test_modelmanager_refresh.cpp"));
    const FilePath testHeader = testDataDir.fileFromSourcesDir( _("test_modelmanager_refresh.h"));

    const auto project
            = helper.createProject(_("test_modelmanager_refresh_also_includes_of_project_files"),
                                   Utils::FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
    rpp.setMacros({{"OH_BEHAVE", "-1"}});
    rpp.setHeaderPaths({HeaderPath::makeUser(testDataDir.includeDir(false))});
    auto part = ProjectPart::create(project->projectFilePath(), rpp, {},
                                    {ProjectFile(testCpp, ProjectFile::CXXSource)});
    auto pi = ProjectInfo::create(ProjectUpdateInfo(project, KitInfo(nullptr), {}, {}), {part});

    QSet<FilePath> refreshedFiles = helper.updateProjectInfo(pi);
    QCOMPARE(refreshedFiles.size(), 1);
    QVERIFY(refreshedFiles.contains(testCpp));
    CPlusPlus::Snapshot snapshot = CppModelManager::snapshot();
    QVERIFY(snapshot.contains(testHeader));
    QVERIFY(snapshot.contains(testCpp));

    Document::Ptr headerDocumentBefore = snapshot.document(testHeader);
    const QList<CPlusPlus::Macro> macrosInHeaderBefore = headerDocumentBefore->definedMacros();
    QCOMPARE(macrosInHeaderBefore.size(), 1);
    QVERIFY(macrosInHeaderBefore.first().name() == "test_modelmanager_refresh_h");

    // Introduce a define that will enable another define once the document is reparsed.
    rpp.setMacros({{"TEST_DEFINE", "1"}});
    part = ProjectPart::create(project->projectFilePath(), rpp, {},
                               {ProjectFile(testCpp, ProjectFile::CXXSource)});
    pi = ProjectInfo::create(ProjectUpdateInfo(project, KitInfo(nullptr), {}, {}), {part});

    refreshedFiles = helper.updateProjectInfo(pi);

    QCOMPARE(refreshedFiles.size(), 1);
    QVERIFY(refreshedFiles.contains(testCpp));
    snapshot = CppModelManager::snapshot();
    QVERIFY(snapshot.contains(testHeader));
    QVERIFY(snapshot.contains(testCpp));

    Document::Ptr headerDocumentAfter = snapshot.document(testHeader);
    const QList<CPlusPlus::Macro> macrosInHeaderAfter = headerDocumentAfter->definedMacros();
    QCOMPARE(macrosInHeaderAfter.size(), 2);
    QVERIFY(macrosInHeaderAfter.at(0).name() == "test_modelmanager_refresh_h");
    QVERIFY(macrosInHeaderAfter.at(1).name() == "TEST_DEFINE_DEFINED");
}

/// QTCREATORBUG-9205
/// Check: When reparsing the same files again, no errors occur
///        (The CppSourceProcessor's already seen files are properly cleared!).
void ModelManagerTest::testRefreshSeveralTimes()
{
    ModelManagerTestHelper helper;

    const MyTestDataDir testDataDir(_("testdata_refresh"));

    const FilePath testHeader1 = testDataDir.filePath("defines.h");
    const FilePath testHeader2 = testDataDir.filePath("header.h");
    const FilePath testCpp = testDataDir.filePath("source.cpp");

    const auto project = helper.createProject(_("test_modelmanager_refresh_several_times"),
                                              Utils::FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
    const ProjectFiles files = {
        ProjectFile(testHeader1, ProjectFile::CXXHeader),
        ProjectFile(testHeader2, ProjectFile::CXXHeader),
        ProjectFile(testCpp, ProjectFile::CXXSource)
    };
    const auto part = ProjectPart::create(project->projectFilePath(), rpp, {}, files);
    auto pi = ProjectInfo::create(ProjectUpdateInfo(project, KitInfo(nullptr), {}, {}), {part});
    CppModelManager::updateProjectInfo(pi);

    CPlusPlus::Snapshot snapshot;
    QSet<FilePath> refreshedFiles;
    Document::Ptr document;

    ProjectExplorer::Macros macros = {{"FIRST_DEFINE"}};
    for (int i = 0; i < 2; ++i) {
        // Simulate project configuration change by having different defines each time.
        macros += {"ANOTHER_DEFINE"};
        rpp.setMacros(macros);
        const auto part = ProjectPart::create(project->projectFilePath(), rpp, {}, files);
        pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part});

        refreshedFiles = helper.updateProjectInfo(pi);
        QCOMPARE(refreshedFiles.size(), 3);

        QVERIFY(refreshedFiles.contains(testHeader1));
        QVERIFY(refreshedFiles.contains(testHeader2));
        QVERIFY(refreshedFiles.contains(testCpp));

        snapshot = CppModelManager::snapshot();
        QVERIFY(snapshot.contains(testHeader1));
        QVERIFY(snapshot.contains(testHeader2));
        QVERIFY(snapshot.contains(testCpp));

        // No diagnostic messages expected
        document = snapshot.document(testHeader1);
        QVERIFY(document->diagnosticMessages().isEmpty());

        document = snapshot.document(testHeader2);
        QVERIFY(document->diagnosticMessages().isEmpty());

        document = snapshot.document(testCpp);
        QVERIFY(document->diagnosticMessages().isEmpty());
    }
}

/// QTCREATORBUG-9581
/// Check: If nothing has changes, nothing should be reindexed.
void ModelManagerTest::testRefreshTestForChanges()
{
    ModelManagerTestHelper helper;

    const MyTestDataDir testDataDir(_("testdata_refresh"));
    const FilePath testCpp = testDataDir.filePath("source.cpp");

    const auto project = helper.createProject(_("test_modelmanager_refresh_2"),
                                              Utils::FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
    const auto part = ProjectPart::create(project->projectFilePath(), rpp, {},
                                          {ProjectFile(testCpp, ProjectFile::CXXSource)});
    const auto pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part});

    // Reindexing triggers a reparsing thread
    helper.resetRefreshedSourceFiles();
    QFuture<void> firstFuture = CppModelManager::updateProjectInfo(pi);
    QVERIFY(firstFuture.isStarted() || firstFuture.isRunning());
    firstFuture.waitForFinished();
    const QSet<FilePath> refreshedFiles = helper.waitForRefreshedSourceFiles();
    QCOMPARE(refreshedFiles.size(), 1);
    QVERIFY(refreshedFiles.contains(testCpp));

    // No reindexing since nothing has changed
    QFuture<void> subsequentFuture = CppModelManager::updateProjectInfo(pi);
    QVERIFY(subsequentFuture.isCanceled() && subsequentFuture.isFinished());
}

/// Check: (1) Added project files are recognized and parsed.
/// Check: (2) Removed project files are recognized and purged from the snapshot.
void ModelManagerTest::testRefreshAddedAndPurgeRemoved()
{
    ModelManagerTestHelper helper;

    const MyTestDataDir testDataDir(_("testdata_refresh"));

    const FilePath testHeader1 = testDataDir.filePath("header.h");
    const FilePath testHeader2 = testDataDir.filePath("defines.h");
    const FilePath testCpp = testDataDir.filePath("source.cpp");

    const auto project = helper.createProject(_("test_modelmanager_refresh_3"),
                                              Utils::FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
    const auto part = ProjectPart::create(project->projectFilePath(), rpp, {},
            {{testCpp, ProjectFile::CXXSource},
             {testHeader1, ProjectFile::CXXHeader}});
    auto pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part});

    CPlusPlus::Snapshot snapshot;
    QSet<FilePath> refreshedFiles;

    refreshedFiles = helper.updateProjectInfo(pi);

    QCOMPARE(refreshedFiles.size(), 2);
    QVERIFY(refreshedFiles.contains(testHeader1));
    QVERIFY(refreshedFiles.contains(testCpp));

    snapshot = CppModelManager::snapshot();
    QVERIFY(snapshot.contains(testHeader1));
    QVERIFY(snapshot.contains(testCpp));

    // Now add testHeader2 and remove testHeader1
    const auto newPart = ProjectPart::create(project->projectFilePath(), rpp, {},
            {{testCpp, ProjectFile::CXXSource},
             {testHeader2, ProjectFile::CXXHeader}});
    pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {newPart});

    refreshedFiles = helper.updateProjectInfo(pi);

    // Only the added project file was reparsed
    QCOMPARE(refreshedFiles.size(), 1);
    QVERIFY(refreshedFiles.contains(testHeader2));

    snapshot = CppModelManager::snapshot();
    QVERIFY(snapshot.contains(testHeader2));
    QVERIFY(snapshot.contains(testCpp));
    // The removed project file is not anymore in the snapshot
    QVERIFY(!snapshot.contains(testHeader1));
}

/// Check: Timestamp modified files are reparsed if project files are added or removed
///        while the project configuration stays the same
void ModelManagerTest::testRefreshTimeStampModifiedIfSourcefilesChange()
{
    QFETCH(QString, fileToChange);
    QFETCH(QStringList, initialProjectFiles);
    QFETCH(QStringList, finalProjectFiles);

    TemporaryCopiedDir temporaryDir(MyTestDataDir(QLatin1String("testdata_refresh2")).path());
    const FilePath filePath = temporaryDir.absolutePath(fileToChange);
    const FilePaths initialProjectFilePaths = toAbsolutePaths(initialProjectFiles, temporaryDir);
    const FilePaths finalProjectFilePaths = toAbsolutePaths(finalProjectFiles, temporaryDir);

    ModelManagerTestHelper helper;

    const auto project = helper.createProject(_("test_modelmanager_refresh_timeStampModified"),
                                              FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::Qt5);
    auto files = Utils::transform<ProjectFiles>(initialProjectFilePaths, [](const FilePath &f) {
        return ProjectFile(f, ProjectFile::CXXSource);
    });
    auto part = ProjectPart::create(project->projectFilePath(), rpp, {}, files);
    auto pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part});

    Document::Ptr document;
    CPlusPlus::Snapshot snapshot;
    QSet<FilePath> refreshedFiles;

    refreshedFiles = helper.updateProjectInfo(pi);

    QCOMPARE(refreshedFiles.size(), initialProjectFilePaths.size());
    snapshot = CppModelManager::snapshot();
    for (const FilePath &file : initialProjectFilePaths) {
        QVERIFY(refreshedFiles.contains(file));
        QVERIFY(snapshot.contains(file));
    }

    document = snapshot.document(filePath);
    const QDateTime lastModifiedBefore = document->lastModified();
    QCOMPARE(document->globalSymbolCount(), 1);
    QCOMPARE(document->globalSymbolAt(0)->name()->identifier()->chars(), "someGlobal");

    // Modify the file
    QTest::qSleep(1000); // Make sure the timestamp is different
    FileChangerAndRestorer fileChangerAndRestorer(filePath);
    QByteArray originalContents;
    QVERIFY(fileChangerAndRestorer.readContents(&originalContents));
    const QByteArray newFileContentes = originalContents + "\nint addedOtherGlobal;";
    QVERIFY(fileChangerAndRestorer.writeContents(newFileContentes));

    // Add or remove source file. The configuration stays the same.
    files = Utils::transform<ProjectFiles>(finalProjectFilePaths, [](const FilePath &f) {
        return ProjectFile(f, ProjectFile::CXXSource);
    });
    part = ProjectPart::create(project->projectFilePath(), rpp, {}, files);
    pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part});

    refreshedFiles = helper.updateProjectInfo(pi);

    QCOMPARE(refreshedFiles.size(), finalProjectFilePaths.size());
    snapshot = CppModelManager::snapshot();
    for (const FilePath &file : finalProjectFilePaths) {
        QVERIFY(refreshedFiles.contains(file));
        QVERIFY(snapshot.contains(file));
    }
    document = snapshot.document(filePath);
    const QDateTime lastModifiedAfter = document->lastModified();
    QVERIFY(lastModifiedAfter > lastModifiedBefore);
    QCOMPARE(document->globalSymbolCount(), 2);
    QCOMPARE(document->globalSymbolAt(0)->name()->identifier()->chars(), "someGlobal");
    QCOMPARE(document->globalSymbolAt(1)->name()->identifier()->chars(), "addedOtherGlobal");
}

void ModelManagerTest::testRefreshTimeStampModifiedIfSourcefilesChange_data()
{
    QTest::addColumn<QString>("fileToChange");
    QTest::addColumn<QStringList>("initialProjectFiles");
    QTest::addColumn<QStringList>("finalProjectFiles");

    const QString testCpp = QLatin1String("source.cpp");
    const QString testCpp2 = QLatin1String("source2.cpp");

    const QString fileToChange = testCpp;
    const QStringList projectFiles1 = {testCpp};
    const QStringList projectFiles2 = {testCpp, testCpp2};

    // Add a file
    QTest::newRow("case: add project file") << fileToChange << projectFiles1 << projectFiles2;

    // Remove a file
    QTest::newRow("case: remove project file") << fileToChange << projectFiles2 << projectFiles1;
}

/// Check: If a second project is opened, the code model is still aware of
///        files of the first project.
void ModelManagerTest::testSnapshotAfterTwoProjects()
{
    QSet<FilePath> refreshedFiles;
    ModelManagerTestHelper helper;
    ProjectCreator project1(&helper);
    ProjectCreator project2(&helper);

    // Project 1
    project1.create(_("test_modelmanager_snapshot_after_two_projects.1"),
                    _("testdata_project1"),
                    {"foo.h", "foo.cpp",  "main.cpp"});

    refreshedFiles = helper.updateProjectInfo(project1.projectInfo);
    QCOMPARE(refreshedFiles, Utils::toSet(project1.projectFiles));
    const int snapshotSizeAfterProject1 = CppModelManager::snapshot().size();

    for (const FilePath &file : std::as_const(project1.projectFiles))
        QVERIFY(CppModelManager::snapshot().contains(file));

    // Project 2
    project2.create(_("test_modelmanager_snapshot_after_two_projects.2"),
                    _("testdata_project2"),
                    {"bar.h", "bar.cpp",  "main.cpp"});

    refreshedFiles = helper.updateProjectInfo(project2.projectInfo);
    QCOMPARE(refreshedFiles, Utils::toSet(project2.projectFiles));

    const int snapshotSizeAfterProject2 = CppModelManager::snapshot().size();
    QVERIFY(snapshotSizeAfterProject2 > snapshotSizeAfterProject1);
    QVERIFY(snapshotSizeAfterProject2 >= snapshotSizeAfterProject1 + project2.projectFiles.size());

    for (const FilePath &file : std::as_const(project1.projectFiles))
        QVERIFY(CppModelManager::snapshot().contains(file));
    for (const FilePath &file : std::as_const(project2.projectFiles))
        QVERIFY(CppModelManager::snapshot().contains(file));
}

/// Check: (1) For a project with a *.ui file an AbstractEditorSupport object
///            is added for the ui_* file.
/// Check: (2) The CppSourceProcessor can successfully resolve the ui_* file
///            though it might not be actually generated in the build dir.
///

void ModelManagerTest::testExtraeditorsupportUiFiles()
{
    VerifyCleanCppModelManager verify;

    TemporaryCopiedDir temporaryDir(MyTestDataDir(QLatin1String("testdata_guiproject1")).path());
    QVERIFY(temporaryDir.isValid());
    const FilePath projectFile = temporaryDir.absolutePath("testdata_guiproject1.pro");

    ProjectOpenerAndCloser projects;
    QVERIFY(projects.open(projectFile, /*configureAsExampleProject=*/ true));

    // Check working copy.
    // An AbstractEditorSupport object should have been added for the ui_* file.
    WorkingCopy workingCopy = CppModelManager::workingCopy();

    QCOMPARE(workingCopy.size(), 2); // CppModelManager::configurationFileName() and "ui_*.h"

    QStringList fileNamesInWorkinCopy;
    const WorkingCopy::Table &elements = workingCopy.elements();
    for (auto it = elements.cbegin(), end = elements.cend(); it != end; ++it)
        fileNamesInWorkinCopy << Utils::FilePath::fromString(it.key().toString()).fileName();

    fileNamesInWorkinCopy.sort();
    const QString expectedUiHeaderFileName = _("ui_mainwindow.h");
    QCOMPARE(fileNamesInWorkinCopy.at(0), CppModelManager::configurationFileName().toString());
    QCOMPARE(fileNamesInWorkinCopy.at(1), expectedUiHeaderFileName);

    // Check CppSourceProcessor / includes.
    // The CppSourceProcessor is expected to find the ui_* file in the working copy.
    const FilePath fileIncludingTheUiFile = temporaryDir.absolutePath("mainwindow.cpp");
    while (!CppModelManager::snapshot().document(fileIncludingTheUiFile))
        QCoreApplication::processEvents();

    const CPlusPlus::Snapshot snapshot = CppModelManager::snapshot();
    const Document::Ptr document = snapshot.document(fileIncludingTheUiFile);
    QVERIFY(document);
    const FilePaths includedFiles = document->includedFiles();
    QCOMPARE(includedFiles.size(), 2);
    QCOMPARE(includedFiles.at(0).fileName(), _("mainwindow.h"));
    QCOMPARE(includedFiles.at(1).fileName(), _("ui_mainwindow.h"));
}

/// QTCREATORBUG-9828: Locator shows symbols of closed files
/// Check: The garbage collector should be run if the last CppEditor is closed.
void ModelManagerTest::testGcIfLastCppeditorClosed()
{
    ModelManagerTestHelper helper;

    MyTestDataDir testDataDirectory(_("testdata_guiproject1"));
    const FilePath file = testDataDirectory.filePath("main.cpp");

    helper.resetRefreshedSourceFiles();

    // Open a file in the editor
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 0);
    Core::IEditor *editor = Core::EditorManager::openEditor(file);
    QVERIFY(editor);
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 1);
    QVERIFY(CppModelManager::isCppEditor(editor));
    QVERIFY(CppModelManager::workingCopy().get(file));

    // Wait until the file is refreshed
    helper.waitForRefreshedSourceFiles();

    // Close file/editor
    Core::EditorManager::closeDocuments({editor->document()}, /*askAboutModifiedEditors=*/false);
    helper.waitForFinishedGc();

    // Check: File is removed from the snapshpt
    QVERIFY(!CppModelManager::workingCopy().get(file));
    QVERIFY(!CppModelManager::snapshot().contains(file));
}

/// Check: Files that are open in the editor are not garbage collected.
void ModelManagerTest::testDontGcOpenedFiles()
{
    ModelManagerTestHelper helper;

    MyTestDataDir testDataDirectory(_("testdata_guiproject1"));
    const FilePath file = testDataDirectory.filePath("main.cpp");

    helper.resetRefreshedSourceFiles();

    // Open a file in the editor
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 0);
    Core::IEditor *editor = Core::EditorManager::openEditor(file);
    QVERIFY(editor);
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 1);
    QVERIFY(CppModelManager::isCppEditor(editor));

    // Wait until the file is refreshed and check whether it is in the working copy
    helper.waitForRefreshedSourceFiles();

    QVERIFY(CppModelManager::workingCopy().get(file));

    // Run the garbage collector
    CppModelManager::GC();

    // Check: File is still there
    QVERIFY(CppModelManager::workingCopy().get(file));
    QVERIFY(CppModelManager::snapshot().contains(file));

    // Close editor
    Core::EditorManager::closeDocuments({editor->document()});
    helper.waitForFinishedGc();
    QVERIFY(CppModelManager::snapshot().isEmpty());
}

namespace {
struct EditorCloser {
    Core::IEditor *editor;
    explicit EditorCloser(Core::IEditor *editor): editor(editor) {}
    ~EditorCloser()
    {
        if (editor)
            QVERIFY(TestCase::closeEditorWithoutGarbageCollectorInvocation(editor));
    }
};

QString nameOfFirstDeclaration(const Document::Ptr &doc)
{
    if (doc && doc->globalNamespace()) {
        if (CPlusPlus::Symbol *s = doc->globalSymbolAt(0)) {
            if (CPlusPlus::Declaration *decl = s->asDeclaration()) {
                if (const CPlusPlus::Name *name = decl->name()) {
                    if (const CPlusPlus::Identifier *identifier = name->identifier())
                        return QString::fromUtf8(identifier->chars(), identifier->size());
                }
            }
        }
    }
    return QString();
}
}

void ModelManagerTest::testDefinesPerProject()
{
    ModelManagerTestHelper helper;

    MyTestDataDir testDataDirectory(_("testdata_defines"));
    const FilePath main1File = testDataDirectory.filePath("main1.cpp");
    const FilePath main2File = testDataDirectory.filePath("main2.cpp");
    const FilePath header = testDataDirectory.filePath("header.h");

    const auto project = helper.createProject(_("test_modelmanager_defines_per_project"),
                                              Utils::FilePath::fromString("blubb.pro"));

    RawProjectPart rpp1;
    rpp1.setProjectFileLocation("project1.projectfile");
    rpp1.setQtVersion(Utils::QtMajorVersion::None);
    rpp1.setMacros({{"SUB1"}});
    rpp1.setHeaderPaths({HeaderPath::makeUser(testDataDirectory.includeDir(false))});
    const auto part1 = ProjectPart::create(project->projectFilePath(), rpp1, {},
            {{main1File, ProjectFile::CXXSource}, {header, ProjectFile::CXXHeader}});

    RawProjectPart rpp2;
    rpp2.setProjectFileLocation("project1.projectfile");
    rpp2.setQtVersion(Utils::QtMajorVersion::None);
    rpp2.setMacros({{"SUB2"}});
    rpp2.setHeaderPaths({HeaderPath::makeUser(testDataDirectory.includeDir(false))});
    const auto part2 = ProjectPart::create(project->projectFilePath(), rpp2, {},
            {{main2File, ProjectFile::CXXSource}, {header, ProjectFile::CXXHeader}});

    const auto pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part1, part2});
    helper.updateProjectInfo(pi);
    QCOMPARE(CppModelManager::snapshot().size(), 4);

    // Open a file in the editor
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 0);

    struct Data {
        QString firstDeclarationName;
        FilePath filePath;
    } d[] = {
        {_("one"), main1File},
        {_("two"), main2File}
    };

    for (auto &i : d) {
        const QString firstDeclarationName = i.firstDeclarationName;

        Core::IEditor *editor = Core::EditorManager::openEditor(i.filePath);
        EditorCloser closer(editor);
        QVERIFY(editor);
        QCOMPARE(Core::DocumentModel::openedDocuments().size(), 1);
        QVERIFY(CppModelManager::isCppEditor(editor));

        Document::Ptr doc = CppModelManager::document(i.filePath);
        QCOMPARE(nameOfFirstDeclaration(doc), firstDeclarationName);
    }
}

void ModelManagerTest::testPrecompiledHeaders()
{
    ModelManagerTestHelper helper;

    MyTestDataDir testDataDirectory(_("testdata_defines"));
    const FilePath main1File = testDataDirectory.filePath("main1.cpp");
    const FilePath main2File = testDataDirectory.filePath("main2.cpp");
    const FilePath header = testDataDirectory.filePath("header.h");
    const FilePath pch1File = testDataDirectory.filePath("pch1.h");
    const FilePath pch2File = testDataDirectory.filePath("pch2.h");

    const auto project = helper.createProject(_("test_modelmanager_defines_per_project_pch"),
                                              Utils::FilePath::fromString("blubb.pro"));

    RawProjectPart rpp1;
    rpp1.setProjectFileLocation("project1.projectfile");
    rpp1.setQtVersion(Utils::QtMajorVersion::None);
    rpp1.setPreCompiledHeaders({pch1File.toString()});
    rpp1.setHeaderPaths({HeaderPath::makeUser(testDataDirectory.includeDir(false))});
    const auto part1 = ProjectPart::create(project->projectFilePath(), rpp1, {},
            {{main1File, ProjectFile::CXXSource}, {header, ProjectFile::CXXHeader}});

    RawProjectPart rpp2;
    rpp2.setProjectFileLocation("project2.projectfile");
    rpp2.setQtVersion(Utils::QtMajorVersion::None);
    rpp2.setPreCompiledHeaders({pch2File.toString()});
    rpp2.setHeaderPaths({HeaderPath::makeUser(testDataDirectory.includeDir(false))});
    const auto part2 = ProjectPart::create(project->projectFilePath(), rpp2, {},
            {{main2File, ProjectFile::CXXSource}, {header, ProjectFile::CXXHeader}});

    const auto pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part1, part2});

    helper.updateProjectInfo(pi);
    QCOMPARE(CppModelManager::snapshot().size(), 4);

    // Open a file in the editor
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 0);

    struct Data {
        QString firstDeclarationName;
        QString firstClassInPchFile;
        FilePath filePath;
    } d[] = {
        {_("one"), _("ClassInPch1"), main1File},
        {_("two"), _("ClassInPch2"), main2File}
    };
    for (auto &i : d) {
        const QString firstDeclarationName = i.firstDeclarationName;
        const QByteArray firstClassInPchFile = i.firstClassInPchFile.toUtf8();
        const FilePath filePath = i.filePath;

        Core::IEditor *editor = Core::EditorManager::openEditor(filePath);
        EditorCloser closer(editor);
        QVERIFY(editor);
        QCOMPARE(Core::DocumentModel::openedDocuments().size(), 1);
        QVERIFY(CppModelManager::isCppEditor(editor));

        auto parser = BuiltinEditorDocumentParser::get(filePath);
        QVERIFY(parser);
        BaseEditorDocumentParser::Configuration config = parser->configuration();
        config.usePrecompiledHeaders = true;
        parser->setConfiguration(config);
        parser->update({CppModelManager::workingCopy(), nullptr,Utils::Language::Cxx, false});

        // Check if defines from pch are considered
        Document::Ptr document = CppModelManager::document(filePath);
        QCOMPARE(nameOfFirstDeclaration(document), firstDeclarationName);

        // Check if declarations from pch are considered
        CPlusPlus::LookupContext context(document, parser->snapshot());
        const CPlusPlus::Identifier *identifier
            = document->control()->identifier(firstClassInPchFile.data());
        const QList<CPlusPlus::LookupItem> results = context.lookup(identifier,
                                                                    document->globalNamespace());
        QVERIFY(!results.isEmpty());
        QVERIFY(results.first().declaration()->type()->asClassType());
    }
}

void ModelManagerTest::testDefinesPerEditor()
{
    ModelManagerTestHelper helper;

    MyTestDataDir testDataDirectory(_("testdata_defines"));
    const FilePath main1File = testDataDirectory.filePath("main1.cpp");
    const FilePath main2File = testDataDirectory.filePath("main2.cpp");
    const FilePath header = testDataDirectory.filePath("header.h");

    const auto project = helper.createProject(_("test_modelmanager_defines_per_editor"),
                                              Utils::FilePath::fromString("blubb.pro"));

    RawProjectPart rpp1;
    rpp1.setQtVersion(Utils::QtMajorVersion::None);
    rpp1.setHeaderPaths({HeaderPath::makeUser(testDataDirectory.includeDir(false))});
    const auto part1 = ProjectPart::create(project->projectFilePath(), rpp1, {},
            {{main1File, ProjectFile::CXXSource}, {header, ProjectFile::CXXHeader}});

    RawProjectPart rpp2;
    rpp2.setQtVersion(Utils::QtMajorVersion::None);
    rpp2.setHeaderPaths({HeaderPath::makeUser(testDataDirectory.includeDir(false))});
    const auto part2 = ProjectPart::create(project->projectFilePath(), rpp2, {},
            {{main2File, ProjectFile::CXXSource}, {header, ProjectFile::CXXHeader}});

    const auto pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part1, part2});
    helper.updateProjectInfo(pi);

    QCOMPARE(CppModelManager::snapshot().size(), 4);

    // Open a file in the editor
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 0);

    struct Data {
        QString editorDefines;
        QString firstDeclarationName;
    } d[] = {
        {_("#define SUB1\n"), _("one")},
        {_("#define SUB2\n"), _("two")}
    };
    for (auto &i : d) {
        const QString editorDefines = i.editorDefines;
        const QString firstDeclarationName = i.firstDeclarationName;

        Core::IEditor *editor = Core::EditorManager::openEditor(main1File);
        EditorCloser closer(editor);
        QVERIFY(editor);
        QCOMPARE(Core::DocumentModel::openedDocuments().size(), 1);
        QVERIFY(CppModelManager::isCppEditor(editor));

        const FilePath filePath = editor->document()->filePath();
        const auto parser = BaseEditorDocumentParser::get(filePath);
        BaseEditorDocumentParser::Configuration config = parser->configuration();
        config.editorDefines = editorDefines.toUtf8();
        parser->setConfiguration(config);
        parser->update({CppModelManager::workingCopy(), nullptr, Utils::Language::Cxx, false});

        Document::Ptr doc = CppModelManager::document(main1File);
        QCOMPARE(nameOfFirstDeclaration(doc), firstDeclarationName);
    }
}

void ModelManagerTest::testUpdateEditorsAfterProjectUpdate()
{
    ModelManagerTestHelper helper;

    MyTestDataDir testDataDirectory(_("testdata_defines"));
    const FilePath fileA = testDataDirectory.filePath("main1.cpp"); // content not relevant
    const FilePath fileB = testDataDirectory.filePath("main2.cpp"); // content not relevant

    // Open file A in editor
    Core::IEditor *editorA = Core::EditorManager::openEditor(fileA);
    QVERIFY(editorA);
    EditorCloser closerA(editorA);
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 1);
    QVERIFY(TestCase::waitForProcessedEditorDocument(fileA));
    ProjectPart::ConstPtr documentAProjectPart = projectPartOfEditorDocument(fileA);
    QVERIFY(!documentAProjectPart->hasProject());

    // Open file B in editor
    Core::IEditor *editorB = Core::EditorManager::openEditor(fileB);
    QVERIFY(editorB);
    EditorCloser closerB(editorB);
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 2);
    QVERIFY(TestCase::waitForProcessedEditorDocument(fileB));
    ProjectPart::ConstPtr documentBProjectPart = projectPartOfEditorDocument(fileB);
    QVERIFY(!documentBProjectPart->hasProject());

    // Switch back to document A
    Core::EditorManager::activateEditor(editorA);

    // Open/update related project
    const auto project
            = helper.createProject(_("test_modelmanager_updateEditorsAfterProjectUpdate"),
                                   Utils::FilePath::fromString("blubb.pro"));
    RawProjectPart rpp;
    rpp.setQtVersion(Utils::QtMajorVersion::None);
    const auto part = ProjectPart::create(project->projectFilePath(), rpp, {},
        {{fileA, ProjectFile::CXXSource}, {fileB, ProjectFile::CXXSource}});
    const auto pi = ProjectInfo::create({project, KitInfo(nullptr), {}, {}}, {part});
    helper.updateProjectInfo(pi);

    // ... and check for updated editor document A
    QVERIFY(TestCase::waitForProcessedEditorDocument(fileA));
    documentAProjectPart = projectPartOfEditorDocument(fileA);
    QCOMPARE(documentAProjectPart->topLevelProject, pi->projectFilePath());

    // Switch back to document B and check if that's updated, too
    Core::EditorManager::activateEditor(editorB);
    QVERIFY(TestCase::waitForProcessedEditorDocument(fileB));
    documentBProjectPart = projectPartOfEditorDocument(fileB);
    QCOMPARE(documentBProjectPart->topLevelProject, pi->projectFilePath());
}

void ModelManagerTest::testRenameIncludes_data()
{
    QTest::addColumn<QString>("oldRelPath");
    QTest::addColumn<QString>("newRelPath");
    QTest::addColumn<bool>("successExpected");

    QTest::addRow("rename in place 1")
        << "subdir1/header1.h" << "subdir1/header1_renamed.h" << true;
    QTest::addRow("rename in place 2")
        << "subdir2/header2.h" << "subdir2/header2_renamed.h" << true;
    QTest::addRow("rename in place 3") << "header.h" << "header_renamed.h" << true;
    QTest::addRow("move up") << "subdir1/header1.h" << "header1_moved.h" << true;
    QTest::addRow("move up (breaks build)") << "subdir2/header2.h" << "header2_moved.h" << false;
    QTest::addRow("move down") << "header.h" << "subdir1/header_moved.h" << true;
    QTest::addRow("move across") << "subdir1/header1.h" << "subdir2/header1_moved.h" << true;
    QTest::addRow("move across (breaks build)")
        << "subdir2/header2.h" << "subdir1/header2_moved.h" << false;
}

void ModelManagerTest::testRenameIncludes()
{
    // Set up project.
    TemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const MyTestDataDir sourceDir("testdata_renameheaders");
    const FilePath srcFilePath = FilePath::fromString(sourceDir.path());
    const FilePath projectDir = tmpDir.filePath().pathAppended(srcFilePath.fileName());
    const auto copyResult = srcFilePath.copyRecursively(projectDir);
    if (!copyResult)
        qDebug() << copyResult.error();
    QVERIFY(copyResult);
    Kit * const kit  = Utils::findOr(KitManager::kits(), nullptr, [](const Kit *k) {
        return k->isValid() && !k->hasWarning() && k->value("QtSupport.QtInformation").isValid();
    });
    if (!kit)
        QSKIP("The test requires at least one valid kit with a valid Qt");
    const FilePath projectFile = projectDir.pathAppended(projectDir.fileName() + ".pro");
    SourceFilesRefreshGuard refreshGuard;
    ProjectOpenerAndCloser projectMgr;
    const ProjectInfo::ConstPtr projectInfo = projectMgr.open(projectFile, true, kit);
    QVERIFY(projectInfo);
    QVERIFY(refreshGuard.wait());

    // Verify initial code model state.
    const auto makeAbs = [&](const QStringList &relPaths) {
        return Utils::transform<QSet<FilePath>>(relPaths, [&](const QString &relPath) {
            return projectDir.pathAppended(relPath);
        });
    };
    const QSet<FilePath> allSources = makeAbs({"main.cpp", "subdir1/file1.cpp", "subdir2/file2.cpp"});
    const QSet<FilePath> allHeaders = makeAbs({"header.h", "subdir1/header1.h", "subdir2/header2.h"});
    QCOMPARE(projectInfo->sourceFiles(), allSources + allHeaders);
    CPlusPlus::Snapshot snapshot = CppModelManager::snapshot();
    for (const FilePath &srcFile : allSources) {
        QCOMPARE(snapshot.allIncludesForDocument(srcFile), allHeaders);
    }

    // Rename the header.
    QFETCH(QString, oldRelPath);
    QFETCH(QString, newRelPath);
    QFETCH(bool, successExpected);
    const FilePath oldHeader = projectDir.pathAppended(oldRelPath);
    const FilePath newHeader = projectDir.pathAppended(newRelPath);
    refreshGuard.reset();
    QVERIFY(ProjectExplorerPlugin::renameFile(oldHeader, newHeader));

    // Verify new code model state.
    QVERIFY(refreshGuard.wait());
    QSet<FilePath> incompleteNewHeadersSet = allHeaders;
    incompleteNewHeadersSet.remove(oldHeader);
    QSet<FilePath> completeNewHeadersSet = incompleteNewHeadersSet;
    completeNewHeadersSet << newHeader;

    snapshot = CppModelManager::snapshot();
    for (const FilePath &srcFile : allSources) {
        const QSet<FilePath> &expectedHeaders = srcFile.fileName() == "main.cpp" && !successExpected
            ? incompleteNewHeadersSet : completeNewHeadersSet;
        QCOMPARE(snapshot.allIncludesForDocument(srcFile), expectedHeaders);
    }
}

void ModelManagerTest::testMoveIncludingSources_data()
{
    QTest::addColumn<QString>("oldRelPath");
    QTest::addColumn<QString>("newRelPath");

    QTest::addRow("move up") << "subdir1/file1.cpp" << "file1_moved.cpp";
    QTest::addRow("move down") << "main.cpp" << "subdir1/main.cpp";
    QTest::addRow("move across") << "subdir1/file1.cpp" << "subdir2/file1_moved.cpp";
}

void ModelManagerTest::testMoveIncludingSources()
{
    QFETCH(QString, oldRelPath);
    QFETCH(QString, newRelPath);

    // Set up project.
    TemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const MyTestDataDir sourceDir("testdata_renameheaders");
    const FilePath srcFilePath = FilePath::fromString(sourceDir.path());
    const FilePath projectDir = tmpDir.filePath().pathAppended(srcFilePath.fileName());
    const auto copyResult = srcFilePath.copyRecursively(projectDir);
    if (!copyResult)
        qDebug() << copyResult.error();
    QVERIFY(copyResult);
    Kit * const kit  = Utils::findOr(KitManager::kits(), nullptr, [](const Kit *k) {
        return k->isValid() && !k->hasWarning() && k->value("QtSupport.QtInformation").isValid();
    });
    if (!kit)
        QSKIP("The test requires at least one valid kit with a valid Qt");
    SourceFilesRefreshGuard refreshGuard;
    const FilePath projectFile = projectDir.pathAppended(projectDir.fileName() + ".pro");
    ProjectOpenerAndCloser projectMgr;
    QVERIFY(projectMgr.open(projectFile, true, kit));
    QVERIFY(refreshGuard.wait());

    // Verify initial code model state.
    const auto makeAbs = [&](const QStringList &relPaths) {
        return Utils::transform<QSet<FilePath>>(relPaths, [&](const QString &relPath) {
            return projectDir.pathAppended(relPath);
        });
    };
    const FilePath oldSource = projectDir.pathAppended(oldRelPath);
    QVERIFY(oldSource.exists());
    const QSet<FilePath> includedHeaders = makeAbs(
        {"header.h", "subdir1/header1.h", "subdir2/header2.h"});
    QCOMPARE(CppModelManager::snapshot().allIncludesForDocument(oldSource), includedHeaders);

    // Rename the source file.
    refreshGuard.reset();
    const FilePath newSource = projectDir.pathAppended(newRelPath);
    QVERIFY(ProjectExplorerPlugin::renameFile(oldSource, newSource, projectMgr.projects().first()));

    // Verify new code model state.
    QVERIFY(refreshGuard.wait());
    QCOMPARE(CppModelManager::snapshot().allIncludesForDocument(newSource), includedHeaders);
}

void ModelManagerTest::testRenameIncludesInEditor()
{
    struct ModelManagerGCHelper {
        ~ModelManagerGCHelper() { CppModelManager::GC(); }
    } GCHelper;
    Q_UNUSED(GCHelper) // do not warn about being unused

    TemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    const QDir workingDir(tmpDir.path());
    const QStringList fileNames = {"baz.h", "baz2.h", "baz3.h", "foo.h", "foo.cpp", "main.cpp"};
    const FilePath headerWithPragmaOnce = FilePath::fromString(workingDir.filePath("foo.h"));
    const FilePath renamedHeaderWithPragmaOnce = FilePath::fromString(workingDir.filePath("bar.h"));
    const QString headerWithNormalGuard(workingDir.filePath(_("baz.h")));
    const QString renamedHeaderWithNormalGuard(workingDir.filePath(_("foobar2000.h")));
    const QString headerWithUnderscoredGuard(workingDir.filePath(_("baz2.h")));
    const QString renamedHeaderWithUnderscoredGuard(workingDir.filePath(_("foobar4000.h")));
    const QString headerWithMalformedGuard(workingDir.filePath(_("baz3.h")));
    const QString renamedHeaderWithMalformedGuard(workingDir.filePath(_("foobar5000.h")));
    const FilePath mainFile = FilePath::fromString(workingDir.filePath("main.cpp"));
    const MyTestDataDir testDir(_("testdata_project1"));

    ModelManagerTestHelper helper;
    helper.resetRefreshedSourceFiles();

    // Copy test files to a temporary directory
    QSet<FilePath> sourceFiles;
    for (const QString &fileName : fileNames) {
        const QString &file = workingDir.filePath(fileName);
        QVERIFY(QFile::copy(testDir.file(fileName), file));
        // Saving source file names for the model manager update,
        // so we can update just the relevant files.
        if (ProjectFile::classify(file) == ProjectFile::CXXSource)
            sourceFiles.insert(FilePath::fromString(file));
    }

    // Update the c++ model manager and check for the old includes
    CppModelManager::updateSourceFiles(sourceFiles).waitForFinished();
    QCoreApplication::processEvents();
    CPlusPlus::Snapshot snapshot = CppModelManager::snapshot();
    for (const FilePath &sourceFile : std::as_const(sourceFiles)) {
        QCOMPARE(snapshot.allIncludesForDocument(sourceFile),
                 QSet<FilePath>{headerWithPragmaOnce});
    }

    // Open a file in the editor
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 0);
    Core::IEditor *editor = Core::EditorManager::openEditor(mainFile);
    QVERIFY(editor);
    EditorCloser editorCloser(editor);
    const QScopeGuard cleanup([] { Core::DocumentManager::saveAllModifiedDocumentsSilently(); });
    QCOMPARE(Core::DocumentModel::openedDocuments().size(), 1);
    QVERIFY(CppModelManager::isCppEditor(editor));
    QVERIFY(CppModelManager::workingCopy().get(mainFile));

    // Test the renaming of a header file where a pragma once guard is present
    QVERIFY(ProjectExplorerPlugin::renameFile(headerWithPragmaOnce, renamedHeaderWithPragmaOnce));

    // Test the renaming the header with include guard:
    // The contents should match the foobar2000.h in the testdata_project2 project
    QVERIFY(ProjectExplorerPlugin::renameFile(FilePath::fromString(headerWithNormalGuard),
                                              FilePath::fromString(renamedHeaderWithNormalGuard)));

    const MyTestDataDir testDir2(_("testdata_project2"));
    QFile foobar2000Header(testDir2.file("foobar2000.h"));
    QVERIFY(foobar2000Header.open(QFile::ReadOnly | QFile::Text));
    const auto foobar2000HeaderContents = foobar2000Header.readAll();
    foobar2000Header.close();

    QFile renamedHeader(renamedHeaderWithNormalGuard);
    QVERIFY(renamedHeader.open(QFile::ReadOnly | QFile::Text));
    auto renamedHeaderContents = renamedHeader.readAll();
    renamedHeader.close();
    QCOMPARE(renamedHeaderContents, foobar2000HeaderContents);

    // Test the renaming the header with underscore pre/suffixed include guard:
    // The contents should match the foobar2000.h in the testdata_project2 project
    QVERIFY(
        Core::FileUtils::renameFile(Utils::FilePath::fromString(headerWithUnderscoredGuard),
                                    Utils::FilePath::fromString(renamedHeaderWithUnderscoredGuard),
                                    Core::HandleIncludeGuards::Yes));

    QFile foobar4000Header(testDir2.file("foobar4000.h"));
    QVERIFY(foobar4000Header.open(QFile::ReadOnly | QFile::Text));
    const auto foobar4000HeaderContents = foobar4000Header.readAll();
    foobar4000Header.close();

    renamedHeader.setFileName(renamedHeaderWithUnderscoredGuard);
    QVERIFY(renamedHeader.open(QFile::ReadOnly | QFile::Text));
    renamedHeaderContents = renamedHeader.readAll();
    renamedHeader.close();
    QCOMPARE(renamedHeaderContents, foobar4000HeaderContents);

    // test the renaming of a header with a malformed guard to verify we do not make
    // accidental refactors
    renamedHeader.setFileName(headerWithMalformedGuard);
    QVERIFY(renamedHeader.open(QFile::ReadOnly | QFile::Text));
    auto originalMalformedGuardContents = renamedHeader.readAll();
    renamedHeader.close();

    QVERIFY(Core::FileUtils::renameFile(Utils::FilePath::fromString(headerWithMalformedGuard),
                                        Utils::FilePath::fromString(renamedHeaderWithMalformedGuard),
                                        Core::HandleIncludeGuards::Yes));

    renamedHeader.setFileName(renamedHeaderWithMalformedGuard);
    QVERIFY(renamedHeader.open(QFile::ReadOnly | QFile::Text));
    renamedHeaderContents = renamedHeader.readAll();
    renamedHeader.close();
    QCOMPARE(renamedHeaderContents, originalMalformedGuardContents);

    // Update the c++ model manager again and check for the new includes
    TestCase::waitForProcessedEditorDocument(mainFile);
    CppModelManager::updateSourceFiles(sourceFiles).waitForFinished();
    QCoreApplication::processEvents();
    snapshot = CppModelManager::snapshot();
    for (const FilePath &sourceFile : std::as_const(sourceFiles)) {
        QCOMPARE(snapshot.allIncludesForDocument(sourceFile),
                 QSet<FilePath>{renamedHeaderWithPragmaOnce});
    }
}

void ModelManagerTest::testDocumentsAndRevisions()
{
    TestCase helper;

    // Index two files
    const MyTestDataDir testDir(_("testdata_project1"));
    const FilePath filePath1 = testDir.filePath(QLatin1String("foo.h"));
    const FilePath filePath2 = testDir.filePath(QLatin1String("foo.cpp"));
    const QSet<FilePath> filesToIndex = {filePath1,filePath2};
    QVERIFY(TestCase::parseFiles(filesToIndex));

    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath1), 1U);
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath2), 1U);

    // Open editor for file 1
    TextEditor::BaseTextEditor *editor1;
    QVERIFY(helper.openCppEditor(filePath1, &editor1));
    helper.closeEditorAtEndOfTestCase(editor1);
    QVERIFY(TestCase::waitForProcessedEditorDocument(filePath1));
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath1), 2U);
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath2), 1U);

    // Index again
    QVERIFY(TestCase::parseFiles(filesToIndex));
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath1), 3U);
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath2), 2U);

    // Open editor for file 2
    TextEditor::BaseTextEditor *editor2;
    QVERIFY(helper.openCppEditor(filePath2, &editor2));
    helper.closeEditorAtEndOfTestCase(editor2);
    QVERIFY(TestCase::waitForProcessedEditorDocument(filePath2));
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath1), 3U);
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath2), 3U);

    // Index again
    QVERIFY(TestCase::parseFiles(filesToIndex));
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath1), 4U);
    VERIFY_DOCUMENT_REVISION(CppModelManager::document(filePath2), 4U);
}

void ModelManagerTest::testSettingsChanges()
{
    ModelManagerTestHelper helper;

    int refreshCount = 0;
    QSet<QString> refreshedFiles;
    connect(CppModelManager::instance(), &CppModelManager::sourceFilesRefreshed,
            &helper, [&](const QSet<QString> &files) {
        ++refreshCount;
        refreshedFiles.unite(files);
    });
    const auto waitForRefresh = [] {
        return ::CppEditor::Tests::waitForSignalOrTimeout(CppModelManager::instance(),
                                                          &CppModelManager::sourceFilesRefreshed,
                                                          5000);
    };

    const auto setupProjectNodes = [](Project &p, const ProjectFiles &projectFiles) {
        auto rootNode = std::make_unique<ProjectNode>(p.projectFilePath());
        for (const ProjectFile &sourceFile : projectFiles) {
            rootNode->addNestedNode(std::make_unique<FileNode>(sourceFile.path,
                                                               sourceFile.isHeader()
                                                               ? FileType::Header
                                                                   : FileType::Source));
        }
        p.setRootProjectNode(std::move(rootNode));
    };

    // Set up projects.
    const MyTestDataDir p1Dir("testdata_project1");
    const FilePaths p1Files
        = Utils::transform({"baz.h", "baz2.h", "baz3.h", "foo.cpp", "foo.h", "main.cpp"},
                           [&](const QString &fn) { return p1Dir.filePath(fn); });
    const ProjectFiles p1ProjectFiles = Utils::transform(p1Files, [](const FilePath &fp) {
        return ProjectFile(fp, ProjectFile::classify(fp.toString()));
    });
    Project * const p1 = helper.createProject("testdata_project1", FilePath::fromString("p1.pro"));
    setupProjectNodes(*p1, p1ProjectFiles);
    RawProjectPart rpp1;
    const auto part1 = ProjectPart::create(p1->projectFilePath(), rpp1, {}, p1ProjectFiles);
    const auto pi1 = ProjectInfo::create(ProjectUpdateInfo(p1, KitInfo(nullptr), {}, {}), {part1});
    const auto p1Sources = Utils::transform<QSet<QString>>(p1Files, &FilePath::toString);
    CppModelManager::updateProjectInfo(pi1);

    const MyTestDataDir p2Dir("testdata_project2");
    const FilePaths p2Files
        = Utils::transform({"bar.h", "bar.cpp", "foobar2000.h", "foobar4000.h", "main.cpp"},
                           [&](const QString &fn) { return p1Dir.filePath(fn); });
    const ProjectFiles p2ProjectFiles = Utils::transform(p2Files, [](const FilePath &fp) {
        return ProjectFile(fp, ProjectFile::classify(fp.toString()));
    });
    Project * const p2 = helper.createProject("testdata_project2", FilePath::fromString("p2.pro"));
    setupProjectNodes(*p2, p2ProjectFiles);
    RawProjectPart rpp2;
    const auto part2 = ProjectPart::create(p2->projectFilePath(), rpp2, {}, p2ProjectFiles);
    const auto pi2 = ProjectInfo::create(ProjectUpdateInfo(p2, KitInfo(nullptr), {}, {}), {part2});
    const auto p2Sources = Utils::transform<QSet<QString>>(p2Files, &FilePath::toString);
    CppModelManager::updateProjectInfo(pi2);

    // Initial check: Have all files been indexed?
    while (refreshCount < 2)
        QVERIFY(waitForRefresh());
    const auto allSources = p1Sources + p2Sources;
    QCOMPARE(refreshedFiles, allSources);

    // Switch first project from global to local settings. Nothing should get re-indexed,
    // as the default values are the same.
    refreshCount = 0;
    refreshedFiles.clear();
    QVERIFY(!CppCodeModelSettings::hasCustomSettings(p1));
    CppCodeModelSettings p1Settings = CppCodeModelSettings::settingsForProject(p1);
    CppCodeModelSettings::setSettingsForProject(p1, p1Settings);
    QVERIFY(CppCodeModelSettings::hasCustomSettings(p1));
    QCOMPARE(refreshCount, 0);
    QVERIFY(!waitForRefresh());

    // Change global settings. Only the second project should get re-indexed, as the first one
    // has its own settings, which are still the same.
    CppCodeModelSettings globalSettings = CppCodeModelSettings::settingsForProject(nullptr);
    globalSettings.indexerFileSizeLimitInMb = 1;
    CppCodeModelSettings::setGlobal(globalSettings);
    if (refreshCount == 0)
        QVERIFY(waitForRefresh());
    QVERIFY(!waitForRefresh());
    QCOMPARE(refreshedFiles, p2Sources);

    // Change first project's settings. Only this project should get re-indexed.
    refreshCount = 0;
    refreshedFiles.clear();
    p1Settings.ignoreFiles = true;
    p1Settings.ignorePattern = "baz3.h";
    CppCodeModelSettings::setSettingsForProject(p1, p1Settings);
    if (refreshCount == 0)
        QVERIFY(waitForRefresh());
    QVERIFY(!waitForRefresh());
    QSet<QString> filteredP1Sources = p1Sources;
    filteredP1Sources -= p1Dir.filePath("baz3.h").toString();
    QCOMPARE(refreshedFiles, filteredP1Sources);
}

static bool True = true;
static bool False = false;

void ModelManagerTest::testOptionalIndexing_data()
{
    QTest::addColumn<bool>("enableGlobally");
    QTest::addColumn<bool *>("enableForP1");
    QTest::addColumn<bool *>("enableForP2");
    QTest::addColumn<bool>("foo1Present");
    QTest::addColumn<bool>("foo2Present");

    QTest::addRow("globally disabled, no custom settings")
        << false << (bool *) nullptr << (bool *) nullptr << false << false;
    QTest::addRow("globally disabled, redundantly disabled for project 2")
        << false << (bool *) nullptr << &False << false << false;
    QTest::addRow("globally disabled, enabled for project 2")
        << false << (bool *) nullptr << &True << false << true;
    QTest::addRow("globally disabled, redundantly disabled for project 1")
        << false << &False << (bool *) nullptr << false << false;
    QTest::addRow("globally disabled, redundantly disabled for both projects")
        << false << &False << &False << false << false;
    QTest::addRow("globally disabled, redundantly disabled for project 1, enabled for project 2")
        << false << &False << &True << false << true;
    QTest::addRow("globally disabled, enabled for project 1")
        << false << &True << (bool *) nullptr << true << false;
    QTest::addRow("globally disabled, enabled for project 1, redundantly disabled for project 2")
        << false << &True << &False << true << false;
    QTest::addRow("globally disabled, enabled for both project")
        << false << &True << &True << true << true;
    QTest::addRow("globally enabled, no custom settings")
        << true << (bool *) nullptr << (bool *) nullptr << true << true ;
    QTest::addRow("globally enabled, disabled for project 2")
        << true << (bool *) nullptr << &False << true << false;
    QTest::addRow("globally enabled, redundantly enabled for project 2")
        << true << (bool *) nullptr << &True << true << true;
    QTest::addRow("globally enabled, disabled for project 1")
        << true << &False << (bool *) nullptr << false << true;
    QTest::addRow("globally enabled, disabled for both projects")
        << true << &False << &False << false << false;
    QTest::addRow("globally enabled, disabled for project 1, redundantly enabled for project 2")
        << true << &False << &True << false << true;
    QTest::addRow("globally enabled, redundantly enabled for project 1")
        << true << &True << (bool *) nullptr << true << true;
    QTest::addRow("globally enabled, redundantly enabled for project 1, disabled for project 2")
        << true << &True << &False << true << false;
    QTest::addRow("globally enabled, redundantly enabled for both projects")
        << true << &True << &True << true << true;
}

void ModelManagerTest::testOptionalIndexing()
{
    if (CppModelManager::isClangCodeModelActive())
        QSKIP("Test only makes sense with built-in locators");

    QFETCH(bool, enableGlobally);
    QFETCH(bool *, enableForP1);
    QFETCH(bool *, enableForP2);
    QFETCH(bool, foo1Present);
    QFETCH(bool, foo2Present);

    // Apply global setting, if necessary. Needs to be reverted in the end.
    class TempIndexingDisabler {
    public:
        TempIndexingDisabler(bool enable)
        {
            if (!enable)
                reset(false);
        }
        ~TempIndexingDisabler() { reset(true); }
    private:
        void reset(bool enable)
        {
            CppCodeModelSettings settings = CppCodeModelSettings::global();
            settings.enableIndexing = enable;
            CppCodeModelSettings::setGlobal(settings);
        }
    };
    const TempIndexingDisabler disabler(enableGlobally);

    // Set up projects.
    TemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const MyTestDataDir sourceDir("testdata_optionalindexing");
    const FilePath srcFilePath = FilePath::fromString(sourceDir.path());
    const FilePath projectDir = tmpDir.filePath().pathAppended(srcFilePath.fileName());
    const auto copyResult = srcFilePath.copyRecursively(projectDir);
    if (!copyResult)
        qDebug() << copyResult.error();
    QVERIFY(copyResult);
    Kit * const kit  = Utils::findOr(KitManager::kits(), nullptr, [](const Kit *k) {
        return k->isValid() && !k->hasWarning() && k->value("QtSupport.QtInformation").isValid();
    });
    if (!kit)
        QSKIP("The test requires at least one valid kit with a valid Qt");
    const FilePath p1ProjectFile = projectDir.pathAppended("lib1.pro");
    auto projectMgr = std::make_unique<ProjectOpenerAndCloser>();
    SourceFilesRefreshGuard refreshGuard;
    QVERIFY(projectMgr->open(p1ProjectFile, true, kit));
    QVERIFY(refreshGuard.wait());
    refreshGuard.reset();
    Project *p1 = projectMgr->projects().first();
    const FilePath p2ProjectFile = projectDir.pathAppended("lib2.pro");
    QVERIFY(projectMgr->open(p2ProjectFile, true, kit));
    QVERIFY(refreshGuard.wait());
    refreshGuard.reset();
    Project *p2 = projectMgr->projects().last();

    const auto applyProjectSpecificSettings = [&](Project *p, bool *enable) {
        if (!enable)
            return;
        refreshGuard.reset();
        CppCodeModelSettings settings = CppCodeModelSettings::settingsForProject(p);
        settings.enableIndexing = *enable;
        CppCodeModelSettings::setSettingsForProject(p, settings);
        if (*enable != enableGlobally)
            QVERIFY(refreshGuard.wait());
    };
    applyProjectSpecificSettings(p1, enableForP1);
    applyProjectSpecificSettings(p2, enableForP2);

    // Compare locator results to expectations.
    Core::LocatorFilterEntries entries = Core::LocatorMatcher::runBlocking(
        Core::LocatorMatcher::matchers(Core::MatcherType::Functions), "foo");
    const auto hasEntry = [&](const QString &name) {
        return Utils::contains(entries, [&](const Core::LocatorFilterEntry &e) {
            return e.displayName == name + "()";
        });
    };
    QCOMPARE(hasEntry("foo1"), foo1Present);
    QCOMPARE(hasEntry("foo2"), foo2Present);

    // Close and re-open projects, then check again, to see whether the settings persisted
    // and are taking effect.
    projectMgr.reset(nullptr);
    projectMgr.reset(new ProjectOpenerAndCloser);
    refreshGuard.reset();
    QVERIFY(projectMgr->open(p1ProjectFile, true, kit));
    p1 = projectMgr->projects().first();
    QCOMPARE(
        CppCodeModelSettings::settingsForProject(p1).enableIndexing,
        enableForP1 ? *enableForP1 : enableGlobally);
    QVERIFY(refreshGuard.wait());
    refreshGuard.reset();
    QVERIFY(projectMgr->open(p2ProjectFile, true, kit));
    p2 = projectMgr->projects().last();
    QCOMPARE(
        CppCodeModelSettings::settingsForProject(p2).enableIndexing,
        enableForP2 ? *enableForP2 : enableGlobally);
    QVERIFY(refreshGuard.wait());

    entries = Core::LocatorMatcher::runBlocking(
        Core::LocatorMatcher::matchers(Core::MatcherType::Functions), "foo");
    QCOMPARE(hasEntry("foo1"), foo1Present);
    QCOMPARE(hasEntry("foo2"), foo2Present);
}

} // CppEditor::Internal

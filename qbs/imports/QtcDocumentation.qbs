import qbs.FileInfo

Product {
    builtByDefault: false
    type: [isOnlineDoc ? "qdoc-output" : "qch"]

    Depends { name: "Qt.core" }
    Depends { name: "qtc" }

    property bool isOnlineDoc
    property path mainDocConfFile
    property string versionTag: qtc.qtcreator_version.replace(/\.|-/g, "")

    Qt.core.qdocEnvironment: [
        "GENERATED_ATTRIBUTIONS_DIR=" + product.buildDirectory,
        "IDE_DISPLAY_NAME=" + qtc.ide_display_name,
        "IDE_CASED_ID=" + qtc.ide_cased_id,
        "IDE_ID=" + qtc.ide_id,
        "QTCREATOR_COPYRIGHT_YEAR=" + qtc.qtcreator_copyright_year,
        "QTC_VERSION=" + qtc.qtcreator_version,
        "QTC_VERSION_TAG=" + qtc.qtcreator_version,
        "QT_INSTALL_DOCS=" + Qt.core.docPath,
        "QDOC_INDEX_DIR=" + Qt.core.docPath,
        "SRCDIR=" + sourceDirectory,
        "VERSION_TAG=" + versionTag,
    ]

    Group {
        name: "main qdocconf file"
        prefix: product.sourceDirectory + '/'
        files: mainDocConfFile
        fileTags: "qdocconf-main"
    }

    Group {
        fileTagsFilter: "qch"
        qbs.install: !qbs.targetOS.contains("macos")
        qbs.installDir: qtc.ide_doc_path
    }
}

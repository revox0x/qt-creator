import qbs 1.0

QtcPlugin {
    name: "BinEditor"

    Depends { name: "Qt.widgets" }
    Depends { name: "Aggregation" }
    Depends { name: "Utils" }

    Depends { name: "Core" }
    Depends { name: "TextEditor" }

    files: [
        "bineditor_global.h",
        "bineditortr.h",
        "bineditorplugin.cpp",
        "bineditorservice.h",
        "markup.cpp",
        "markup.h",
    ]
}


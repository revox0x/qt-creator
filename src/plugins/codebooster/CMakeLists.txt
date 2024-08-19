# cmake_minimum_required(VERSION 3.10)

# project(CodeBooster)

# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTORCC ON)
# set(CMAKE_AUTOUIC ON)
# set(CMAKE_CXX_STANDARD 17)
# set(QtCreator_DIR "C:/Qt/QtCreator/qtcreator-windows-x64-msvc-11.0.0/lib/cmake/QtCreator")

# find_package(QtCreator REQUIRED COMPONENTS Core)
# find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
# find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
# set(QtX Qt${QT_VERSION_MAJOR})

# # Add a CMake option that enables building your plugin with tests.
# # You don't want your released plugin binaries to contain tests,
# # so make that default to 'NO'.
# # Enable tests by passing -DWITH_TESTS=ON to CMake.
# option(WITH_TESTS "Builds with tests" NO)

# if(WITH_TESTS)
#   # Look for QtTest
#   find_package(${QtX} REQUIRED COMPONENTS Test)

#   # Tell CMake functions like add_qtc_plugin about the QtTest component.
#   set(IMPLICIT_DEPENDS Qt::Test)

#   # Enable ctest for auto tests.
#   enable_testing()
# endif()


add_qtc_plugin(CodeBooster
  PLUGIN_DEPENDS
    Core
    LanguageClient
  DEPENDS
    Qt::Widgets
    Qt::Network
    Qt::Sql
    ExtensionSystem
    Utils
    ProjectExplorer
    TextEditor
  SOURCES
    codebooster.qrc
    codeboosterclient.cpp
    codeboosterclient.h
    codeboosterconstants.h
    codeboosterhoverhandler.cpp
    codeboosterhoverhandler.h
    codeboostericons.h
    codeboosteroptionspage.cpp
    codeboosteroptionspage.h
    codeboosterplugin.cpp
    codeboosterplugin.h
    codeboosterprojectpanel.cpp
    codeboosterprojectpanel.h
    codeboostersettings.cpp
    codeboostersettings.h
    codeboostersuggestion.cpp
    codeboostersuggestion.h
    codeboostertr.h
    requests/getcompletions.h
    codeboosterclientinterface.h
    codeboosterclientinterface.cpp
    promptbuilder.h
    promptbuilder.cpp
    replyparser.h
    replyparser.cpp
)

target_sources(CodeBooster
  PRIVATE
    chatsidebar/chatviewfactory.cpp
    chatsidebar/chatviewfactory.h
    chatsidebar/chatview.cpp
    chatsidebar/chatview.h
    chatsidebar/chatview.ui
    chatsidebar/inputwidget.cpp
    chatsidebar/inputwidget.h
    chatsidebar/instrumentor.h
    chatsidebar/chatdatabase.cpp
    chatsidebar/chatdatabase.h
    chatsidebar/chatexportdialog.cpp
    chatsidebar/chatexportdialog.h
    chatsidebar/chatexportdialog.ui
    chatsidebar/md4c/entity.c
    chatsidebar/md4c/entity.h
    chatsidebar/md4c/md4c.c
    chatsidebar/md4c/md4c.h
    chatsidebar/md4c/md4c-html.c
    chatsidebar/md4c/md4c-html.h
    chatsidebar/qmarkdowntextedit/linenumberarea.h
    chatsidebar/qmarkdowntextedit/markdownhighlighter.cpp
    chatsidebar/qmarkdowntextedit/markdownhighlighter.h
    chatsidebar/qmarkdowntextedit/media.qrc
    chatsidebar/qmarkdowntextedit/qmarkdowntextedit.cpp
    chatsidebar/qmarkdowntextedit/qmarkdowntextedit.h
    chatsidebar/qmarkdowntextedit/qownlanguagedata.cpp
    chatsidebar/qmarkdowntextedit/qownlanguagedata.h
    chatsidebar/qmarkdowntextedit/qplaintexteditsearchwidget.cpp
    chatsidebar/qmarkdowntextedit/qplaintexteditsearchwidget.h
    chatsidebar/qmarkdowntextedit/qplaintexteditsearchwidget.ui
    chatsidebar/markdownpreview/codetohtmlconverter.cpp
    chatsidebar/markdownpreview/codetohtmlconverter.h
    chatsidebar/markdownpreview/filedialog.cpp
    chatsidebar/markdownpreview/filedialog.h
    chatsidebar/markdownpreview/markdownhtmlconverter.cpp
    chatsidebar/markdownpreview/markdownhtmlconverter.h
    chatsidebar/markdownpreview/markdownpreview.pri
    chatsidebar/markdownpreview/markdownpreviewsetting.cpp
    chatsidebar/markdownpreview/markdownpreviewsetting.h
    chatsidebar/markdownpreview/messagepreviewwidget.cpp
    chatsidebar/markdownpreview/messagepreviewwidget.h
    chatsidebar/markdownpreview/misc.cpp
    chatsidebar/markdownpreview/misc.h
    chatsidebar/markdownpreview/notepreviewwidget.cpp
    chatsidebar/markdownpreview/notepreviewwidget.h
    chatsidebar/markdownpreview/qtexteditsearchwidget.cpp
    chatsidebar/markdownpreview/qtexteditsearchwidget.h
    chatsidebar/markdownpreview/qtexteditsearchwidget.ui
    chatsidebar/markdownpreview/resource.qrc
    chatsidebar/markdownpreview/schema.cpp
    chatsidebar/markdownpreview/schema.h
    chatsidebar/markdownpreview/schemes.conf
    codeboosterutils.h
    codeboosterutils.cpp
    chatsidebar/chathistorypage.h
    chatsidebar/chathistorypage.cpp
    chatsidebar/chathistorypage.ui
    chatsidebar/chatsessionwgt.h
    chatsidebar/chatsessionwgt.cpp
    chatsidebar/chatsessionwgt.ui
    widgetcover.cpp widgetcover.h
    chatsidebar/sliderindicator.h
    chatsidebar/sliderindicator.cpp
    widgettheme.h
    widgettheme.cpp
    chatsidebar/customlinewidget.h
    askcodeboostertaskhandler.h
    askcodeboostertaskhandler.cpp

)

qt_add_translations(CodeBooster TS_FILES CodeBooster_zh_CN.ts)
install(FILES ${PROJECT_BINARY_DIR}/CodeBooster_zh_CN.qm DESTINATION share/qtcreator/translations)

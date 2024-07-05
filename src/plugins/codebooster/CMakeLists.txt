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

qt_add_translations(CodeBooster TS_FILES CodeBooster_zh_CN.ts)
install(FILES ${PROJECT_BINARY_DIR}/CodeBooster_zh_CN.qm DESTINATION share/qtcreator/translations)

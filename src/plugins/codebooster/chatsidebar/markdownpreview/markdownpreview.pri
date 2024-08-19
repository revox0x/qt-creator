INCLUDEPATH += $$PWD/

include($$PWD/qmarkdowntextedit-headers.pri)
include($$PWD/qmarkdowntextedit-sources.pri)

HEADERS += \
    $$PWD/chatdatabase.h \
    $$PWD/chatexportdialog.h \
    $$PWD/codetohtmlconverter.h \
    $$PWD/filedialog.h \
    $$PWD/markdownhtmlconverter.h \
    $$PWD/markdownpreviewsetting.h \
    $$PWD/messagepreviewwidget.h \
    $$PWD/misc.h \
    $$PWD/notepreviewwidget.h \
    $$PWD/qownlanguagedata.h \
    $$PWD/qtexteditsearchwidget.h \
    $$PWD/schema.h

SOURCES += \
    $$PWD/chatdatabase.cpp \
    $$PWD/chatexportdialog.cpp \
    $$PWD/codetohtmlconverter.cpp \
    $$PWD/filedialog.cpp \
    $$PWD/markdownhtmlconverter.cpp \
    $$PWD/markdownpreviewsetting.cpp \
    $$PWD/messagepreviewwidget.cpp \
    $$PWD/misc.cpp \
    $$PWD/notepreviewwidget.cpp \
    $$PWD/qownlanguagedata.cpp \
    $$PWD/qtexteditsearchwidget.cpp \
    $$PWD/schema.cpp

FORMS += \
    $$PWD/chatexportdialog.ui \
    $$PWD/qtexteditsearchwidget.ui

RESOURCES += \
    $$PWD/resource.qrc

# MyOffice - root qmake project  (https://github.com/arshc0der/myoffice)
# Requires Qt >= 6.5 with the Svg module and Qt private headers (qt6-base-private-dev on Debian/Ubuntu).
TEMPLATE = app
TARGET   = MyOffice
QT      += core gui widgets concurrent svg gui-private
CONFIG  += c++17 warn_on
CONFIG  -= app_bundle

!versionAtLeast(QT_VERSION, 6.5.0): error("MyOffice requires Qt 6.5 or newer (found $$QT_VERSION)")

INCLUDEPATH += $$PWD
msvc: QMAKE_CXXFLAGS += /utf-8

RESOURCES += resources/resources.qrc

include(core/core.pri)
include(ui/ui.pri)
include(myword/myword.pri)
include(myexcel/myexcel.pri)
include(myppt/myppt.pri)

SOURCES += main.cpp

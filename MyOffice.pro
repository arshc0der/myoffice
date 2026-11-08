# MyOffice - root qmake project (Milestone 1: application shell)
# Requires Qt >= 6.5 (QStyleHints::colorScheme) and a C++17 compiler.
TEMPLATE = app
TARGET   = MyOffice
QT      += core gui widgets concurrent
CONFIG  += c++17 warn_on
CONFIG  -= app_bundle

!versionAtLeast(QT_VERSION, 6.5.0): error("MyOffice requires Qt 6.5 or newer (found $$QT_VERSION)")

INCLUDEPATH += $$PWD
msvc: QMAKE_CXXFLAGS += /utf-8

include(core/core.pri)
include(ui/ui.pri)

SOURCES += main.cpp

# Module projects (myword/, myexcel/, mypowerpoint/) are added as static-library
# subprojects in their own phases and linked here.

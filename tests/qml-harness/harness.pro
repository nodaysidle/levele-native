TEMPLATE = app
TARGET = harness
QT += core gui qml quick quickcontrols2 svg concurrent dbus testlib
CONFIG += c++17 console
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -Wall -Wextra

R = $$PWD/../..
INCLUDEPATH += $$R/src

SOURCES += harness.cpp \
    $$R/src/theme.cpp \
    $$R/src/document.cpp \
    $$R/src/templates.cpp \
    $$R/src/studio.cpp \
    $$R/src/canvasitem.cpp \
    $$R/src/exporter.cpp \
    $$R/src/portalfilechooser.cpp

HEADERS += \
    $$R/src/theme.h \
    $$R/src/document.h \
    $$R/src/templates.h \
    $$R/src/studio.h \
    $$R/src/canvasitem.h \
    $$R/src/exporter.h \
    $$R/src/portalfilechooser.h

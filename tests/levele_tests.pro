TEMPLATE = app
TARGET = levele_tests

QT += testlib core gui qml quick quickcontrols2 svg concurrent dbus
CONFIG += c++17 console testcase
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -Wall -Wextra

R = $$PWD/..
INCLUDEPATH += $$R/src

SOURCES += \
    levele_tests.cpp \
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

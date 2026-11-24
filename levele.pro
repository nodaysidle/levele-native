TEMPLATE = app
TARGET = levele

QT += core gui qml quick quickcontrols2 svg concurrent dbus

CONFIG += c++17
CONFIG -= app_bundle

QMAKE_CXXFLAGS += -Wall -Wextra

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/theme.cpp \
    src/document.cpp \
    src/templates.cpp \
    src/studio.cpp \
    src/canvasitem.cpp \
    src/exporter.cpp \
    src/portalfilechooser.cpp

HEADERS += \
    src/theme.h \
    src/document.h \
    src/templates.h \
    src/studio.h \
    src/canvasitem.h \
    src/exporter.h \
    src/portalfilechooser.h

RESOURCES += resources.qrc

# No INSTALLS: `make install` used to copy the binary into the source tree.
# Packaging installs it (PKGBUILD, once Alan approves one).

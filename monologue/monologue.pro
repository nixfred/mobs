QT += core gui qml quick quickcontrols2 multimedia dbus concurrent
CONFIG += c++17 release
TARGET = monologue
TEMPLATE = app
include(src/common.pri)
SOURCES += src/main.cpp
RESOURCES += src/resources.qrc

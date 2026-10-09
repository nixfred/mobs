QT += core gui multimedia dbus concurrent
CONFIG += c++17 console
TARGET = camera_check
TEMPLATE = app
include(../src/common.pri)
INCLUDEPATH += ../src
SOURCES += camera_check.cpp

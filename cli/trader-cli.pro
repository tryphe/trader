QT = core network

TARGET = trader-cli
DESTDIR = ../

MOC_DIR = ../build-tmp/cli
OBJECTS_DIR = ../build-tmp/cli

CONFIG += c++14 c++17
CONFIG += RELEASE

SOURCES += commandcaller.cpp \
        main.cpp

HEADERS += \
    commandcaller.h


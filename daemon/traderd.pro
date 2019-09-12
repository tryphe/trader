QT       = core network websockets

TARGET = traderd
DESTDIR = ../

MOC_DIR = ../build-tmp/daemon
OBJECTS_DIR = ../build-tmp/daemon

CONFIG += c++14 c++17
CONFIG += RELEASE
#CONFIG += DEBUG

# enables stack symbols on release build for QMessageLogContext function and line output
#DEFINES -= QT_MESSAGELOGCONTEXT

LIBS += -lgmp

QMAKE_CXXFLAGS_RELEASE = -ansi -pedantic-errors -fstack-protector-strong -fstack-reuse=none -D_FORTIFY_SOURCE=2 -pie -fPIE -O3
QMAKE_CFLAGS_RELEASE   = -ansi -pedantic-errors -fstack-protector-strong -fstack-reuse=none -D_FORTIFY_SOURCE=2 -pie -fPIE -O3

SOURCES += main.cpp \
    commandlistener.cpp \
    commandrunner.cpp \
    costfunctioncache.cpp \
    enginesettings.cpp \
    fallbacklistener.cpp \
    position.cpp \
    engine.cpp \
    positionman.cpp \
    spruce.cpp \
    trader.cpp \
    stats.cpp \
    trexrest.cpp \
    bncrest.cpp \
    polorest.cpp \
    baserest.cpp \
    engine_test.cpp \
    coinamount.cpp \
    coinamount_test.cpp \
    wssserver.cpp

HEADERS += build-config.h \
    commandlistener.h \
    commandrunner.h \
    costfunctioncache.h \
    enginesettings.h \
    fallbacklistener.h \
    global.h \
    coinamount.h \
    keydefs.h \
    position.h \
    engine.h \
    positionman.h \
    spruce.h \
    trader.h \
    stats.h \
    trexrest.h \
    bncrest.h \
    polorest.h \
    keystore.h \
    engine_test.h \
    baserest.h \
    misctypes.h \
    ssl_policy.h \
    coinamount_test.h \
    wssserver.h

RESOURCES += \
    ../res/resources.server.qrc


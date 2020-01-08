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
    alphatracker.cpp \
    commandlistener.cpp \
    commandrunner.cpp \
    costfunctioncache.cpp \
    fallbacklistener.cpp \
    market.cpp \
    position.cpp \
    engine.cpp \
    positionman.cpp \
    spruce.cpp \
    spruceoverseer.cpp \
    trader.cpp \
    stats.cpp \
    trexrest.cpp \
    bncrest.cpp \
    polorest.cpp \
    wavesrest.cpp \
    baserest.cpp \
    engine_test.cpp \
    coinamount.cpp \
    coinamount_test.cpp

HEADERS += build-config.h \
    alphatracker.h \
    commandlistener.h \
    commandrunner.h \
    costfunctioncache.h \
    enginesettings.h \
    fallbacklistener.h \
    global.h \
    coinamount.h \
    keydefs.h \
    market.h \
    position.h \
    engine.h \
    positiondata.h \
    positionman.h \
    spruce.h \
    spruceoverseer.h \
    trader.h \
    stats.h \
    trexrest.h \
    bncrest.h \
    wavesrest.h \
    polorest.h \
    keystore.h \
    engine_test.h \
    baserest.h \
    misctypes.h \
    ssl_policy.h \
    coinamount_test.h

RESOURCES += \
    ../res/resources.server.qrc


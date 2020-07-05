QT       = core network

QMAKE_CXXFLAGS = -O3
QMAKE_CFLAGS = -O3

CONFIG += c++14 c++17
CONFIG += release

LIBS += -lgmp

TARGET = backtest-mt
TEMPLATE = app

SOURCES += main.cpp \
    ../daemon/alphatracker.cpp \
    ../daemon/coinamount.cpp \
    ../daemon/coinamount_test.cpp \
    ../daemon/market.cpp \
    ../daemon/position.cpp \
    ../daemon/sprucev2.cpp \
    ../daemon/priceaggregator.cpp \
    ../libbase58/base58.c \
    ../qbase58/qbase58.cpp \
    ../qbase58/qbase58_test.cpp \
    simulationthread.cpp \
    tester.cpp

HEADERS += \
    ../daemon/global.h \
    ../daemon/alphatracker.h \
    ../daemon/coinamount.h \
    ../daemon/coinamount_test.h \
    ../daemon/market.h \
    ../daemon/position.h \
    ../daemon/sprucev2.h \
    ../daemon/priceaggregator.h \
    ../daemon/misctypes.h \
    ../libbase58/libbase58.h \
    ../qbase58/qbase58.h \
    ../qbase58/qbase58_test.h \
    simulationthread.h \
    tester.h

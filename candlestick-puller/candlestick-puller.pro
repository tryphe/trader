QT = core network

TARGET = candlestick-puller
TEMPLATE = app

CONFIG += c++14 c++17
CONFIG += release

LIBS += -lgmp

SOURCES += \
        ../daemon/coinamount.cpp \
        ../daemon/market.cpp \
        ../daemon/priceaggregator.cpp \
        ../daemon/pricesignal.cpp \
        main.cpp \
        puller.cpp

HEADERS += \
        ../daemon/ssl_policy.h \
        ../daemon/coinamount.h \
        ../daemon/market.h \
        ../daemon/priceaggregator.h \
        ../daemon/pricesignal.h \
        puller.h

DISTFILES += \
    README.md

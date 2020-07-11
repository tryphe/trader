QT = core network

TARGET = candlestick-puller
TEMPLATE = app

LIBS += -lgmp

SOURCES += \
        ../daemon/coinamount.cpp \
        ../daemon/market.cpp \
        main.cpp \
        puller.cpp

HEADERS += \
        ../daemon/ssl_policy.h \
        ../daemon/coinamount.h \
        ../daemon/market.h \
        puller.h

DISTFILES += \
    README.md


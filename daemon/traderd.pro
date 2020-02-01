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
    coinamount_test.cpp \
    wavesutil.cpp \
    wavesutil_test.cpp \
    wavesaccount.cpp \
    wavesaccount_test.cpp \
    ../libbase58/base58.c \
    ../qbase58/qbase58.cpp \
    ../qbase58/qbase58_test.cpp \
    ../blake2b/sse/blake2b.c \
    ../libcurve25519-donna/nacl_sha512/hash.c \
    ../libcurve25519-donna/nacl_sha512/blocks.c \
    ../libcurve25519-donna/additions/keygen.c \
    ../libcurve25519-donna/additions/curve_sigs.c \
    ../libcurve25519-donna/additions/compare.c \
    ../libcurve25519-donna/additions/fe_montx_to_edy.c \
    ../libcurve25519-donna/additions/open_modified.c \
    ../libcurve25519-donna/additions/sign_modified.c \
    ../libcurve25519-donna/additions/ge_p3_to_montx.c \
    ../libcurve25519-donna/additions/zeroize.c \
    ../libcurve25519-donna/ge_scalarmult_base.c \
    ../libcurve25519-donna/fe_0.c \
    ../libcurve25519-donna/fe_1.c \
    ../libcurve25519-donna/fe_add.c \
    ../libcurve25519-donna/fe_invert.c \
    ../libcurve25519-donna/fe_isnegative.c \
    ../libcurve25519-donna/fe_isnonzero.c \
    ../libcurve25519-donna/fe_sub.c \
    ../libcurve25519-donna/fe_sq.c \
    ../libcurve25519-donna/fe_sq2.c \
    ../libcurve25519-donna/fe_frombytes.c \
    ../libcurve25519-donna/fe_pow22523.c \
    ../libcurve25519-donna/fe_mul.c \
    ../libcurve25519-donna/fe_tobytes.c \
    ../libcurve25519-donna/fe_cmov.c \
    ../libcurve25519-donna/fe_copy.c \
    ../libcurve25519-donna/fe_neg.c \
    ../libcurve25519-donna/ge_add.c \
    ../libcurve25519-donna/ge_p3_0.c \
    ../libcurve25519-donna/ge_frombytes.c \
    ../libcurve25519-donna/ge_tobytes.c \
    ../libcurve25519-donna/ge_p3_tobytes.c \
    ../libcurve25519-donna/ge_precomp_0.c \
    ../libcurve25519-donna/ge_p2_dbl.c \
    ../libcurve25519-donna/ge_p3_dbl.c \
    ../libcurve25519-donna/ge_p2_0.c \
    ../libcurve25519-donna/ge_p1p1_to_p2.c \
    ../libcurve25519-donna/ge_p1p1_to_p3.c \
    ../libcurve25519-donna/ge_p3_to_p2.c \
    ../libcurve25519-donna/ge_p3_to_cached.c \
    ../libcurve25519-donna/ge_double_scalarmult.c \
    ../libcurve25519-donna/ge_madd.c \
    ../libcurve25519-donna/ge_msub.c \
    ../libcurve25519-donna/ge_sub.c \
    ../libcurve25519-donna/sc_reduce.c \
    ../libcurve25519-donna/sc_muladd.c

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
    coinamount_test.h \
    wavesutil.h \
    wavesutil_test.h \
    wavesaccount.h \
    wavesaccount_test.h \
    ../libbase58/libbase58.h \
    ../qbase58/qbase58.h \
    ../qbase58/qbase58_test.h \
    ../blake2b/sse/blake2.h \
    ../libcurve25519-donna/nacl_includes/crypto_uint32.h \
    ../libcurve25519-donna/nacl_includes/crypto_int32.h \
    ../libcurve25519-donna/fe.h \
    ../libcurve25519-donna/ge.h \
    ../libcurve25519-donna/additions/crypto_additions.h \
    ../libcurve25519-donna/additions/keygen.h \
    ../libcurve25519-donna/additions/curve_sigs.h

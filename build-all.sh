#!/bin/bash

failureHints=''

# cleanup old build stuff
rm -r build-tmp/ 2> /dev/null
rm traderd* trader-cli 2> /dev/null
rm cli/Makefile* daemon/Makefile* 2> /dev/null

# ensure WSS resources exist
./generate_certs.sh

# something bad happened
if [ ! -f res/x509.cert ] || [ ! -f res/x509.key ] || [ ! -f res/session.token ]; then
  echo "Failed to build resource files."
  exit 1
fi

# select bittrex, deselect poloniex/binance
sed -i 's|.*#define EXCHANGE_BITTREX|#define EXCHANGE_BITTREX|' daemon/build-config.h
sed -i 's|.*#define EXCHANGE_POLONIEX|//#define EXCHANGE_POLONIEX|' daemon/build-config.h
sed -i 's|.*#define EXCHANGE_BINANCE|//#define EXCHANGE_BINANCE|' daemon/build-config.h
make -j
if [ -f traderd ]; then
  strip -s traderd
  mv traderd traderd-bittrex
  failureHints+='traderd-bittrex SUCCESS. '
else
  failureHints+='traderd-bittrex FAILED. '
  echo "Failed to build traderd-bittrex"
fi

# deselect bittrex, select poloniex
sed -i 's|.*#define EXCHANGE_BITTREX|//#define EXCHANGE_BITTREX|' daemon/build-config.h
sed -i 's|.*#define EXCHANGE_POLONIEX|#define EXCHANGE_POLONIEX|' daemon/build-config.h
make -j
if [ -f traderd ]; then
  strip -s traderd
  mv traderd traderd-poloniex
  failureHints+='traderd-poloniex SUCCESS. '
else
  failureHints+='traderd-poloniex FAILED. '
  echo "Failed to build traderd-poloniex"
fi

# deselect poloniex, select binance
sed -i 's|.*#define EXCHANGE_POLONIEX|//#define EXCHANGE_POLONIEX|' daemon/build-config.h
sed -i 's|.*#define EXCHANGE_BINANCE|#define EXCHANGE_BINANCE|' daemon/build-config.h
make -j
if [ -f traderd ]; then
  strip -s traderd
  mv traderd traderd-binance
  failureHints+='traderd-binance SUCCESS. '
else
  failureHints+='traderd-binance FAILED. '
  echo "Failed to build traderd-binance"
fi

if [ -f trader-cli ]; then
  strip -s trader-cli
  failureHints+='trader-cli SUCCESS. '
else
  failureHints+='trader-cli FAILED. '
fi

echo "Summary: $failureHints"

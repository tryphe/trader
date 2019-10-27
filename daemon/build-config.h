#ifndef BUILDCONFIG_H
#define BUILDCONFIG_H

#define BUILD_VERSION "1.75i"

/// select your exchange
//#define EXCHANGE_BITTREX
//#define EXCHANGE_POLONIEX
#define EXCHANGE_BINANCE

/// where to print logs
#define PRINT_LOGS_TO_CONSOLE
#define PRINT_LOGS_TO_FILE
#define PRINT_LOGS_TO_FILE_COLOR

/// what to log
//#define PRINT_LOGS_WITH_FUNCTION_NAMES
#define PRINT_ENABLED_SSL_CIPHERS
//#define PRINT_DISABLED_SSL_CIPHERS

/// allow wss connections?
//#define WSS_INTERFACE

/// allow wss on which interface? valid entries are:
/// ip address strings, hostname strings,
#define WSS_BIND "" // if you want to bind to a specific address, set this and WSS_ADDRESS will be ignored
#define WSS_ADDRESS AnyIPv4 // options: Any, AnyIPv4, AnyIPv6, LocalHost, LocalHostIPv6
#define WSS_ADDRESS_FAR "127.0.0.1" // the daemon's public address from the perspective of the gui

/// build options
//#define EXTRA_NICE // be extra nice to the exchange api

#endif // BUILDCONFIG_H

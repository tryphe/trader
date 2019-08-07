#ifndef BUILDCONFIG_H
#define BUILDCONFIG_H

#define BUILD_VERSION "1.73c"

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
#define PRINT_TEST_PERFORMANCE

/// allow bulk input from in.txt? (deprecated with ipc sockets)
#define FALLBACK_FILE_INPUT

/// build options
//#define TRYPHE_BUILD
//#define SECONDARY_BOT

#endif // BUILDCONFIG_H

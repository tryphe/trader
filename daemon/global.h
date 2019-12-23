#ifndef GLOBAL_H
#define GLOBAL_H

#include "build-config.h"
#include "keydefs.h"
#include <gmp.h>

#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QSslSocket>

#define kDebug QMessageLogger( __FILE__, __LINE__, Q_FUNC_INFO ).debug().noquote

/// global symbols
static const quint8 ENGINE_BITTREX                          ( 0 );
static const quint8 ENGINE_BINANCE                          ( 1 );
static const quint8 ENGINE_POLONIEX                         ( 2 );

static const quint8 SIDE_BUY                                ( 1 );
static const quint8 SIDE_SELL                               ( 2 );

static const quint8 CANCELLING_LOWEST                       ( 1 );
static const quint8 CANCELLING_HIGHEST                      ( 2 );
static const quint8 CANCELLING_FOR_SHORTLONG                ( 3 );
static const quint8 CANCELLING_FOR_DC                       ( 4 );
static const quint8 CANCELLING_FOR_SLIPPAGE_RESET           ( 5 );
static const quint8 CANCELLING_FOR_MAX_AGE                  ( 6 );
static const quint8 CANCELLING_FOR_USER                     ( 7 );
static const quint8 CANCELLING_FOR_SPRUCE                   ( 8 );
static const quint8 CANCELLING_FOR_SPRUCE_2                 ( 9 );
static const quint8 CANCELLING_FOR_SPRUCE_3                 ( 10 );
static const quint8 CANCELLING_FOR_SPRUCE_4                 ( 11 );

static const quint8 FILL_GETORDER                           ( 1 );
static const quint8 FILL_HISTORY                            ( 2 );
static const quint8 FILL_TICKER                             ( 3 );
static const quint8 FILL_CANCEL                             ( 4 );
static const quint8 FILL_WSS                                ( 5 );

static const QLatin1String BUY                              ( "buy" );
static const QLatin1String SELL                             ( "sell" );
static const QLatin1String ACTIVE                           ( "active" );
static const QLatin1String GHOST                            ( "ghost" );
static const QLatin1String COMMAND                          ( "command" );
static const QLatin1String NONCE                            ( "nonce" );
static const QLatin1String ALL                              ( "all" );
static const QByteArray    KEY                              ( "key" );
static const QByteArray    SIGN                             ( "sign" );
static const QByteArray    CONTENT_TYPE                     ( "Content-Type" );
static const QByteArray    CONTENT_TYPE_ARGS                ( "application/x-www-form-urlencoded" );
static const QLatin1String BITTREX_SUBPATH                  ( "tt" );
static const QLatin1String POLONIEX_SUBPATH                 ( "pt" );
static const QLatin1String BINANCE_SUBPATH                  ( "bt" );
static const int           MARKET_STRING_WIDTH              ( 10 );
static const int           PRICE_WIDTH                      ( 17 );
static const int           ORDER_STRING_SIZE                ( 22 );
static const QLatin1String DEFAULT_MARKET_STRING_TEMPLATE   ( "%1_%2" );

// trex symbols
static const QLatin1String BITTREX_DEFAULT_FEERATE          ( "0.0015" );
static const QLatin1String BITTREX_MINIMUM_ORDER_SIZE       ( "0.00065000" );
static const QLatin1String BITTREX_MARKET_STRING_TEMPLATE   ( "%1-%2" );
static const QLatin1String BITTREX_EXCHANGE_STR             ( "Bittrex" );
static const int BITTREX_TIMER_INTERVAL_NAM_SEND            ( 330 );
static const int BITTREX_TIMER_INTERVAL_ORDER_HISTORY       ( 60000 );
static const int BITTREX_TIMER_INTERVAL_ORDERBOOK           ( 150000 );
static const int BITTREX_TIMER_INTERVAL_TICKER              ( 15000 );
static const int BITTREX_SAFETY_DELAY                       ( 8500 );

static const QLatin1String TREX_REST_URL                    ( "https://bittrex.com/api/v1.1/" );
static const QLatin1String TREX_COMMAND_CANCEL              ( "market/cancel" );
static const QLatin1String TREX_COMMAND_BUY                 ( "market/buylimit" );
static const QLatin1String TREX_COMMAND_SELL                ( "market/selllimit" );
static const QLatin1String TREX_COMMAND_GET_ORDERS          ( "market/getopenorders" );
static const QLatin1String TREX_COMMAND_GET_ORDER           ( "account/getorder" );
static const QLatin1String TREX_COMMAND_GET_ORDER_HIST      ( "account/getorderhistory" );
static const QLatin1String TREX_COMMAND_GET_BALANCES        ( "account/getbalances" );
static const QLatin1String TREX_COMMAND_GET_MARKET_SUMS     ( "public/getmarketsummaries" );
static const QLatin1String TREX_APIKEY                      ( "apikey" );
static const QByteArray TREX_APISIGN                        ( "apisign" );

// binance symbols
static const QLatin1String BINANCE_DEFAULT_FEERATE          ( "0.001" );
static const QLatin1String BINANCE_MINIMUM_ORDER_SIZE       ( "0.00105000" );
static const QLatin1String BINANCE_MARKET_STRING_TEMPLATE   ( "%2%1" );
static const QLatin1String BINANCE_EXCHANGE_STR             ( "Binance" );
static const int BINANCE_TIMER_INTERVAL_NAM_SEND            ( 111 );
static const int BINANCE_TIMER_INTERVAL_ORDERBOOK           ( 50000 );
static const int BINANCE_TIMER_INTERVAL_TICKER              ( 15000 );
static const int BINANCE_SAFETY_DELAY                       ( 2000 );

static const QLatin1String BNC_URL                          ( "https://api.binance.com/api/v3/" );
static const QLatin1String BNC_URL_WSS                      ( "wss://api.binance.com" );
static const QLatin1String BNC_COMMAND_GETORDERS            ( "sign-get-openOrders" );
static const QLatin1String BNC_COMMAND_BUYSELL              ( "sign-post-order" );
static const QLatin1String BNC_COMMAND_CANCEL               ( "sign-delete-order" );
static const QLatin1String BNC_COMMAND_GETTICKER            ( "get-ticker/bookTicker" );
static const QLatin1String BNC_COMMAND_GETEXCHANGEINFO      ( "get-v1-exchangeInfo" );
static const QLatin1String BNC_COMMAND_GETBALANCES          ( "sign-get-account" );
static const QLatin1String BNC_RECVWINDOW                   ( "recvWindow" );
static const QLatin1String BNC_TIMESTAMP                    ( "timestamp" );
static const QLatin1String BNC_SIGNATURE                    ( "signature" );
static const QByteArray BNC_APIKEY                          ( "X-MBX-APIKEY" );

// polo symbols
static const QLatin1String POLONIEX_DEFAULT_FEERATE         ( "0.001" );
static const QLatin1String POLONIEX_MINIMUM_ORDER_SIZE      ( "0.00011000" );
static const QLatin1String POLONIEX_MARKET_STRING_TEMPLATE  ( "%1_%2" );
static const QLatin1String POLONIEX_EXCHANGE_STR            ( "Poloniex" );
static const int POLONIEX_TIMER_INTERVAL_NAM_SEND           ( 200 );
static const int POLONIEX_TIMER_INTERVAL_ORDERBOOK          ( 5000 );
static const int POLONIEX_TIMER_INTERVAL_TICKER             ( 10000 );
static const int POLONIEX_SAFETY_DELAY                      ( 2000 );

static const QLatin1String POLO_URL_TRADE               ( "https://poloniex.com/tradingApi" );
static const QLatin1String POLO_URL_PUBLIC              ( "https://poloniex.com/public" );
static const QLatin1String POLO_URL_PRIVATE             ( "https://poloniex.com/private" );
static const QLatin1String POLO_URL_WSS                 ( "wss://api2.poloniex.com/" );

static const QLatin1String POLO_COMMAND_GETCHARTDATA    ( "returnChartData" );
static const QLatin1String POLO_COMMAND_GETBALANCES     ( "returnCompleteBalances" );
static const QLatin1String POLO_COMMAND_GETORDERS       ( "returnOpenOrders" );
static const QLatin1String POLO_COMMAND_GETORDERS_ARGS  ( "currencyPair=all" );
static const QLatin1String POLO_COMMAND_CANCEL          ( "cancelOrder" );
static const QLatin1String POLO_COMMAND_CANCEL_ARGS     ( "orderNumber=" );
static const QLatin1String POLO_COMMAND_GETBOOKS        ( "returnOrderBook" );
static const QLatin1String POLO_COMMAND_GETBOOKS_ARGS   ( "currencyPair=all&depth=1" );
static const QLatin1String POLO_COMMAND_GETFEE          ( "returnFeeInfo" );

namespace Global {

static inline const QString printVectorqint32( const QVector<qint32> &vec )
{
    QString ret = "[";

    for ( int i = 0; i < vec.size(); i++ )
        ret += QString( "%1, " )
                .arg( vec.value( i ) );

    // trim last comma
    int sz = ret.size();
    if ( sz > 1 )
        ret.truncate( sz -2 );

    ret += QChar( ']' );

    return ret;
}

static inline QString getBuildString()
{
    return QString( "bot version[%1]  qt[%2]  gmp[%3]  ssl[%4]  build time[%5 %6]" )
            .arg( BUILD_VERSION )
            .arg( QT_VERSION_STR )
            .arg( __gmp_version )
            .arg( QSslSocket::sslLibraryBuildVersionString() )
            .arg( __DATE__ )
            .arg( __TIME__ );
}

static inline QString getDateStringMDY()
{
    return QDateTime::currentDateTime().toString( "MM-dd-yy" );
}
static inline QString getDateStringFull()
{
    return QDateTime::currentDateTime().toString( "MM-dd-yy HH:mm:ss" );
}

static inline QByteArray getBittrexPoloSignature( const QByteArray &body, const QByteArray &secret )
{
    // sign hmac-sha512
    return QMessageAuthenticationCode::hash( body, secret, QCryptographicHash::Sha512 ).toHex();
}

static inline QByteArray getBncSignature( const QByteArray &body, const QByteArray &secret )
{
    // sign hmac-sha256
    return QMessageAuthenticationCode::hash( body, secret, QCryptographicHash::Sha256 ).toHex();
}

static inline const QString getConfigPath()
{
    return QStandardPaths::writableLocation( QStandardPaths::ConfigLocation );
}

static inline const QString getTraderPath()
{
    return getConfigPath() + QDir::separator() + "trader";
}

static inline const QString getBittrexSettingsPath()
{
    return getTraderPath() + QDir::separator() + "bittrex.settings";
}
static inline const QString getBinanceSettingsPath()
{
    return getTraderPath() + QDir::separator() + "binance.settings";
}
static inline const QString getPoloniexSettingsPath()
{
    return getTraderPath() + QDir::separator() + "poloniex.settings";
}

static inline const QString getIPCPath()
{
    return getTraderPath() + QDir::separator() + "trader.ipc";
}

static inline const QString getMarketStatsPath()
{
    return getTraderPath() + QDir::separator() + "stats";
}

static inline const QString getCostFunctionCachePath()
{
    return getTraderPath() + QDir::separator() + "cache";
}

static inline const QString getOldLogsPath()
{
    return getTraderPath() + QDir::separator() + "logs_old";
}

static inline void ensurePath()
{
    // get dir ~/.config/<trader_dir>
    QString trader_path = getTraderPath();
    QString oldlogs_path = getOldLogsPath();
    QString function_cache_path = getCostFunctionCachePath();

    // make sure ~/.config and trader_path exists (trader_path is a subdirectory of config_path)
    QDir dir;
    bool ret0 = dir.exists( trader_path ) || dir.mkdir( trader_path );
    bool ret1 = dir.exists( oldlogs_path ) || dir.mkdir( oldlogs_path );
    bool ret2 = dir.exists( function_cache_path ) || dir.mkdir( function_cache_path );

    // if we failed, we have the wrong file permissions to continue
    if ( !ret0 || !ret1 || !ret2 )
        qCritical() << "local error: could not open config path" << trader_path << "(check file permissions)";
}

static inline bool moveOldLogsOut()
{
    const QString trader_path = getTraderPath();
    const QString oldlogs_path = getOldLogsPath();

    // get log files from previous runs
    QStringList files = QDir( trader_path ).entryList( QStringList() << "log.*.txt", QDir::Files | QDir::Readable | QDir::Writable, QDir::Name );

    // move the files
    for ( int i = 0; i < files.size(); i++ )
    {
        const QString &log_filename = files.value( i );
        QString from = trader_path + QDir::separator() + log_filename;
        QString to = oldlogs_path + QDir::separator() + log_filename;
        //kDebug() << from << "->" << to;

        // create a name that doesn't exist (very unlikely), then copy, then remove
        while ( QFile::exists( to ) ) to += ".1";

        // try to copy the file, then remove it if it didn't fail
        if ( !QFile::copy( from, to ) || !QFile::remove( from ) )
            return false;
    }

    return true;
}

static inline void cleanseColorTags( QString &s )
{
    s.replace( QRegExp( ">>>[^<<<]*<<<" ), QLatin1String() );
}

static inline void fillInColors( QString &s )
{
    // fill in colors
    s.replace( ">>>red<<<", "\x1b[31m" );
    s.replace( ">>>grn<<<", "\x1b[32m" );
    s.replace( ">>>none<<<", "\x1b[0m" );
}

static void messageOutput( QtMsgType type, const QMessageLogContext &context, const QString &msg )
{
    // avoid warnings
    Q_UNUSED( messageOutput )
    Q_UNUSED( context )

    // add a logfile tag for test build
    static QString log_file_path = QString( getTraderPath() + QDir::separator() + "log.%1.txt" )
                                    .arg( QDateTime::currentSecsSinceEpoch() );

    static QString log_color_file_path = QString( getTraderPath() + QDir::separator() + "log.%1_color.txt" )
                                    .arg( QDateTime::currentSecsSinceEpoch() );

    static QFile *log_file_handle = new QFile( log_file_path );
    static QFile *log_color_file_handle = new QFile( log_color_file_path );

    // open log file
    if ( !log_file_handle->isOpen() )
    {
        // make sure path exists
        ensurePath();

        // move old logs to <trader_dir>/old_logs/
        if ( !moveOldLogsOut() )
            qCritical() << "local error: failed to move old logs out, check file permissions";

        if ( !log_file_handle->open( QFile::Append | QFile::Text ) )
            qCritical() << "local error: failed to open log file!:" << log_file_path;
        else
            kDebug() << "opened log" << log_file_path;
    }

    // open log_color file
    if ( !log_color_file_handle->isOpen() )
    {
        // make sure path exists
        QDir dir;
        dir.mkdir( getTraderPath() );

        if ( !log_color_file_handle->open( QFile::Append | QFile::Text ) )
            qCritical() << "local error: failed to open log_color file!:" << log_color_file_path;
        else
            kDebug() << "opened log_color" << log_color_file_path;
    }

#if defined(PRINT_LOGS_WITH_FUNCTION_NAMES)
#include <QMessageLogContext>

    QString endMessage = QString( "%1 %2 %3  %4\n" )
                            .arg( getDateStringFull() )
                            .arg( context.function )
                            .arg( "" )
                            .arg( msg );
#else
    static QString endMessage;
    endMessage = QString( "%1  %2\n" )
                    .arg( getDateStringFull() )
                    .arg( msg );
#endif

    // form color string
    static QString endMessage_unixcolors;
    endMessage_unixcolors= endMessage;
    fillInColors( endMessage_unixcolors ); // replace color tags with unix color codes

    static QByteArray endMessageBytes;
    endMessageBytes = endMessage_unixcolors.toUtf8();

    // print to console
#if defined(PRINT_LOGS_TO_CONSOLE)
    fprintf( stderr, "%s", endMessageBytes.constData() );
#endif

#if defined(PRINT_LOGS_TO_FILE_COLOR)
    // print to file_color
    if ( log_color_file_handle->isWritable() )
    {
        log_color_file_handle->write( endMessageBytes, endMessageBytes.size() );
        log_color_file_handle->flush();
    }
#endif

#if defined(PRINT_LOGS_TO_FILE)
    cleanseColorTags( endMessage ); // replace color tags with nothing

    endMessageBytes = endMessage.toUtf8();

    // print to file
    if ( log_file_handle->isWritable() )
    {
        log_file_handle->write( endMessageBytes, endMessageBytes.size() );
        log_file_handle->flush();
    }
#endif

    if ( type == QtFatalMsg )
        abort();
}

} // namespace Global

#endif // GLOBAL_H

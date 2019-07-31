#ifndef GLOBAL_H
#define GLOBAL_H

#include "build-config.h"

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

/// plain strings
#define BUY                             "buy"
#define SELL                            "sell"
#define COMMAND                         "command"
#define NONCE                           "nonce"
#define CONTENT_TYPE                    "Content-Type"
#define CONTENT_TYPE_ARGS               "application/x-www-form-urlencoded"
#define KEY                             "key"
#define SIGN                            "sign"


/// urls/symbols
#if defined(EXCHANGE_BITTREX)
    #define EXCHANGE_STR                        "Bittrex"
    #define REST_OBJECT                         TrexREST
    #define EXCHANGE_SUBPATH                    "tt"
    #define ORDER_STRING_SIZE                   22
    #define INTERFACE_PORT                      62000

    #define TREX_REST_URL                       "https://bittrex.com/api/v1.1/" // QByteArray::fromBase64( "aHR0cHM6Ly9iaXR0cmV4LmNvbS9hcGkvdjEuMS8=" ) // "https://bittrex.com/api/v1.1/"

    #define TREX_COMMAND_CANCEL                 "market/cancel" // QByteArray::fromBase64( "bWFya2V0L2NhbmNlbA" ) //"market/cancel"
    #define TREX_COMMAND_BUY                    "market/buylimit" // QByteArray::fromBase64( "bWFya2V0L2J1eWxpbWl0" ) //"market/buylimit"
    #define TREX_COMMAND_SELL                   "market/selllimit" // QByteArray::fromBase64( "bWFya2V0L3NlbGxsaW1pdA" ) //"market/selllimit"
    #define TREX_COMMAND_GET_ORDERS             "market/getopenorders" // QByteArray::fromBase64( "bWFya2V0L2dldG9wZW5vcmRlcnM" ) //"market/getopenorders"
    #define TREX_COMMAND_GET_ORDER              "account/getorder" // QByteArray::fromBase64( "YWNjb3VudC9nZXRvcmRlcg" ) //"account/getorder"
    #define TREX_COMMAND_GET_ORDER_HISTORY      "account/getorderhistory" // QByteArray::fromBase64( "YWNjb3VudC9nZXRvcmRlcmhpc3Rvcnk" ) //"account/getorderhistory"
    #define TREX_COMMAND_GET_BALANCES           "account/getbalances" // QByteArray::fromBase64( "YWNjb3VudC9nZXRiYWxhbmNlcw" ) //"account/getbalances"
    #define TREX_COMMAND_GET_MARKET_SUMMARIES   "public/getmarketsummaries" // QByteArray::fromBase64( "cHVibGljL2dldG1hcmtldHN1bW1hcmllcw" ) //"public/getmarketsummaries"

    #define TREX_APIKEY                         "apikey" // QByteArray::fromBase64( "YXBpa2V5" ) // "apikey"
    #define TREX_APISIGN                        "apisign" // QByteArray::fromBase64( "YXBpc2lnbg" ) // "apisign"

#elif defined(EXCHANGE_BINANCE)
    #define EXCHANGE_STR                    "Binance"
    #define REST_OBJECT                     BncREST
    #define EXCHANGE_SUBPATH                "bt"
    #define ORDER_STRING_SIZE               15
    #define INTERFACE_PORT                  62001

    #define BNC_URL                         "https://api.binance.com/api/v3/"
    #define BNC_URL_WSS                     "wss://api.binance.com"

    #define BNC_COMMAND_GETORDERS           "sign-get-openOrders"
    #define BNC_COMMAND_BUYSELL             "sign-post-order"
    #define BNC_COMMAND_CANCEL              "sign-delete-order"
    #define BNC_COMMAND_GETTICKER           "get-ticker/bookTicker"
    #define BNC_COMMAND_GETEXCHANGEINFO     "get-v1-exchangeInfo"
    #define BNC_COMMAND_GETBALANCES         "sign-get-account"

    #define BNC_RECVWINDOW                  "recvWindow"
    #define BNC_TIMESTAMP                   "timestamp"
    #define BNC_SIGNATURE                   "signature"
    #define BNC_APIKEY                      "X-MBX-APIKEY"

#elif defined(EXCHANGE_POLONIEX)
    #define EXCHANGE_STR                    "Poloniex"
    #define REST_OBJECT                     PoloREST
    #define EXCHANGE_SUBPATH                "pt"
    #define ORDER_STRING_SIZE               11
    #define INTERFACE_PORT                  62002

    #define POLO_URL_TRADE                  "https://poloniex.com/tradingApi"
    #define POLO_URL_PUBLIC                 "https://poloniex.com/public"
    #define POLO_URL_PRIVATE                "https://poloniex.com/private"
    #define POLO_URL_WSS                    "wss://api2.poloniex.com/"

    #define POLO_COMMAND_GETCHARTDATA       "returnChartData"
    #define POLO_COMMAND_GETBALANCES        "returnCompleteBalances"
    #define POLO_COMMAND_GETORDERS          "returnOpenOrders"
    #define POLO_COMMAND_GETORDERS_ARGS     "currencyPair=all"
    #define POLO_COMMAND_CANCEL             "cancelOrder"
    #define POLO_COMMAND_CANCEL_ARGS        "orderNumber="
    #define POLO_COMMAND_GETBOOKS           "returnOrderBook"
    #define POLO_COMMAND_GETBOOKS_ARGS      "currencyPair=all&depth=1"
    #define POLO_COMMAND_GETFEE             "returnFeeInfo"
#endif


//#define kDebug QMessageLogger( nullptr,0,0 ).debug().noquote
#define kDebug QMessageLogger( __FILE__, __LINE__, Q_FUNC_INFO ).debug().noquote


static const quint8 SIDE_BUY  = 1;
static const quint8 SIDE_SELL = 2;

static const quint8 CANCELLING_LOWEST             = 1;
static const quint8 CANCELLING_HIGHEST            = 2;
static const quint8 CANCELLING_FOR_SHORTLONG      = 3;
static const quint8 CANCELLING_FOR_DC             = 4;
static const quint8 CANCELLING_FOR_SLIPPAGE_RESET = 5;
static const quint8 CANCELLING_FOR_MAX_AGE        = 6;
static const quint8 CANCELLING_FOR_USER           = 7;

static const quint8 FILL_GETORDER = 1;
static const quint8 FILL_HISTORY  = 2;
static const quint8 FILL_TICKER   = 3;
static const quint8 FILL_CANCEL   = 4;
static const quint8 FILL_WSS      = 5;


namespace Global {

static inline const QString printVectorqint32( const QVector<qint32> &vec )
{
    QString ret = "[";

    for ( int i = 0; i < vec.size(); i++ )
        ret += QString( "%1, " )
                .arg( vec.value( i ) );

    // trim last comma
    if ( ret.size() > 1 )
        ret.truncate( ret.size() -2 );

    ret += "]";

    return ret;
}

static inline QString getBuildString()
{
    return QString( "exchange[%1]  bot version[%2]  qt[%3]  gmp[%4]  ssl[%5]  build time[%6 %7]" )
            .arg( EXCHANGE_STR )
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

static inline QString getExchangeNonce( qint64 *current_nonce, bool should_correct_nonce = false )
{
    // calculate a new nonce and compare it against the old nonce
    qint64 request_nonce_new = QDateTime::currentMSecsSinceEpoch();// 1423753004519 + QDateTime::currentMSecsSinceEpoch();
    QString request_nonce_str;

    // let a corrected nonce past incase we got a nonce error (allow multi-bot per key)
    if ( request_nonce_new <= *current_nonce &&
         should_correct_nonce )
    {
        // let it past incase we overwrote with new_nonce
        request_nonce_str = QString::number( ++*current_nonce );
    }
    else
    {
        request_nonce_str = QString::number( request_nonce_new );
        *current_nonce = request_nonce_new;
    }

    return request_nonce_str;
}

static inline const QString getConfigPath()
{
    return QStandardPaths::writableLocation( QStandardPaths::ConfigLocation );
}

static inline const QString getTraderPath()
{
    return getConfigPath() + QDir::separator() + EXCHANGE_SUBPATH;
}

static inline const QString getOldLogsPath()
{
    return getTraderPath() + QDir::separator() + "logs_old";
}

static inline const QString getIPCPath()
{
    return getTraderPath() + QDir::separator() + "trader.ipc";
}

static inline void ensurePath()
{
    // get dir ~/.config/<trader_dir>
    QString trader_path = getTraderPath();
    QString config_path = getConfigPath();
    QString oldlogs_path = getOldLogsPath();

    // make sure ~/.config and trader_path exists (trader_path is a subdirectory of config_path)
    QDir dir;
    bool ret0 = dir.exists( config_path ) || dir.mkdir( config_path );
    bool ret1 = dir.exists( trader_path ) || dir.mkdir( trader_path );
    bool ret2 = dir.exists( oldlogs_path ) || dir.mkdir( oldlogs_path );

    // if we failed, we have the wrong file permissions to continue
    if ( !ret0 || !ret1 || !ret2 )
        qCritical() << "local error: could not open config path" << trader_path << "(check file permissions)";
}

static inline void moveOldLogsOut()
{
    const QString &trader_path = getTraderPath();
    const QString &oldlogs_path = getOldLogsPath();

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
        assert( QFile::copy( from, to ) );
        assert( QFile::remove( from ) );
    }
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


static inline QString getOrderString( const QString &order_id )
{
    // for bittrex, shorten the id a bit by turning it into base64
#if defined(EXCHANGE_BITTREX)
    QByteArray b = QByteArray::fromHex( QString( order_id ).remove( QChar( '-' ) ).toUtf8() );
    return QString( b.toBase64( QByteArray::OmitTrailingEquals ) );
#else
    return order_id;
#endif
}


static void messageOutput( QtMsgType type, const QMessageLogContext &context, const QString &msg )
{
    // avoid warnings
    Q_UNUSED( messageOutput )
    Q_UNUSED( context )

    // add a logfile tag for test build
    QString testbuild_str;
#if defined(DEBUG_BUILD)
    testbuild_str += ".test";
#elif defined(DEBUG_BUILD_2)
    testbuild_str += ".test2";
#endif

    static QString log_file_path = QString( getTraderPath() + QDir::separator() + "log%1.%2.txt" )
                                    .arg( testbuild_str )
                                    .arg( QDateTime::currentMSecsSinceEpoch() );

    static QString log_color_file_path = QString( getTraderPath() + QDir::separator() + "log%1.%2_color.txt" )
                                    .arg( testbuild_str )
                                    .arg( QDateTime::currentMSecsSinceEpoch() );

    static QFile *log_file_handle = new QFile( log_file_path );
    static QFile *log_color_file_handle = new QFile( log_color_file_path );

    // open log file
    if ( !log_file_handle->isOpen() )
    {
        // make sure path exists
        ensurePath();

        // move old logs to <trader_dir>/old_logs/
        moveOldLogsOut();

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
#if defined(PRINT_LOGS_TO_CONSOLE) || defined(DEBUG_BUILD) || defined(DEBUG_BUILD_2)
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

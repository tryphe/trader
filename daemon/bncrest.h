#ifndef BNCREST_H
#define BNCREST_H

#include "build-config.h"

#if defined(EXCHANGE_BINANCE)

#include <QObject>
#include <QQueue>
#include <QHash>
#include <QMap>

#include "global.h"
#include "position.h"
#include "keystore.h"
#include "baserest.h"
#include "keydefs.h"

class QNetworkReply;
class QTimer;
class QWebSocket;

class BncREST : public BaseREST
{
    Q_OBJECT

public:
    explicit BncREST( Engine *_engine );
    ~BncREST();

    void init();

    bool yieldToFlowControl();
    bool yieldToLag();

public Q_SLOTS:
    void sendNamQueue();
    void onNamReply( QNetworkReply *const &reply );

    void onCheckBotOrders();
    void onCheckTicker();
    void onCheckExchangeInfo();

    void wssConnected();
    void wssCheckConnection();
    void wssTextMessageReceived( const QString &msg );
    void wssSendSubscriptions();

public:
    void sendNamRequest( Request *const &request );
    void sendBuySell( Position *const &pos, bool quiet = true );
    void sendCancel( const QString &_order_id, Position *const &pos = nullptr );
    void parseBuySell( Request *const &request, const QJsonObject &response );
    void parseCancelOrder( Request *const &request, const QJsonObject &response );
    void parseOpenOrders( const QJsonArray &markets, qint64 request_time_sent_ms );
    void parseReturnBalances( const QJsonObject &obj );
    void parseOrderBook( const QJsonArray &info, qint64 request_time_sent_ms );
    void parseExchangeInfo( const QJsonObject &obj );

    void wssSendJsonObj( const QJsonObject &obj );


    QWebSocket *wss;
    qint64 wss_connect_try_time;
    qint64 wss_heartbeat_time;

    // rate limit stuff
    qint32 binance_weight;

    QTimer *exchangeinfo_timer;

    // rate limit - track orders sent
    QMap<QString /*date MDY*/, qint32 /*num*/> daily_orders; // track daily orders

    qint64 wss_safety_delay_time;
    qint32 ratelimit_second, // orders limit
           ratelimit_minute, // weight limit
           ratelimit_day; // orders limit

    bool wss_1000_state, wss_1002_state;
    qint64 wss_account_feed_update_time;
};

#endif // EXCHANGE_BINANCE
#endif // BNCREST_H

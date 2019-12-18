#ifndef BNCREST_H
#define BNCREST_H

#include "build-config.h"

#include <QObject>
#include <QQueue>
#include <QHash>
#include <QMap>

#include "global.h"
#include "position.h"
#include "keystore.h"
#include "baserest.h"

class QNetworkReply;
class QTimer;
class QWebSocket;

class BncREST : public BaseREST
{
    Q_OBJECT

    friend class CommandRunner;

public:
    explicit BncREST( Engine *_engine, QNetworkAccessManager *_nam );
    ~BncREST();

    void init();

    bool yieldToLag() const;

    void sendNamRequest( Request *const &request );
    void sendBuySell( Position *const &pos, bool quiet = true );
    void sendCancel( const QString &_order_id, Position *const &pos = nullptr );
    void parseBuySell( Request *const &request, const QJsonObject &response );
    void parseCancelOrder( Request *const &request, const QJsonObject &response );
    void parseOpenOrders( const QJsonArray &markets, qint64 request_time_sent_ms );
    void parseReturnBalances( const QJsonObject &obj );
    void parseTicker( const QJsonArray &info, qint64 request_time_sent_ms );
    void parseExchangeInfo( const QJsonObject &obj );
    void wssSendJsonObj( const QJsonObject &obj );

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

private:
    QMap<QString, QString> market_aliases;
    QMap<QString /*date MDY*/, qint32 /*num*/> daily_orders; // track daily orders sent

    qint64 wss_connect_try_time{ 0 },
           wss_heartbeat_time{ 0 },
           wss_account_feed_update_time{ 0 },
           wss_safety_delay_time{ 2000 }; // only detect a wss filled order after this amount of time - for possible wss lag

    bool wss_1000_state{ false }, // account subscription
         wss_1002_state{ false }; // ticker subscription

    // rate limit stuff
    qint32 binance_weight{ 0 }, // current weight
           ratelimit_second{ 10 }, // orders limit
           ratelimit_minute{ 600 }, // weight limit
           ratelimit_day{ 100000 }; // orders limit

    QTimer *send_timer{ nullptr };
    QTimer *orderbook_timer{ nullptr };
    QTimer *ticker_timer{ nullptr };
    QTimer *exchangeinfo_timer{ nullptr };
    QWebSocket *wss{ nullptr };
};

#endif // BNCREST_H

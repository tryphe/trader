#ifndef POLOREST_H
#define POLOREST_H

#include "build-config.h"

#if defined(EXCHANGE_POLONIEX)

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

class PoloREST : public BaseREST
{
    Q_OBJECT

public:
    explicit PoloREST( Engine *_engine );
    ~PoloREST();

    void init();

    bool yieldToFlowControl();
    bool yieldToLag();

    void sendNamRequest( Request *const &request );
    void sendBuySell( Position *const &pos, bool quiet = true );
    void sendCancel( const QString &order_id, Position *const &pos = nullptr );

    void parseBuySell( Request *const &request, const QJsonObject &response );
    void parseCancelOrder( Request *const &request, const QJsonObject &response );
    void parseOpenOrders( const QJsonObject &markets, qint64 request_time_sent_ms );
    void parseReturnBalances( const QJsonObject &balances );
    void parseFeeInfo( const QJsonObject &info );
    void parseOrderBook( const QJsonObject &info, qint64 request_time_sent_ms );

    void wssSendJsonObj( const QJsonObject &obj );
    void setupCurrencyMap( QMap<qint32, QString> &m );

public Q_SLOTS:
    // timer slots
    void sendNamQueue();
    void onCheckBotOrders();
    void onCheckOrderBooks();
    void onCheckFee();

    // nam slots
    void onNamReply( QNetworkReply *const &reply );

    // websockets slots
    void wssConnected();
    void wssCheckConnection();
    void wssTextMessageReceived( const QString &msg );
    void wssSendSubscriptions();

public:
//private: // TODO: fix this
    QWebSocket *wss;
    qint64 wss_connect_try_time;
    qint64 wss_heartbeat_time;

    QTimer *fee_timer;
    QTimer *wss_timer;

    // currency ids for websocket feed
    QMap<qint32, QString> currency_name_by_id;

    QMap<QString /*market*/, qreal> slippage_multiplier;

    qint64 poloniex_throttle_time; // when we should wait until to sent the next request
    qint64 wss_safety_delay_time;

    bool wss_1000_state, wss_1002_state;
    qint64 wss_1000_subscribe_try_time, wss_1002_subscribe_try_time;
    qint64 wss_account_feed_update_time;
};

#endif // EXCHANGE_POLONIEX
#endif // POLOREST_H

#ifndef POLOREST_H
#define POLOREST_H

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

    friend class CommandRunner;

public:
    explicit PoloREST( Engine *_engine, QNetworkAccessManager *_nam );
    ~PoloREST();

    void init();

    bool yieldToLag() const;

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

    bool getWSS1000State() const { return wss_1000_state; }
    qreal getSlippageMul( const QString &market ) const { return slippage_multiplier.value( market, 0.005 ); }

    void checkBotOrders( bool ignore_flow_control = false );

public Q_SLOTS:
    // timer slots
    void sendNamQueue();
    void onCheckBotOrders();
    void onCheckTicker();
    void onCheckFee();

    // nam slots
    void onNamReply( QNetworkReply *const &reply );

    // websockets slots
    void wssConnected();
    void wssCheckConnection();
    void wssTextMessageReceived( const QString &msg );
    void wssSendSubscriptions();

private:
    QMap<qint32, QString> currency_name_by_id; // currency ids for websocket feed
    QMap<QString /*market*/, qreal> slippage_multiplier;

    bool wss_1000_state{ false }, // account subscription
         wss_1002_state{ false }; // ticker subscription

    qint64 wss_connect_try_time{ 0 },
           wss_heartbeat_time{ 0 },
           wss_1000_subscribe_try_time{ 0 },
           wss_1002_subscribe_try_time{ 0 },
           wss_account_feed_update_time{ 0 };

    qint64 poloniex_throttle_time{ 0 }; // when we should wait until to sent the next request

    QTimer *fee_timer{ nullptr };
    QTimer *wss_timer{ nullptr };
    QWebSocket *wss{ nullptr };
};

#endif // POLOREST_H

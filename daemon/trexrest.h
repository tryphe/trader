#ifndef TREXREST_H
#define TREXREST_H

#include "build-config.h"

#if defined(EXCHANGE_BITTREX)

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

class TrexREST : public BaseREST
{
    Q_OBJECT

public:
    explicit TrexREST( Engine *_engine );
    ~TrexREST();

    void init();

    bool yieldToLag() const;

public Q_SLOTS:
    void sendNamQueue();
    void onNamReply( QNetworkReply *const &reply );

    void onCheckBotOrders();
    void onCheckOrderHistory();
    void onCheckTicker();

    void wssConnected();
    void wssCheckConnection();
    void wssTextMessageReceived( const QString &msg );

public:
    void sendNamRequest( Request *const &request );
    void sendBuySell( Position *const &pos, bool quiet = true );
    void sendCancel( const QString &order_id, Position *const &pos = nullptr );
    void parseBuySell( Request *const &request, const QJsonObject &response );
    void parseCancelOrder( Request *const &request, const QJsonObject &response );
    void parseOpenOrders(const QJsonArray &orders, qint64 request_time_sent_ms );
    void parseReturnBalances( const QJsonArray &balances );
    void parseGetOrder( const QJsonObject &order );
    void parseOrderBook( const QJsonArray &info, qint64 request_time_sent_ms );
    void parseOrderHistory( const QJsonObject &obj );

    void wssSendJsonObj( const QJsonObject &obj );

private:
    qint64 order_history_update_time{ 0 };

    QTimer *order_history_timer{ nullptr };
};

#endif // EXCHANGE_BITTREX
#endif // TREXREST_H

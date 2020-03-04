#ifndef WAVESREST_H
#define WAVESREST_H

#include <QObject>

#include "global.h"
#include "position.h"
#include "keystore.h"
#include "baserest.h"
#include "wavesaccount.h"

class QNetworkReply;
class QTimer;

class WavesREST : public BaseREST
{
    Q_OBJECT

    friend class CommandRunner;

public:
    explicit WavesREST( Engine *_engine, QNetworkAccessManager *_nam );
    ~WavesREST();

    void init();

    void sendNamRequest( Request *const &request );

    void getOrderStatus( Position *const &pos );

    void sendCancel( const QString &order_id, Position *const &pos , const Market &market );
    void sendCancelNonLocal( const QString &order_id , const QString &amount_asset_alias, const QString &price_asset_alias );
    void sendBuySell( Position *const &pos, bool quiet = true );

public Q_SLOTS:
    void sendNamQueue();
    void onNamReply( QNetworkReply *const &reply );

    void onCheckMarketData();
    void onCheckTicker();
    void onCheckBotOrders();
    void onCheckCancellingOrders();

private:
    void checkTicker( bool ignore_flow_control = false );

    void parseMarketData( const QJsonObject &info );
    void parseMarketStatus( const QJsonObject &info, Request *const &request );
    void parseOrderStatus( const QJsonObject &info, Request *const &request );
    void parseCancelOrder( const QJsonObject &info, Request *const &request );
    void parseNewOrder( const QJsonObject &info, Request *const &request );
    void parseMyOrders( const QJsonArray &orders, qint64 request_time_sent_ms );

    WavesAccount account;

    QStringList tracked_markets;

    QVector<Position*> cancelling_orders_to_query;

    bool initial_ticker_update_done{ false };
    qint32 next_ticker_index_to_query{ 0 };
    qint32 last_cancelling_index_checked{ 0 };
    QTimer *market_data_timer{ nullptr };
};

#endif // WAVESREST_H

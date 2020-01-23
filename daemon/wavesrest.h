#ifndef WAVESREST_H
#define WAVESREST_H

#include <QObject>

#include "global.h"
#include "position.h"
#include "keystore.h"
#include "baserest.h"

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

public Q_SLOTS:
    void sendNamQueue();
    void onNamReply( QNetworkReply *const &reply );

    void onCheckMarketData();
    void onCheckTicker();
    void onCheckBotOrders();

private:
    void parseMarketData( const QJsonObject &info );
    void parseOrderBookData( const QJsonObject &info );

    QMap<QString,QString> asset_by_alias, alias_by_asset;
    QStringList price_assets, tracked_markets;

    bool initial_ticker_update_done{ false };
    qint32 next_ticker_index_to_query{ 0 };
    QTimer *market_data_timer{ nullptr };
};

#endif // WAVESREST_H

#ifndef ENGINE_H
#define ENGINE_H

#include "misctypes.h"
#include "position.h"
#include "baserest.h"
#include "coinamount.h"

#include <QObject>
#include <QNetworkReply>

class REST_OBJECT;
class Stats;
class WSInterface;
class PositionMan;
class EngineSettings;

class Engine : public QObject
{
    Q_OBJECT

    friend class EngineTest;
    friend class PositionMan;

public:
    explicit Engine();
    ~Engine();

    Position *addPosition( QString market, quint8 side, QString buy_price , QString sell_price,
                           QString order_size, QString type = "active", QString strategy_tag = QLatin1String(),
                           QVector<qint32> indices = QVector<qint32>(), bool landmark = false, bool quiet = false );

    void processFilledOrders( QVector<Position*> &filled_positions, qint8 fill_type );

    // post-parse processing stuff
    void processOpenOrders( QVector<QString> &order_numbers, QMultiHash<QString, OrderInfo> &orders, qint64 request_time_sent_ms );
    void processTicker( const QMap<QString, TickerInfo> &ticker_data, qint64 request_time_sent_ms = 0 );
    void processCancelledOrder( Position *const &pos );

    // user/local commands
    void cancelOrder( Position *const &pos, bool quiet = false, quint8 cancel_reason = 0 );
    void cancelAll( QString market );
    void cancelLocal( QString market = "" );
    void cancelHighest( const QString &market );
    void cancelLowest( const QString &market );
    //void cancelOrderByPrice( const QString &market, QString price );
    //void setOrderMeat( Position *const &pos, QString order_number );
    void saveMarket( QString market, qint32 num_orders = 15 );

    void setNextLowest( const QString &market, quint8 side = SIDE_BUY, bool landmark = false );
    void setNextHighest( const QString &market, quint8 side = SIDE_SELL, bool landmark = false );

    void flipHiBuyPrice( const QString &market, QString tag = QLatin1String() );
    void flipHiBuyIndex( const QString &market, QString tag = QLatin1String() );
    void flipLoSellPrice( const QString &market, QString tag = QLatin1String() );
    void flipLoSellIndex( const QString &market, QString tag = QLatin1String() );
    Coin getLoSell( const QString &market ) const;
    Coin getHiBuy( const QString &market ) const;
    Coin getHiBuyFlipPrice( const QString &market ) const;
    Coin getLoSellFlipPrice( const QString &market ) const;

    qint32 getMarketOrderTotal( const QString &market, bool onetime_only = false ) const;
    qint32 getBuyTotal( const QString &market ) const;
    qint32 getSellTotal( const QString &market ) const;

    PositionMan *positions;
    EngineSettings *settings;

    // utility functions
    void setStats( Stats *_stats ) { stats = _stats; }
    Stats *getStats() const { return stats; }
    void printInternal();

    void setRest( REST_OBJECT *_rest ) { rest = _rest; }
    REST_OBJECT *getRest() const { return rest; }

    void setMaintenanceTime( qint64 time ) { maintenance_time = time; }
    qint64 getMaintenanceTime() const { return maintenance_time; }

    QHash<QString, MarketInfo> &getMarketInfoStructure() { return market_info; }
    MarketInfo &getMarketInfo( const QString &market ) { return market_info[ market ]; }

    void setTesting( bool testing ) { is_testing = testing; }
    bool isTesting() const { return is_testing; }
    void setVerbosity( int v ) { verbosity = v; }

    void setMarketSettings( QString market, qint32 order_min, qint32 order_max, qint32 order_dc, qint32 order_dc_nice,
                            qint32 landmark_start, qint32 landmark_thresh, bool market_sentiment, qreal market_offset );

    void findBetterPrice( Position *const &pos );
    void deleteReply( QNetworkReply *const &reply, Request *const &request );

public Q_SLOTS:
    void onCheckTimeouts();
    void onCheckDivergeConverge();

private:
    // timer routines
    void cleanGraceTimes();
    void checkBuySellCount();
    void checkMaintenance();

    void converge( QMap<QString/*market*/,QVector<qint32>> &market_map, quint8 side );
    void diverge( QMap<QString/*market*/,QVector<qint32>> &market_map );

    Position *getPositionByIndex( const QString &market, const qint32 idx ) const;
    Position *getHighestActiveBuyPosByIndex( const QString &market ) const;
    Position *getHighestActiveSellPosByIndex( const QString &market ) const;
    Position *getLowestActiveSellPosByIndex( const QString &market ) const;
    Position *getLowestActiveBuyPosByIndex( const QString &market ) const;
    Position *getHighestActiveBuyPosByPrice( const QString &market ) const;
    Position *getLowestActiveSellPosByPrice( const QString &market ) const;
    Position *getLowestActivePingPong( const QString &market ) const;
    Position *getHighestActivePingPong( const QString &market ) const;
    Coin getHighestBuyPrice( const QString &market ) const;
    Coin getLowestSellPrice( const QString &market ) const;
    inline bool isIndexDivergingConverging( const QString &market, const qint32 index ) const;
    void addLandmarkPositionFor( Position *const &pos );
    void flipPosition( Position *const &pos );
    void cancelOrderMeatDCOrder( Position *const &pos );
    bool tryMoveOrder( Position *const &pos );
    void fillNQ( const QString &order_id, qint8 fill_type, quint8 extra_data = 0 );


    QHash<QString, MarketInfo> market_info;

    // other
    QHash<QString/*order_id*/, qint64/*seen_time*/> order_grace_times; // record "seen" time to allow for grace period before we remove stray orders

    // state for cancel commands
    bool is_running_cancelall;
    QString cancel_market_filter;

    // state for maintenance
    qint64 maintenance_time;
    bool maintenance_triggered;

    // state for initial runtime tests
    bool is_testing;
    int verbosity; // 0 = none, 1 = normal, 2 = extra

    REST_OBJECT *rest;
    Stats *stats;
};

#endif // ENGINE_H

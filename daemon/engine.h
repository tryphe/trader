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

struct EngineSettings
{
    explicit EngineSettings()
    {
        is_chatty = false;
        should_clear_stray_orders = false /*false*/; // auto cancels orders that aren't ours - set false when multiple bots are running the same api key
        should_clear_stray_orders_all = false; // cancel orders not in our price index
        should_slippage_be_calculated = false; // try calculated slippage before additive. false = additive + additive2 only
        should_adjust_hibuy_losell = true; // adjust hi_buy/lo_sell maps based on post-only price errors
        should_adjust_hibuy_losell_debugmsgs_ticker = false; // enable chatty messages for hi/lo bounds adjust for wss-ticker
        should_mitigate_blank_orderbook_flash = true;
        should_dc_slippage_orders = false;
        should_use_aggressive_spread = true;
        request_timeout = 3 * 60000; // how long before we resend most requests
        cancel_timeout = 6 * 60000; // how long before we resend a cancel request
        stray_grace_time_limit = 0; // time to allow stray orders to stick around before we cancel them. this is also the re-cancel time (keep it largeish)
        safety_delay_time = 0; // allow for a safe period to avoid orders we just set possibly not showing up yet
        ticker_safety_delay_time = 0; // only accept ticker after this amount of time
    }

    Coin fee;

    bool is_chatty;
    bool should_clear_stray_orders;
    bool should_clear_stray_orders_all;
    bool should_slippage_be_calculated; // calculated/additive preference of slippage
    bool should_adjust_hibuy_losell;
    bool should_adjust_hibuy_losell_debugmsgs_ticker;
    bool should_mitigate_blank_orderbook_flash;
    bool should_dc_slippage_orders;
    bool should_use_aggressive_spread;
    qint64 request_timeout;
    qint64 cancel_timeout;
    qint64 stray_grace_time_limit;
    qint64 safety_delay_time;
    qint64 ticker_safety_delay_time;
};

class Engine : public QObject
{
    Q_OBJECT

    friend class EngineTest;

public:
    explicit Engine();
    ~Engine();

    // position stuff
    bool hasActivePositions() const;
    bool hasQueuedPositions() const;

    bool isActivePosition( Position *const &pos ) const;
    bool isQueuedPosition( Position *const &pos ) const;
    bool isPosition( Position *const &pos ) const;
    bool isPositionOrderID( const QString &order_id ) const;
    Position *getPositionForOrderID( const QString &order_id ) const;

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
    void setOrderMeat( Position *const &pos, QString order_number );
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

    // utility functions
    void setStats( Stats *_stats ) { stats = _stats; }
    Stats *getStats() const { return stats; }
    void printInternal();

    void setRest( REST_OBJECT *_rest ) { rest = _rest; }
    REST_OBJECT *getRest() const { return rest; }

    void setMaintenanceTime( qint64 time ) { maintenance_time = time; }
    qint64 getMaintenanceTime() const { return maintenance_time; }

    QSet<Position*> &positionsAll() { return positions_all; }

    EngineSettings &settings() { return m_settings; }

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
    void removeFromDC( Position *const &pos );
    bool tryMoveOrder( Position *const &pos );
    void fillNQ( const QString &order_id, qint8 fill_type, quint8 extra_data = 0 );
    void deletePosition( Position * const &pos );

    QHash<QString, MarketInfo> market_info;

    // maintain a map of queued positions and set positions
    QHash<QString /* orderNumber */, Position*> positions_by_number;
    QSet<Position*> positions_active; // ptr list of active positions
    QSet<Position*> positions_queued; // ptr list of queued positions
    QSet<Position*> positions_all; // active and queued

    // internal dc stuff
    QMap<QVector<Position*>/*waiting for cancel*/, QPair<bool/*is_landmark*/,QVector<qint32>/*indices*/>> diverge_converge;
    QMap<QString/*market*/, QVector<qint32>/*reserved idxs*/> diverging_converging; // store a vector of converging/diverging indices

    // other
    QHash<QString/*order_id*/, qint64/*seen_time*/> order_grace_times; // record "seen" time to allow for grace period before we remove stray orders

    EngineSettings m_settings;

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

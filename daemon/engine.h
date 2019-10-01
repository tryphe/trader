#ifndef ENGINE_H
#define ENGINE_H

#include "misctypes.h"
#include "position.h"
#include "baserest.h"
#include "coinamount.h"
#include "spruce.h"

#include <QObject>
#include <QNetworkReply>

class REST_OBJECT;
class Stats;
class PositionMan;
class EngineSettings;

class Engine : public QObject
{
    Q_OBJECT

    friend class EngineTest;

public:
    explicit Engine();
    ~Engine();

    Position *addPosition( QString market_input, quint8 side, QString buy_price , QString sell_price,
                           QString order_size, QString type = ACTIVE, QString strategy_tag = QLatin1String(),
                           QVector<qint32> indices = QVector<qint32>(), bool landmark = false, bool quiet = false );

    void processFilledOrders( QVector<Position*> &to_be_filled, qint8 fill_type );

    // post-parse processing stuff
    void processOpenOrders( QVector<QString> &order_numbers, QMultiHash<QString, OrderInfo> &orders, qint64 request_time_sent_ms );
    void processTicker( const QMap<QString, TickerInfo> &ticker_data, qint64 request_time_sent_ms = 0 );
    void processCancelledOrder( Position *const &pos );

    //void cancelOrderByPrice( const QString &market, QString price );
    void saveMarket( QString market, qint32 num_orders = 15 );
    void saveSettings();
    void loadSettings();
    void saveStats();
    void loadStats();

    Spruce &getSpruce() { return spruce; }
    PositionMan *getPositionMan() const { return positions; }
    EngineSettings *getSettings() const { return settings; }

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

    bool hasWSSInterface() const { return wss_interface; }
    void setTesting( bool testing ) { is_testing = testing; }
    bool isTesting() const { return is_testing; }
    void setVerbosity( int v ) { verbosity = v; }
    int getVerbosity() const { return verbosity; }

    void setMarketSettings( QString market, qint32 order_min, qint32 order_max, qint32 order_dc, qint32 order_dc_nice,
                            qint32 landmark_start, qint32 landmark_thresh, bool market_sentiment, qreal market_offset );

    void findBetterPrice( Position *const &pos );

signals:
    void newEngineMessage( QString &str ); // new wss message
    void gotUserCommandChunk( QString &s ); // loaded settings file

public Q_SLOTS:
    void onEngineMaintenance();
    void onCheckTimeouts();
    void onSpruceUp();
    void handleUserMessage( const QString &str );

private:
    QPair<Coin,Coin> getSpruceSpread( const QString &market );

    // timer routines
    void cleanGraceTimes();
    void checkMaintenance();

    void addLandmarkPositionFor( Position *const &pos );
    void flipPosition( Position *const &pos );
    void cancelOrderMeatDCOrder( Position *const &pos );
    bool tryMoveOrder( Position *const &pos );
    void fillNQ( const QString &order_id, qint8 fill_type, quint8 extra_data = 0 );

    Coin getPriceForMarket( quint8 side, const QString &currency, const QString &base );

    Spruce spruce;
    QHash<QString, MarketInfo> market_info;
    QHash<QString/*order_id*/, qint64/*seen_time*/> order_grace_times; // record "seen" time to allow for stray grace period

    // state
    qint64 maintenance_time{ 0 };
    bool maintenance_triggered{ false };
    bool is_testing{ false };
    int verbosity{ 1 }; // 0 = none, 1 = normal, 2 = extra
    bool wss_interface{ false };

    PositionMan *positions{ nullptr };
    EngineSettings *settings{ nullptr };
    QTimer *maintenance_timer{ nullptr };

    REST_OBJECT *rest{ nullptr };
    Stats *stats{ nullptr };
};

#endif // ENGINE_H

#ifndef ENGINE_H
#define ENGINE_H

#include "misctypes.h"
#include "position.h"
#include "baserest.h"
#include "coinamount.h"
#include "spruce.h"

#include <QObject>
#include <QNetworkReply>

class CommandRunner;
class CommandListener;
class AlphaTracker;
class PositionMan;
class EngineSettings;

class TrexREST;
class BncREST;
class PoloREST;

class Engine : public QObject
{
    Q_OBJECT

    friend class EngineTest;

public:
    explicit Engine( const quint8 _engine_type );
    ~Engine();

    Position *addPosition( QString market_input, quint8 side, QString buy_price , QString sell_price,
                           QString order_size, QString type = ACTIVE, QString strategy_tag = QLatin1String(),
                           QVector<qint32> indices = QVector<qint32>(), bool landmark = false, bool quiet = false );

    void processFilledOrders( QVector<Position*> &to_be_filled, qint8 fill_type );

    // post-parse processing stuff
    void processOpenOrders( QVector<QString> &order_numbers, QMultiHash<QString, OrderInfo> &orders, qint64 request_time_sent_ms );
    void processTicker( const QMap<QString, TickerInfo> &ticker_data, qint64 request_time_sent_ms = 0 );
    void processCancelledOrder( Position *const &pos );

    void saveMarket( QString market, qint32 num_orders = 15 );
    void saveSettings();
    void loadSettings();
    void saveStats();
    void loadStats();

    Spruce *getSpruce() { return spruce; }
    PositionMan *getPositionMan() const { return positions; }
    EngineSettings *getSettings() const { return settings; }

    QString getSettingsPath() const { return engine_type == ENGINE_BITTREX  ? Global::getBittrexSettingsPath() :
                                             engine_type == ENGINE_BINANCE  ? Global::getBinanceSettingsPath() :
                                             engine_type == ENGINE_POLONIEX ? Global::getPoloniexSettingsPath() :
                                                                               QString(); }

    void printInternal();

    void setMaintenanceTime( qint64 time ) { maintenance_time = time; }
    qint64 getMaintenanceTime() const { return maintenance_time; }

    QDateTime getStartTime() const { return start_time; }

    QHash<QString, MarketInfo> &getMarketInfoStructure() { return market_info; }
    MarketInfo &getMarketInfo( const QString &market ) { return market_info[ market ]; }

    void setTesting( bool testing ) { is_testing = testing; }
    bool isTesting() const { return is_testing; }
    void setVerbosity( int v ) { verbosity = v; }
    int getVerbosity() const { return verbosity; }

    void setMarketSettings( QString market, qint32 order_min, qint32 order_max, qint32 order_dc, qint32 order_dc_nice,
                            qint32 landmark_start, qint32 landmark_thresh, bool market_sentiment, qreal market_offset );

    void findBetterPrice( Position *const &pos );

    void sendBuySell( Position *const &pos, bool quiet = false );
    void sendCancel( const QString &order_number, Position *const &pos );
    void sendNamQueue();
    bool yieldToFlowControl();

    void updateStatsAndPrintFill( const QString &fill_type, const Market &market, const QString &order_id, const quint8 side,
                                  const QString &strategy_tag, const Coin &btc_amount, const Coin &price, const Coin &btc_commission,
                                  bool partial_fill );

    QMultiMap<qint64/*time thresh*/,QString/*order_id*/> cancelled_orders_for_polling;

    quint8 engine_type{ 0 };
    TrexREST *rest_trex{ nullptr };
    BncREST *rest_bnc{ nullptr };
    PoloREST *rest_polo{ nullptr };
    AlphaTracker *alpha{ nullptr };
    Spruce *spruce{ nullptr };

signals:
    void newEngineMessage( QString &str ); // new wss message
    void gotUserCommandChunk( QString &s ); // loaded settings file

public Q_SLOTS:
    void onEngineMaintenance();
    void onCheckTimeouts();
    void onSpruceUp();
    void handleUserMessage( const QString &str );

private:
    QPair<Coin,Coin> getSpruceSpread( const QString &market, quint8 side );
    QPair<Coin,Coin> getSpruceSpreadLimit( const QString &market, quint8 side );

    // timer routines
    void cleanGraceTimes();
    void checkMaintenance();
    void autoSaveSpruceSettings();

    void addLandmarkPositionFor( Position *const &pos );
    void flipPosition( Position *const &pos );
    void cancelOrderMeatDCOrder( Position *const &pos );
    bool tryMoveOrder( Position *const &pos );
    void fillNQ( const QString &order_id, qint8 fill_type, quint8 extra_data = 0 );

    QPair<Coin,Coin> getSpreadForMarket( const QString &market );
    QHash<QString, MarketInfo> market_info;
    QHash<QString/*order_id*/, qint64/*seen_time*/> order_grace_times; // record "seen" time to allow for stray grace period

    QDateTime start_time;

    // primitives
    qint64 maintenance_time{ 0 };
    bool maintenance_triggered{ false };
    bool is_testing{ false };
    int verbosity{ 1 }; // 0 = none, 1 = normal, 2 = extra

    PositionMan *positions{ nullptr };
    EngineSettings *settings{ nullptr };

    QTimer *maintenance_timer{ nullptr };
    QTimer *autosave_timer{ nullptr };
};

#endif // ENGINE_H

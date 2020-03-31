#ifndef ENGINE_H
#define ENGINE_H

#include "misctypes.h"
#include "position.h"
#include "baserest.h"
#include "coinamount.h"

#include <QObject>
#include <QNetworkReply>

class Spruce;
class CommandRunner;
class CommandListener;
class AlphaTracker;
class PositionMan;
class EngineSettings;

class TrexREST;
class BncREST;
class PoloREST;
class WavesREST;

class Engine : public QObject
{
    Q_OBJECT

    friend class EngineTest;
    friend class SpruceOverseer;
    friend class SpruceOverseerTest;

public:
    explicit Engine( const quint8 _engine_type );
    ~Engine();

    Position *addPosition( QString market_input, quint8 side, QString buy_price , QString sell_price,
                           QString order_size, QString type = ACTIVE, QString strategy_tag = QLatin1String(),
                           QVector<qint32> indices = QVector<qint32>(), bool landmark = false, bool quiet = false );

    void processFilledOrders( QVector<Position*> &to_be_filled, qint8 fill_type );

    // post-parse processing stuff
    void processOpenOrders( QVector<QString> &order_numbers, QMultiHash<QString, OrderInfo> &orders, qint64 request_time_sent_ms );
    void processTicker( BaseREST *base_rest_module, const QMap<QString, TickerInfo> &ticker_data, qint64 request_time_sent_ms = 0 );
    void processCancelledOrder( Position *const &pos );

    void saveMarket( QString market, qint32 num_orders = 15 );
    void loadSettings();

    PositionMan *getPositionMan() const { return positions; }
    EngineSettings *getSettings() const { return settings; }

    QString getSettingsPath() const { return engine_type == ENGINE_BITTREX  ? Global::getBittrexSettingsPath() :
                                             engine_type == ENGINE_BINANCE  ? Global::getBinanceSettingsPath() :
                                             engine_type == ENGINE_POLONIEX ? Global::getPoloniexSettingsPath() :
                                             engine_type == ENGINE_WAVES    ? Global::getWavesSettingsPath() :
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
    void sendCancel( const QString &order_number, Position *const &pos, const Market &market = Market() );
    bool yieldToFlowControl();

    void updateStatsAndPrintFill( const QString &fill_type, Market market, const QString &order_id, quint8 side,
                                  const QString &strategy_tag, Coin amount, Coin quantity, Coin price,
                                  const Coin &btc_commission );

    QVector<QString/*order_id*/> orders_for_polling;

    quint8 engine_type{ 0 };
    Spruce *spruce{ nullptr };
    QVector<BaseREST*> rest_arr;
    AlphaTracker *alpha{ nullptr };

signals:
    void newEngineMessage( QString &str ); // new wss message
    void gotUserCommandChunk( QString &s ); // loaded settings file

public Q_SLOTS:
    void onEngineMaintenance();
    void onCheckTimeouts();

private:
    // timer routines
    void cleanGraceTimes();
    void checkMaintenance();

    void addLandmarkPositionFor( Position *const &pos );
    void flipPosition( Position *const &pos );
    void cancelOrderMeatDCOrder( Position *const &pos );
    bool tryMoveOrder( Position *const &pos );
    void fillNQ( const QString &order_id, qint8 fill_type, quint8 extra_data = 0 );

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
};

#endif // ENGINE_H

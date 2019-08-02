#ifndef STATS_H
#define STATS_H

#include <QString>
#include <QMap>
#include <QPair>

#include "global.h"
#include "coinamount.h"

class QObject;
class TrexREST;
class PoloREST;
class BncREST;
class Position;
class Engine;

//struct MarketStats
//{
//    explicit MarketStats()
//        : market_profit_fills( 0 ), // track each profit margin for each market
//          market_fills( 0 ), // track local market volume
//          daily_fills( 0 )
//    {

//    }

//    Coin market_volume; // track local market volume
//    Coin market_profit; // track total profit per market
//    Coin daily_market_volume; // track daily profit per market
//    Coin daily_market_profit; // track daily profit per market
//    Coin daily_volumes; // track daily volume total
//    Coin daily_profit; // track daily profit total
//    Coin market_shortlong;
//    QString last_price; // track last seen price for each market

//    qint64 market_profit_fills; // track each profit margin for each market
//    qint64 market_fills; // track local market volume
//    qint32 daily_fills;
//};

class Stats
{
public:
    explicit Stats( Engine *_engine, REST_OBJECT *_rest = nullptr );
    ~Stats();

    void updateStats( Position *const &pos );
    void clearSome( const QString &market );
    void clearAll();

    void printPositions(QString market);
    void printOrdersByIndex( QString market );
    void printOrders( QString market );
    void printVolumes();
    void printDailyVolumes();
    void printDailyFills();
    void printFills();
    //void printOrdersTotal();
    void printLastPrices();
    void printBuySellTotal();
    void printStrategyShortLong( QString strategy_tag );
    void printProfit();
    void printDailyProfit();
    void printMarketProfit();
    void printDailyMarketProfit();
    void printDailyMarketVolume();
    void printDailyMarketProfitVolume();
    void printMarketProfitVolume();
    void printMarketProfitFills();
    void printDailyMarketProfitRW();
    void printMarketProfitRW();

    QMap<QString /*strat*/, QMap<QString/*currency*/,Coin/*short-long*/>> shortlong;

private:
    //QMap<QString, MarketStats> market_stats;
    QMap<QString /*market*/, Coin /*volume*/> market_volumes; // track local market volume
    QMap<QString /*market*/, Coin /*profit*/> market_profit; // track total profit per market
    QMap<QString /*market*/, qint64/*volume*/> market_profit_fills; // track each profit margin for each market
    QMap<QString /*market*/, Coin /*volume*/> daily_market_volume; // track daily profit per market
    QMap<QString /*market*/, Coin /*profit*/> daily_market_profit; // track daily profit per market
    QMap<QString /*market*/, Coin /*volume*/> daily_volumes; // track daily volume total
    QMap<QString /*market*/, Coin /*profit*/> daily_profit; // track daily profit total
    QMap<QString /*market*/, qint64/*orders*/> market_fills; // track local market volume
    QMap<QString /*market*/, QString/*price*/> last_price; // track last seen price for each market
    QMap<QString /*market*/, qint32 /*num*/> daily_fills;

    // stats to decide new order multipliers, risk-reward and market-volume
    QMap<QString /*market-profit*/, QPair<Coin,quint64> /*total, count*/> market_profit_risk_reward;
    QMap<QString /*day-market-profit*/, QPair<Coin,quint64> /*total, count*/> daily_market_profit_risk_reward;
    QMap<QString /*market*/, Coin /*volume*/> daily_market_profit_volume; // track daily profit per market
    QMap<QString /*market*/, Coin /*volume*/> market_profit_volume; // track each profit margin for each market

    Engine *engine;
    REST_OBJECT *rest;
};

#endif // STATS_H

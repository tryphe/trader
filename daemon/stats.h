#ifndef STATS_H
#define STATS_H

#include <QString>
#include <QMap>
#include <QPair>

#include "global.h"
#include "coinamount.h"
#include "alphatracker.h"

class QObject;
class TrexREST;
class PoloREST;
class BncREST;
class Position;
class Engine;

class Stats
{
public:
    explicit Stats( Engine *_engine, REST_OBJECT *_rest = nullptr );
    ~Stats();

    void updateStats( Position *const &pos );
    void addStrategyStats( Position *const &pos );
    void clearAll();

    void printPositions(QString market);
    void printOrdersByIndex( QString market );
    void printOrders( QString market );
    void printDailyVolumes();
    void printDailyFills();
    void printLastPrices();
    void printBuySellTotal();
    void printStrategyShortLong( QString strategy_tag );
    void printDailyMarketVolume();

    QMap<QString /*strat*/, QMap<QString/*currency*/,Coin/*short-long*/>> shortlong;

    // new way to track trade profit
    AlphaTracker alpha;

private:
    QMap<QString /*market*/, Coin /*volume*/> daily_market_volume; // track daily profit per market
    QMap<QString /*market*/, Coin /*volume*/> daily_volumes; // track daily volume total
    QMap<QString /*market*/, QString/*price*/> last_price; // track last seen price for each market
    QMap<QString /*market*/, qint32 /*num*/> daily_fills;

    Engine *engine;
    REST_OBJECT *rest;
};

#endif // STATS_H

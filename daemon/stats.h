#ifndef STATS_H
#define STATS_H

#include <QString>
#include <QMap>

#include "global.h"
#include "coinamount.h"
#include "alphatracker.h"

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

    void updateStats( Position *const &pos , bool partial_fill = false );
    void addStrategyStats( Position *const &pos );
    void clearAll();

    void printPositions( QString market );
    void printOrders( const QString &market, bool by_index = false );
    void printDailyVolumes();
    void printDailyFills();
    void printLastPrices();
    void printBuySellTotal();
    void printStrategyShortLong( QString strategy_tag );
    void printDailyMarketVolume();

    AlphaTracker &alpha() { return m_alpha; }

private:
    AlphaTracker m_alpha;

    QMap<QString /*strat*/, QMap<QString/*currency*/,Coin/*short-long*/>> shortlong;
    QMap<QString /*market*/, Coin /*volume*/> daily_market_volume; // track daily profit per market
    QMap<QString /*market*/, Coin /*volume*/> daily_volumes; // track daily volume total
    QMap<QString /*market*/, QString/*price*/> last_price; // track last seen price for each market
    QMap<QString /*market*/, qint32 /*num*/> daily_fills;

    Engine *engine;
    REST_OBJECT *rest;
};

#endif // STATS_H

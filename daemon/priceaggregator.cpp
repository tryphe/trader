#include "priceaggregator.h"
#include "misctypes.h"
#include "enginemap.h"
#include "engine.h"

#include <QString>
#include <QMap>
#include <QDateTime>

PriceAggregator::PriceAggregator()
{

}

Spread PriceAggregator::getSpread( const QString &market )
{
    static const bool prices_uses_avg = true; // false = assemble widest combined spread between all exchanges, true = average spreads between all exchanges

    Spread ret;
    quint16 samples = 0;

    for ( EngineMap::const_iterator i = m_engine_map->begin(); i != m_engine_map->end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->getMarketInfoStructure().contains( market ) )
            continue;

        // ensure ticker isn't stale
        if ( engine->rest_arr.at( engine->engine_type )->ticker_update_time
             < QDateTime::currentMSecsSinceEpoch() - 60000 )
        {
            //kDebug() << "local warning: engine ticker" << engine->engine_type << "is stale, skipping";
            continue;
        }

        const MarketInfo &info = engine->getMarketInfo( market );

        // ensure prices are valid
        if ( !info.spread.isValid() )
            continue;

        // use avg spread
        if ( prices_uses_avg )
        {
            samples++;

            // incorporate prices of this exchange
            ret.bid += info.spread.bid;
            ret.ask += info.spread.ask;
        }
        // or, use combined spread edges
        else
        {
            // incorporate bid price of this exchange
            if ( ret.bid.isZeroOrLess() || // bid doesn't exist yet
                 ret.bid > info.spread.bid ) // bid is higher than the exchange bid
                ret.bid = info.spread.bid;

            // incorporate ask price of this exchange
            if ( ret.ask.isZeroOrLess() || // ask doesn't exist yet
                 ret.ask < info.spread.ask ) // ask is lower than the exchange ask
                ret.ask = info.spread.ask;
        }
    }

    if ( prices_uses_avg )
    {
        // on 0 samples, return here
        if ( samples < 1 )
            return Spread();

        // divide by num of samples if necessary
        if ( samples > 1 )
        {
            ret.bid /= samples;
            ret.ask /= samples;
        }
    }

    if ( !ret.isValid() )
        return Spread();

    return ret;
}

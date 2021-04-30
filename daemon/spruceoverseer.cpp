#include "spruceoverseer.h"
#include "sprucev2.h"
#include "alphatracker.h"
#include "position.h"
#include "coinamount.h"
#include "misctypes.h"
#include "market.h"
#include "enginemap.h"
#include "engine.h"
#include "positionman.h"
#include "priceaggregator.h"

#include <QString>
#include <QVector>
#include <QMap>
#include <QList>
#include <QSet>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QTimer>

const bool expand_spread_base_down = false; // true = getSpreadForSide always expands down for base greed value before applying other effects

SpruceOverseer::SpruceOverseer( EngineMap *_engine_map, PriceAggregator *_price_aggregator, SpruceV2 *_spruce )
    : QObject( nullptr ),
    engine_map( _engine_map ),
    price_aggregator( _price_aggregator ),
    spruce( _spruce )
{
    kDebug() << "[SpruceOverseer]";

    // this timer does tit-for-tat
    spruce_timer = new QTimer( this );
    connect( spruce_timer, &QTimer::timeout, this, &SpruceOverseer::onSpruceUp );
    spruce_timer->setTimerType( Qt::VeryCoarseTimer );
    spruce_timer->start( spruce->getIntervalSecs() * 1000 );

    // autosave spruce settings
    autosave_timer = new QTimer( this );
    connect( autosave_timer, &QTimer::timeout, this, &SpruceOverseer::onBackupAndSave );
    autosave_timer->setTimerType( Qt::VeryCoarseTimer );
    autosave_timer->start( 60000 * 60 ); // set default to 1hr

    m_phase_man.addPhase( NO_FLUX ); // run one phase for all markets with no flux
    m_phase_man.addPhase( FLUX_PER_MARKET ); // run one phase for each market with the flux in the selected market
}

SpruceOverseer::~SpruceOverseer()
{
    spruce_timer->stop();
    autosave_timer->stop();

    delete spruce_timer;
    delete autosave_timer;
}

void SpruceOverseer::onSpruceUp()
{
    if ( !spruce->isActive() )
        return;

    // save midspread prices to set m_current_prices to after (so getBaseCapital uses midspread prices)
    QMap<QString, Coin> midspread_prices;

    m_last_midspread_output.clear();

    // cache mid spread for each market (needed for noflux phase optimization)
    QMap<QString, Coin> spread_midprice;

    // cache markets. if we altered the market count, update markets
    const QVector<QString> alpha_markets = spruce->getMarketsAlpha();
    if ( m_phase_man.getMarketCount() < alpha_markets.size() )
        m_phase_man.setMarkets( alpha_markets );

    // go through each phase for each side for each market relevant to each phase
    for ( m_phase_man.begin(); !m_phase_man.atEnd(); m_phase_man.next() )
    {
        const quint8 side = m_phase_man.getCurrentPhaseSide();
        const bool is_midspread_phase = ( m_phase_man.getCurrentPhase() == NO_FLUX );
        const QString flux_market = ( is_midspread_phase ) ? QLatin1String() : m_phase_man.getCurrentPhaseMarkets().value( 0 );

        const QString phase_name = QString( "%1-%2-%3" )
                                  .arg( is_midspread_phase ? "noflux" : "flux" )
                                  .arg( side == SIDE_BUY ? "B" : "S" )
                                  .arg( is_midspread_phase ? "all" : flux_market ); // todo: put selected market here in market loop

        // initialize duplicity spread
        Spread spread_duplicity;
        if ( !is_midspread_phase )
        {
            spread_duplicity = getSpreadForSide( flux_market, side, true, false, true, true );

            if ( !spread_duplicity.isValid() )
            {
                kDebug() << "spruceoverseer error: duplicity spread was not valid for phase" << phase_name;
                return;
            }
        }

        spruce->clearCurrentPrices();
//        kDebug() << "alpha markets" << alpha_markets;
        for ( QVector<QString>::const_iterator i = alpha_markets.begin(); i != alpha_markets.end(); i++ )
        {
            const Market market = *i;
            const QString currency = market.getQuote();

            // check spread midprice
            const Coin midprice = price_aggregator->getSpread( market ).getMidPrice();
            if ( midprice.isZeroOrLess() )
            {
                kDebug() << "spruceoverseer error:" << market << "midprice" << midprice << "was invalid for phase" << phase_name;
                return;
            }

            spread_midprice[ market ] = midprice;

            // for noflux phase, if market matches selected market, select best price from duplicity price or mid price
            Coin diffusion_price;
            if ( !is_midspread_phase && market == flux_market )
            {
                // set the most optimistic price to use, either the midprice or duplicity price
                diffusion_price = ( side == SIDE_BUY ) ? std::min( midprice, spread_duplicity.bid ) :
                                                         std::max( midprice, spread_duplicity.ask );
            }
            else
            {
                diffusion_price = midprice;

                // undercut slightly *for markets with only one price source like BTC_USDN
                if ( side == SIDE_SELL )
                    diffusion_price *= Coin( "0.999999" );
                else if ( side == SIDE_BUY )
                    diffusion_price *= Coin( "1.000001" );
            }

            spruce->setCurrentPrice( currency, diffusion_price );
        }

        // after we set the prices, build cache for base capital and other things for each phase
        spruce->buildCache();

        // copy prices to set back at the end of this function
        if ( is_midspread_phase )
            midspread_prices = spruce->getCurrentPrices();

        // on the sell side of the midspread phase, the result is the same as the buy side, so skip it
        if ( !( side == SIDE_SELL && is_midspread_phase ) )
        {
            // calculate amount to short/long, and fail if necessary
            if ( !spruce->calculateAmountToShortLong( is_midspread_phase ) )
            {
                kDebug() << "error: calculateAmountToShortLong() failed";
                return;
            }
        }

        const QMap<QString,Coin> &qty_to_shortlong_map = spruce->getQuantityToShortLongMap();
//        kDebug() << "qsl map:" << qty_to_shortlong_map;

        for ( QMap<quint8, Engine*>::const_iterator e = engine_map->begin(); e != engine_map->end(); e++ )
        {
            Engine *engine = e.value();

            const bool is_exchange_responsive = engine->isOrderBookResponsive();

            for ( QMap<QString,Coin>::const_iterator i = qty_to_shortlong_map.begin(); i != qty_to_shortlong_map.end(); i++ )
            {
                const Market market = Market( i.key() );
                const Coin &quantity = i.value();

                // skip market unless it's selected, except for midspread phase which can set every market
                if ( !is_midspread_phase && market != flux_market )
                    continue;

                const QString exchange_market_key = QString( "%1-%2" )
                                                    .arg( e.key() )
                                                    .arg( market );

                // get market allocation for this exchange and apply to qty_to_shortlong
                const Coin market_alloc_ratio = spruce->getExchangeAllocation( exchange_market_key, is_midspread_phase );

                // continue on zero market allocation for this engine
                if ( market_alloc_ratio.isZeroOrLess() )
                    continue;

                // if we have something allocated for this exchange, check if orderbooks are responsive for this engine
                if ( !is_exchange_responsive )
                {
                    kDebug() << "[SpruceOverseer] engine" << engine->getEngineTypeStr() << " has market allocations but oderbooks are stale, skipping setting orders";
                    continue;
                }

                QString order_type = "onetime-override-";
                Coin buy_price, sell_price;

                // set price for order
                if ( is_midspread_phase )
                {
                    buy_price = sell_price = spread_midprice.value( market ); // note: midspread buy == midspread sell

                    // set local taker mode to disable local spread collision detection which would modify the price
                    order_type += "-taker";

                    // set fast timeout
                    order_type += QString( "-timeout%1" ).arg( spruce->getOrderTimeoutMidspread() );
                }
                else
                {
                    buy_price = spread_duplicity.bid;
                    sell_price = spread_duplicity.ask;
                }

                // run cancellors for this phase every iteration
                const Coin cancel_thresh_price = ( is_midspread_phase ) ? Coin() :
                                                 ( side == SIDE_BUY ) ? buy_price : sell_price;
                runCancellors( engine, market, side, phase_name, cancel_thresh_price );

                const Coin qty_to_shortlong = quantity * market_alloc_ratio;
                const bool is_buy = qty_to_shortlong.isZeroOrLess();

                // disable snapback if we flipped to the other side
                if ( is_midspread_phase &&
                     ( (  is_buy && spruce->getSnapbackState( market, SIDE_SELL ) ) ||
                       ( !is_buy && spruce->getSnapbackState( market, SIDE_BUY ) ) ) )
                {
                    spruce->setSnapbackState( market, is_buy ? SIDE_SELL : SIDE_BUY, false, buy_price );
                }

                // don't place buys during the ask price loop, or sells during the bid price loop
                if ( (  is_buy && side == SIDE_SELL ) ||
                     ( !is_buy && side == SIDE_BUY ) )
                    continue;

                // give priority to noflux phase orders by continuing if flux phase and order currency is banned
                if ( !is_midspread_phase )
                {
                    const QString &order_currency = ( side == SIDE_BUY ) ? market.getBase() : market.getQuote();

                    if ( engine->isFluxCurrencyBanned( order_currency ) )
                        continue;
                }

                // cache some order settings
                const Coin order_size_default = spruce->getOrderSize();
                const Coin order_size_limit = spruce->getOrderNice( market.getQuote(), side, is_midspread_phase );

                // cache amount to short/long
                const Coin current_market_price = spruce->getCurrentPrice( market.getQuote() );
                const Coin amount_to_shortlong = current_market_price * qty_to_shortlong;
                const Coin amount_to_shortlong_abs = amount_to_shortlong.abs();
                const Coin amount_to_shortlong_effective = amount_to_shortlong_abs - order_size_limit;

                // continue on an amount that is non-actionable and will only cause problems like div0
                if ( amount_to_shortlong_abs.isZeroOrLess() || amount_to_shortlong_effective.isZeroOrLess() )
                    continue;

                const Coin qty_to_shortlong_effective = is_buy ? amount_to_shortlong_effective / current_market_price :
                                                                 std::min( amount_to_shortlong_effective / current_market_price, spruce->getCurrentQty( market.getQuote() ) ); // if we're selling, clamp effective sell amount to how much we actually have

                // measure how close amount_to_sl is to hitting the limit, also prevent div0 if the limit is 0
                const Coin pct_progress = ( order_size_limit.isZero() ) ? Coin() : amount_to_shortlong_abs / order_size_limit.abs() * 100;

                // check amount active
                const Coin spruce_active_for_side = engine->positions->getActiveSpruceEquityTotal( market, phase_name, side, Coin() );

                // cache snapback states to print which side is active
                const bool is_snapback_buys_enabled = spruce->getSnapbackState( market, SIDE_BUY );
                const bool is_snapback_sells_enabled = spruce->getSnapbackState( market, SIDE_SELL );

                QString message_out = QString( "%1 %2 %3 | q %4 | a %5/%6 (%7%) | act %8" )
                        // print the market name in the color of the position to be taken
                        .arg( QString( "%1%2%3" ).arg( amount_to_shortlong.isGreaterThanZero() ? COLOR_RED : COLOR_GREEN )
                                                 .arg( market, -MARKET_STRING_WIDTH )
                                                 .arg( COLOR_NONE ) )
                        // if flux phase, print nothing. if midstate, print spaces if snapback disabled,
                        // if snapback is enabled, print "snap" with the color of the side with snapback active
                        .arg( !is_midspread_phase ? QString( "    " ) :
                              QString( "%1%2%3" ).arg( is_snapback_buys_enabled  ? COLOR_GREEN :
                                                       is_snapback_sells_enabled ? COLOR_RED :
                                                                                   COLOR_NONE )
                                                 .arg( is_snapback_buys_enabled || is_snapback_sells_enabled ? "snap" : "    " )
                                                 .arg( COLOR_NONE ) )
                        .arg( side == SIDE_BUY ? buy_price : sell_price )
                        .arg( Coin( qty_to_shortlong_effective ).toString( 4 ), 12 ) // note: two toString() to workaround a nit display bug
                        .arg( Coin( amount_to_shortlong.toString( 4 ) ).toString( 4 ), 9 ) // print amount to s/l
                        .arg( ( is_buy ? -order_size_limit : order_size_limit ).toString( 4 ), -13 ) // print amount to s/l less the nice buffer (the actionable amount)
                        .arg( pct_progress.toString( 1 ), 7 )
                        .arg( spruce_active_for_side.toString( 4 ), 8 );

                if ( is_midspread_phase )
                    m_last_midspread_output += message_out + "\n";

                // prepend phase name for debug output
                message_out = QString( "%1 %2" )
                               .arg( phase_name, -MARKET_STRING_WIDTH - 9 )
                               .arg( message_out );

                const bool snapback_state = spruce->getSnapbackState( market, side );

                // if snapback is enabled, check to automatically disable snapback
                if ( is_midspread_phase &&
                     snapback_state &&
                     amount_to_shortlong_abs < order_size_limit ) // make sure we are under the limit if we are disabling, to avoid enable/disable loop
                {
                    kDebug() << "[SpruceOverseer] debug: trying to disable snapback for" << market << side;

                    // disable snapback
                    spruce->setSnapbackState( market, side, false, buy_price );

                    // cancel orders related to this strategy that are now stale
                    engine->getPositionMan()->cancelStrategy( phase_name );
                }

                const bool under_the_limit = amount_to_shortlong_abs < order_size_limit;

                // we're over the nice value for the midspread phase OR snapback trigger #1 already went off
                // this will modify nice values for all phases on this side on the next round of onSpruceUp() call to getOrderNice()
                if ( is_midspread_phase &&
                     !snapback_state &&
                     ( !under_the_limit || spruce->getSnapbackStateTrigger1( market, side ) ) )
                {
                    spruce->setSnapbackState( market, side, true, buy_price, amount_to_shortlong_abs ); // note: buy price == sell price
                    continue; // note: continue here, we only want to set an order in the midspread phase if snapback is completely enabled (takes 10 calls)
                }

                // if we're under the nice size limit, skip conflict checks and order setting
                if ( under_the_limit )
                    continue;

                /// for duplicity phases, detect conflicting positions for this market within the spread distance limit
                Coin spread_distance_limit;
                if ( !is_midspread_phase )
                {
                    const Coin spread_put_threshold = spruce->getOrderNiceSpreadPut( side );

                    // reduce spread if pending amount to shortlong is greater than size * order_nice_spreadput
                    Coin spread_reduce;
                    if ( spread_put_threshold.isGreaterThanZero() &&
                         amount_to_shortlong_abs > spread_put_threshold )
                    {
                        spread_reduce = ( amount_to_shortlong_abs / spread_put_threshold ) * ( CoinAmount::SATOSHI * 100000 );

                        const Spread collapsed_spread = getSpreadForSide( market, side, true, false, false, false, spread_reduce );
                        if ( collapsed_spread.isValid() )
                        {
                            buy_price = collapsed_spread.bid;
                            sell_price = collapsed_spread.ask;
                        }
                        else
                        {
                            kDebug() << "spruceoverseer error: collapsed spread was not valid for phase" << phase_name;
                            return;
                        }
                    }

                    // cache spread distance limit for this side
                    if ( is_buy )
                        m_last_spread_distance_buys.insert( market, spread_reduce );
                    else
                        m_last_spread_distance_sells.insert( market, spread_reduce );

                    // cache spread distance limit for this side, but selected larger spread_reduce value from both sides
                    const Coin spread_reduce_selected = std::max( m_last_spread_distance_buys.value( market ), m_last_spread_distance_sells.value( market ) );
                    spread_distance_limit = std::min( spruce->getOrderGreed() + spread_reduce_selected, spruce->getOrderGreedMinimum() );

                    // search positions for conflicts
                    for ( QSet<Position*>::const_iterator j = engine->positions->all().begin(); j != engine->positions->all().end(); j++ )
                    {
                        Position *const &pos = *j;

                        // look for positions on the other side of this market
                        if ( pos->side == side ||
                             pos->is_cancelling ||
                             pos->order_set_time == 0 ||
                             pos->market != market ||
                            !pos->strategy_tag.endsWith( market ) )
                            continue;

                        if ( (  is_buy && buy_price >= pos->sell_price * spread_distance_limit ) ||
                             ( !is_buy && sell_price * spread_distance_limit <= pos->buy_price ) )
                        {
                            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_CONFLICT );
                        }
                    }
                }

                // set slow timeout
                if ( !order_type.contains( "timeout" ) )
                {
                    QPair<quint16,quint16> timeout_flux = spruce->getOrderTimeoutFlux();

                    order_type += QString( "-timeout%1" )
                                  .arg( Global::getSecureRandomRange32( timeout_flux.first, timeout_flux.second ) );
                }

                // calculate order size, prevent going over amount_to_shortlong_abs but also prevent going under order_size_default
                const quint16 ORDER_CHUNKS_FLUX = spruce->getOrdersPerSideFlux();
                const quint16 ORDER_CHUNKS_MIDSPREAD = spruce->getOrdersPerSideMidspread();
                const Coin order_size = ( is_midspread_phase ) ?
                            std::max( order_size_default /* ORDER_CHUNKS_MIDSPREAD*/, ( amount_to_shortlong_abs - spruce_active_for_side - order_size_limit ) / ORDER_CHUNKS_MIDSPREAD ) :
                            std::max( order_size_default, ( amount_to_shortlong_abs - order_size_limit ) / ORDER_CHUNKS_FLUX ); // note: spruce_active_for_side is excluded here because the flux is variant

                // don't go over the abs value of our new projected position, and also regard nice value
                if ( spruce_active_for_side + order_size > amount_to_shortlong_abs - order_size_limit )
                    continue;

                if ( is_midspread_phase )
                    kDebug() << message_out;
                // append minimum spread distance to message (there is no distance for the midspread phase)
                else // ( !is_midspread_phase )
                {
                    message_out += QString( " | dst %1" )
                                    .arg( spread_distance_limit.toString( 4 ) );

                    kDebug() << message_out;
                }

                // reduce ub chance, not a ping-pong order, don't supply both prices
                if ( is_buy )
                    sell_price = Coin();
                else
                    buy_price = Coin();

                // queue the order if we aren't paper trading
#if !defined(PAPER_TRADE)
                engine->addPosition( market, is_buy ? SIDE_BUY : SIDE_SELL, buy_price, sell_price, order_size,
                                     order_type, phase_name, QVector<qint32>(), false, true );
#endif
            }
        }

        // clear cache for this phase
        spruce->clearCache();
    }

    // set spruce prices to midspread so getBaseCapital is consistent with the spread
    spruce->setCurrentPrices( midspread_prices );
}

void SpruceOverseer::adjustSpread( Spread &spread, Coin limit, quint8 side, Coin &minimum_ticksize, bool expand )
{
    // get price ticksize
    Coin ticksize = minimum_ticksize;

    adjustTicksizeToSpread( ticksize, spread, minimum_ticksize );

    static const Coin CONTRACT_RATIO = Coin( SPREAD_CONTRACT_RATIO );
    static Coin diff_threshold;
    static Coin moved;

    // if contracting, calculate the contraction limit
    if ( !expand )
    {
        moved = Coin();
        ticksize = -ticksize;

        const Coin diff = std::max( spread.bid, spread.ask ) - std::min( spread.bid, spread.ask );
        diff_threshold = diff * CONTRACT_RATIO;
    }

    quint32 j = ( side == SIDE_BUY ) ? 0 : 1;

    while ( expand ? spread.bid > spread.ask * limit :
                     spread.bid < spread.ask * limit )
    {
        j++;
        if ( j % 2 == 1 ) // expand 50/50
            spread.bid -= ticksize;
        else
            spread.ask += ticksize;

        // only collapse the spread by up to half the distance
        if ( !expand )
        {
            moved += ticksize;
            if ( moved >= diff_threshold )
                break;
        }
    }
}

void SpruceOverseer::adjustTicksizeToSpread( Coin &ticksize, Spread &spread, const Coin &ticksize_minimum )
{
    const Coin diff = std::max( spread.bid, spread.ask ) - std::min( spread.bid, spread.ask );

    // clamp adjustSpread() to 1000 iterations
    if ( diff > ticksize * 1000 )
        ticksize = std::max( diff / 1000, ticksize_minimum );
}

Spread SpruceOverseer::getSpreadLimit( const QString &market, bool order_duplicity )
{
    // get trailing limit for this side
    const Coin trailing_limit_buy = spruce->getOrderTrailingLimit( SIDE_BUY );
    const Coin trailing_limit_sell = spruce->getOrderTrailingLimit( SIDE_SELL );

    // get price ticksize
    const Coin ticksize_minimum = getPriceTicksizeForMarket( market );
    Coin ticksize = ticksize_minimum;

    // read combined spread from all exchanges. include limts for side, but don't randomize
    Spread ticker_buy = getSpreadForSide( market, SIDE_BUY, order_duplicity, false, true );
    Spread ticker_sell = getSpreadForSide( market, SIDE_SELL, order_duplicity, false, true );

    // first, vibrate one way
    Spread spread1 = Spread( ticker_buy.bid, ticker_buy.ask );

    // adjust ticksize
    adjustTicksizeToSpread( ticksize, ticker_buy, ticksize_minimum );

    // expand spread
    adjustSpread( spread1, trailing_limit_buy, SIDE_BUY, ticksize );

    // vibrate the other way
    Spread spread2 = Spread( ticker_sell.bid, ticker_sell.ask );

    // adjust ticksize
    adjustTicksizeToSpread( ticksize, ticker_buy, ticksize_minimum );

    // expand spread
    adjustSpread( spread2, trailing_limit_sell, SIDE_SELL, ticksize );

    // combine vibrations
    Spread combined_spread = Spread( spread1.bid, spread2.ask );

    if ( !order_duplicity )
    {
        const Coin midprice = combined_spread.getMidPrice();
        combined_spread.bid = midprice;
        combined_spread.ask = midprice;
    }

    //kDebug() << "spread for" << market << ":" << combined_spread;

    return combined_spread;
}

Spread SpruceOverseer::getSpreadForSide( const QString &market, quint8 side, bool order_duplicity, bool taker_mode, bool include_greed_random, bool is_randomized, Coin greed_reduce )
{
    if ( side == SIDE_SELL )
        greed_reduce = -greed_reduce;

    /// step 1: get aggregated spread
    Coin mp = price_aggregator->getSpread( market ).getMidPrice();
    Spread ret( mp, mp );

    /// step 2: apply base greed value to spread
    // get price ticksize
    const Coin ticksize_minimum = getPriceTicksizeForMarket( market );
    Coin ticksize = ticksize_minimum;

    adjustTicksizeToSpread( ticksize, ret, ticksize_minimum );

    /// step 3: adjust spread by distance chosen
    // ensure the spread is more profitable than base greed value
    const Coin spread_distance_base = spruce->getOrderGreed();
    const Coin limit = std::min( spread_distance_base + greed_reduce, spruce->getOrderGreedMinimum() );

    // contract our spread in the direction specified
    if ( greed_reduce.isGreaterThanZero() )
        adjustSpread( ret, limit, expand_spread_base_down ? SIDE_BUY : side, ticksize, false );

    // expand further in the direction specified, if needed
    adjustSpread( ret, limit, expand_spread_base_down ? SIDE_BUY : side, ticksize, true );

    // if we included randomness, expand again
    if ( include_greed_random )
    {
        Coin spread_distance_random;
        if ( is_randomized )
            spread_distance_random = spruce->getOrderGreedRandom( side );
        else
            spread_distance_random = ( side == SIDE_BUY ) ? spruce->getOrderGreed() - spruce->getOrderRandomBuy() :
                                                            spruce->getOrderGreed() - spruce->getOrderRandomSell();

        const Coin limit_random = std::min( spread_distance_random + greed_reduce, spruce->getOrderGreedMinimum() );

        // expand to limit_random
        adjustSpread( ret, limit_random, side, ticksize, true );
    }

    // if duplicity is on, leave everything normal
    if ( order_duplicity )
    {
        // if taker-mode is also on, run divide-conquer on reversed spread, since we are bidding at
        // the ask price, etc.
        if ( taker_mode )
        {
            const Coin tmp = ret.ask;
            ret.ask = ret.bid;
            ret.bid = tmp;
        }
    }
    else // if order duplicity is off, run divide-conquer on the price at the center of the spread
    {
        const Coin midprice = ret.getMidPrice();
        ret.bid = midprice;
        ret.ask = midprice;
    }

    return ret;
}

Coin SpruceOverseer::getPriceTicksizeForMarket( const Market &market ) const
{
    for ( QMap<quint8, Engine*>::const_iterator i = engine_map->begin(); i != engine_map->end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->market_info.contains( market ) )
            continue;

        const MarketInfo &info = engine->getMarketInfo( market );

        return std::max( info.matcher_ticksize,
                         Coin( info.spread.bid / 20000 ).truncatedByTicksize( info.matcher_ticksize ) );
    }

    return CoinAmount::SATOSHI;
}

void SpruceOverseer::loadSettings()
{
    const QString path = getSettingsPath();
    QFile loadfile( path );

    if ( !loadfile.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        kDebug() << "local warning: couldn't load spruce settings file" << path;
        return;
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    // emit new lines
    QString data = loadfile.readAll();
    kDebug() << "[SpruceOverseer] loaded spruce settings," << data.size() << "bytes.";

    emit gotUserCommandChunk( data );
}

void SpruceOverseer::saveSettings()
{
    // open settings file
    const QString path = getSettingsPath();

    QFile savefile( path );

    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't open spruce settings file" << path;
        return;
    }

    QTextStream out_savefile( &savefile );

    // save spruce state
    out_savefile << spruce->getSaveState();

    // save the buffer
    out_savefile.flush();
    savefile.close();
}

void SpruceOverseer::loadStats()
{
    QString path = Global::getMarketStatsPath();
    QFile loadfile( path );

    if ( !loadfile.open( QIODevice::ReadWrite | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't load stats file" << path;
        return;
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    QString data = loadfile.readAll();
    kDebug() << "[SpruceOverseer] loaded stats," << data.size() << "bytes.";

    alpha->reset();
    alpha->readSaveState( data );
}

void SpruceOverseer::saveStats()
{
    // open stats file
    QString path = Global::getMarketStatsPath();
    QFile savefile( path );

    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't open savemarket file" << path;
        return;
    }

    QTextStream out_savefile( &savefile );
    out_savefile << alpha->getSaveState();

    // save the buffer
    out_savefile.flush();
    savefile.close();
}

void SpruceOverseer::onBackupAndSave()
{
    if ( !spruce->isActive() )
        return;

    const QString trader_path = Global::getTraderPath();
    const QString settings_path = trader_path + QDir::separator() + "spruce.settings";
    const QString stats_path = trader_path + QDir::separator() + "stats";

    // backup settings file
    if ( QFile::exists( settings_path ) )
    {
        QString new_settings_path = Global::getOldLogsPath() + QDir::separator() + "spruce.settings." + QString::number( QDateTime::currentSecsSinceEpoch() );

        if ( QFile::copy( settings_path, new_settings_path ) )
        {
            QFile::remove( settings_path ); // remove old file on success
            saveSettings();
        }
        else
        {
            kDebug() << "local error: couldn't backup settings file to" << new_settings_path;
        }
    }

    // backup stats file
    if ( QFile::exists( stats_path ) )
    {
        QString new_stats_path = Global::getOldLogsPath() + QDir::separator() + "stats." + QString::number( QDateTime::currentSecsSinceEpoch() );

        if ( QFile::copy( stats_path, new_stats_path ) )
        {
            QFile::remove( stats_path ); // remove old file on success
            saveStats();
        }
        else
        {
            kDebug() << "local error: couldn't backup spruce stats file to" << new_stats_path;
        }
    }

    saveSettings();
    saveStats();
    price_aggregator->save();

    kDebug() << "info: settings and stats have been backed up and saved";
}

void SpruceOverseer::runCancellors( Engine *engine, const QString &market, const quint8 side, const QString &phase_name, const Coin &flux_price )
{
    const bool is_midspread_phase = phase_name.contains( "noflux" );

    // sort active positions by longest active first, shortest active last
    const QVector<Position*> active_by_set_time = engine->positions->activeBySetTime();

    // look for spruce positions we should cancel on this side
    const QVector<Position*>::const_iterator begin = active_by_set_time.begin(),
                                             end = active_by_set_time.end();
    for ( QVector<Position*>::const_iterator j = begin; j != end; j++ )
    {
        Position *const &pos = *j;
        bool is_inverse = false;

        // don't skip inverse markets matching this side (the inverse side)
        if (  pos->side != side &&
             !pos->is_cancelling &&
              pos->strategy_tag == phase_name &&
              pos->market.getInverse() == market )
        {
            //kDebug() << "found inverse market for cancellor for pos" << pos->stringifyOrder();
            is_inverse = true;
        }
        // skip non-qualifying position
        else if ( pos->side != side ||
                  pos->is_cancelling ||
                  pos->market != market ||
                  pos->strategy_tag != phase_name )
        {
            continue;
        }

        // get possible spread price vibration limits for new spruce order on this side
        const Spread spread_limit = getSpreadLimit( market, true );

        // set sp1 price
        Coin buy_price_limit, sell_price_limit;
        if ( is_midspread_phase )
        {
            const Coin midspread_price = price_aggregator->getSpread( market ).getMidPrice();
            buy_price_limit = midspread_price * Coin("0.99");
            sell_price_limit = midspread_price * Coin("1.01");
        }
        else
        {
            buy_price_limit = spread_limit.bid * Coin("0.99");
            sell_price_limit = spread_limit.ask * Coin("1.01");
        }

        // cache actual side/price
        const quint8 side_actual = is_inverse ? ( ( side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY ) : side;
        const Coin price_actual = is_inverse ? ( CoinAmount::COIN / pos->price ) : pos->price;

        /// cancellor 1: look for prices that are trailing the spread too far
        if ( buy_price_limit.isGreaterThanZero() && sell_price_limit.isGreaterThanZero() && // ticker is valid
             ( ( side_actual == SIDE_BUY  && price_actual < buy_price_limit ) ||
               ( side_actual == SIDE_SELL && price_actual > sell_price_limit ) ) )
        {
            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_PRICE_BOUNDS );
            continue;
        }

        // skip orders outside flux price
        if ( flux_price.isGreaterThanZero() )
        {
            const Coin flux_price_actual = is_inverse ? ( CoinAmount::COIN / flux_price ) : flux_price;

            // for cancellor 2, only try to cancel positions within the flux bounds
            if ( ( side_actual == SIDE_BUY  && price_actual < flux_price_actual ) ||
                 ( side_actual == SIDE_SELL && price_actual > flux_price_actual ) )
                continue;
        }

        // note: skipped sp2 for now
//        const QString exchange_market_key = QString( "%1-%2" )
//                                            .arg( engine->engine_type )
//                                            .arg( market );

//        // get market allocation
//        const QString &currency = Market( market ).getQuote();
//        const Coin active_amount = engine->positions->getActiveSpruceEquityTotal( market, phase_name, side_actual, flux_price );
//        const Coin amount_to_shortlong = spruce->getExchangeAllocation( exchange_market_key, is_midspread_phase ) * spruce->getCurrentPrice( currency ) * spruce->getQuantityToShortLongByCurrency( currency );

//        // get active tolerance
//        const Coin nice_zero_bound = spruce->getOrderNiceZeroBound( currency, side_actual, is_midspread_phase );
//        const Coin zero_bound_tolerance = spruce->getOrderSize() * nice_zero_bound;

//        /// cancellor 2: look for active amount > amount_to_shortlong + order_size_limit
//        if ( ( side_actual == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
//               active_amount - zero_bound_tolerance > amount_to_shortlong.abs() ) ||
//             ( side_actual == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
//               active_amount - zero_bound_tolerance > amount_to_shortlong.abs() ) )
//        {
//            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_2 );
//            continue;
//        }
    }
}

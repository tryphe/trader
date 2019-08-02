#include "engine.h"
#include "engine_test.h"
#include "trexrest.h"
#include "bncrest.h"
#include "polorest.h"
#include "stats.h"

#include <algorithm>
#include <QtMath>
#include <QVector>
#include <QSet>
#include <QMap>
#include <QQueue>
#include <QPair>
#include <QStringList>

Engine::Engine()
    : QObject( nullptr ),
      is_running_cancelall( false ), // state
      maintenance_time( 0 ),
      maintenance_triggered( false ),
      is_testing( false ),
      rest( nullptr ),
      stats( nullptr )
{
    kDebug() << "[Engine]";
}

Engine::~Engine()
{
    // delete local positions
    while( positions_all.size() > 0 )
        deletePosition( *positions_all.begin() );

    // these are deleted in trader
    rest = nullptr;
    stats = nullptr;

    kDebug() << "[Engine] done.";
}

bool Engine::hasActivePositions() const
{
    return positions_active.size();
}

bool Engine::hasQueuedPositions() const
{
    return positions_queued.size();
}

bool Engine::isActivePosition( Position * const &pos ) const
{
    return positions_active.contains( pos );
}

bool Engine::isQueuedPosition( Position * const &pos ) const
{
    return positions_queued.contains( pos );
}

bool Engine::isPosition( Position * const &pos ) const
{
    return positions_all.contains( pos );
}

bool Engine::isPositionOrderID( const QString &order_id ) const
{
    return positions_by_number.contains( order_id );
}

Position *Engine::getPositionForOrderID( const QString &order_id ) const
{
    return positions_by_number.value( order_id, nullptr );
}

Position *Engine::addPosition( QString market, quint8 side, QString price_lo, QString price_hi,
                               QString order_size, QString type, QVector<qint32> indices,
                               bool landmark, bool quiet )
{
    // convert accidental underscore to dash, and vice versa
#if defined(EXCHANGE_BITTREX)
    market.replace( QChar('_'), QChar('-') );
#elif defined(EXCHANGE_POLONIEX)
    market.replace( QChar('-'), QChar('_') );
#endif

    // parse alternate size from order_size, format: 0.001/0.002 (the alternate size is 0.002)
    QStringList parse = order_size.split( QChar( '/' ) );
    QString alternate_size;
    if ( parse.size() > 1 )
    {
        order_size = parse.value( 0 ); // this will be formatted below
        alternate_size = CoinAmount::toSatoshiFormatStrExpr( parse.value( 1 ) ); // formatted
    }
    parse.clear(); // cleanup

    const bool is_onetime = type.startsWith( "onetime" );
    const bool is_taker = type.contains( "-taker" );
    const bool is_ghost = type == "ghost";
    const bool is_active = type == "active";
    const bool is_override = type.contains( "-override" );

    // check for incorrect order type
    if ( !is_active && !is_ghost && !is_onetime )
    {
        kDebug() << "local error: please specify 'active', 'ghost', or 'onetime' for the order type";
        return nullptr;
    }

    // check for blank argument
    if ( market.isEmpty() || price_lo.isEmpty() || price_hi.isEmpty() || order_size.isEmpty() )
    {
        kDebug() << "local error: an argument was empty. mkt:" << market << "lo:" << price_lo << "hi:"
                 << price_hi << "sz:" << order_size;
        return nullptr;
    }

    // somebody fucked up
    if ( side != SIDE_SELL && side != SIDE_BUY )
    {
        kDebug() << "local error: invalid 'side'" << side;
        return nullptr;
    }

    // don't permit landmark type (uses market indices) with one-time orders
    if ( landmark && is_onetime )
    {
        kDebug() << "local error: can't use landmark order type with one-time order";
        return nullptr;
    }

    // check that we didn't make an erroneous buy/sell price. if it's a onetime order, do single price check
    if ( ( !is_onetime && ( Coin( price_hi ) <= Coin( price_lo ) ||
                            Coin( price_lo ).isZeroOrLess() || Coin( price_hi ).isZeroOrLess() ) ) ||
         ( is_onetime && side == SIDE_BUY && Coin( price_lo ).isZeroOrLess() ) ||
         ( is_onetime && side == SIDE_SELL && Coin( price_hi ).isZeroOrLess() ) ||
         ( is_onetime && alternate_size.size() > 0 && Coin( alternate_size ).isZeroOrLess() ) )
    {
        kDebug() << "local error: tried to set bad" << ( is_onetime ? "one-time" : "ping-pong" ) << "order. hi price"
                 << price_hi << "lo price" << price_lo << "size" << order_size << "alternate size" << alternate_size;
        return nullptr;
    }

    // reformat strings
    QString formatted_price_lo = CoinAmount::toSatoshiFormatStr( price_lo );
    QString formatted_price_hi = CoinAmount::toSatoshiFormatStr( price_hi );
    QString formatted_order_size = CoinAmount::toSatoshiFormatStr( order_size );

    //kDebug() << price_hi.size() << price_hi << formatted_price_hi.size() << formatted_price_hi;

    // anti-stupid check: did we put in price/amount decimals that didn't go into the price? abort if so
    if ( price_lo.size() > formatted_price_lo.size() ||
         price_hi.size() > formatted_price_hi.size() ||
         order_size.size() > formatted_order_size.size() )
    {
        kDebug() << "local error: too many decimals in one of these values: price_hi:"
                 << price_hi << "price_lo:" << price_lo << "order_size:" << order_size << "alternate_size:" << alternate_size;
        return nullptr;
    }

    // set values to formatted value
    price_lo = formatted_price_lo;
    price_hi = formatted_price_hi;
    order_size = formatted_order_size;

    // anti-stupid check: did we put in a taker price that's <>30% of the current bid/ask?
    if ( !is_override && is_taker &&
        ( ( side == SIDE_SELL && getHiBuy( market ).ratio( 0.9 ) > Coin( price_hi ) ) ||  // bid * 0.9 > price_hi
          ( side == SIDE_SELL && getHiBuy( market ).ratio( 1.1 ) < Coin( price_hi ) ) ||  // bid * 1.1 < price_hi
          ( side == SIDE_BUY && getLoSell( market ).ratio( 1.1 ) < Coin( price_lo ) ) ||  // ask * 1.1 < price_lo
          ( side == SIDE_BUY && getLoSell( market ).ratio( 0.9 ) > Coin( price_lo ) ) ) ) // ask * 0.9 > price_lo
    {
        kDebug() << "local error: taker price_hi:" << price_hi << "price_lo:" << price_lo << "is >10% from spread, aborting order. add '-override' if intentional.";
        return nullptr;
    }

    // figure out the market index if we didn't supply one
    if ( !is_onetime && indices.isEmpty() )
    {
        const PositionData posdata = PositionData( price_lo, price_hi, order_size, alternate_size );

        // get the next position index and append to our positions
        indices.append( market_info[ market ].position_index.size() );

        // add position indices to our market info
        market_info[ market ].position_index.append( posdata );

        //kDebug() << "added index for" << market << "#" << indices.value( 0 );
    }

    // if it's a ghost just exit here (we added it to the index, but don't add it live)
    if ( !is_onetime && !is_active )
        return nullptr;

    // make position object
    Position *const &pos = new Position( market, side, price_lo, price_hi, order_size, indices, landmark, this );

    // check for correctly loaded position data
    if ( !pos || pos->market.isEmpty() || pos->price.isEmpty() || pos->btc_amount.isZeroOrLess() || pos->quantity.isZeroOrLess() )
    {
        kDebug() << "local warning: new position failed to initialize" << market << side << price_lo << price_hi << order_size << indices << landmark;
        if ( pos ) delete pos;
        return nullptr;
    }

    // if running tests, exit early
    if ( is_testing )
    {
        positions_queued.insert( pos );
        positions_all.insert( pos );
        return pos;
    }

    // enforce PERCENT_PRICE on binance
#if defined(EXCHANGE_BINANCE)
    const MarketInfo &info = market_info.value( market );

    // respect the binance limits with a 20% padding (we don't know what the 5min avg is, so we'll just compress the range)
    const Coin &buy_limit = info.highest_buy * info.price_min_mul.ratio( 1.2 );
    const Coin &sell_limit = info.lowest_sell * info.price_max_mul.ratio( 0.8 );

    // regardless of the order type, enforce lo/hi price >0 to be in bounds
    if ( ( !pos->price_lo.isZeroOrLess() && pos->price_lo < buy_limit ) ||
         ( !pos->price_hi.isZeroOrLess() && pos->price_hi > sell_limit ) )
    {
        kDebug() << "hit PERCENT_PRICE limit for" << market << buy_limit << sell_limit << "for pos" << pos->stringifyOrderWithoutOrderID();
        delete pos;
        return nullptr;
    }
#endif

    pos->is_onetime = is_onetime;
    pos->is_taker = is_taker;

    // allow one-time orders to set a timeout
    if ( is_onetime && type.contains( "-timeout" ) )
    {
        bool ok = true;
        int read_from = type.indexOf( "-timeout" ) + 8;
        int timeout = type.mid( read_from, type.size() - read_from ).toInt( &ok );

        if ( ok && timeout > 0 )
            pos->max_age_minutes = timeout;
    }

    // if it's not a taker order, enable local post-only mode
    if ( !is_taker )
    {
        ensureBounds( pos ); // check for slippage based on spread
        setMarketBoundsForPos( pos ); // calculate a new hi_buy/lo_sell spread based on this position

        // if we are setting a new position, try to obtain a better price
        if ( tryMoveSlippageOrder( pos ) )
            pos->applyOffset();
    }

    // position is now queued, update engine state
    positions_queued.insert( pos );
    positions_all.insert( pos );
    market_info[ market ].order_prices.append( pos->price );

    // send rest request
    rest->sendBuySell( pos, quiet );
    return pos;
}

void Engine::addLandmarkPositionFor( Position *const &pos )
{
    // add position with dummy elements
    addPosition( pos->market, pos->side, "0.00000001", "0.00000002", "0.00000000", "active",
                 pos->market_indices, true, true );
}

void Engine::fillNQ( const QString &order_id, qint8 fill_type , quint8 extra_data )
{
    // 1 = getorder-fill
    // 2 = history-fill
    // 3 = ticker-fill
    // 4 = cancel-fill
    // 5 = wss-fill

    static const QStringList fill_strings = QStringList()
            << "getorder-fill"
            << "history-fill"
            << "ticker-fill"
            << "cancel-fill"
            << "wss-fill";

    // check for correct value
    if ( fill_type < 1 || fill_type > 5 )
    {
        kDebug() << "local error: unexpected fill type" << fill_type << "for order" << order_id;
        return;
    }

    // prevent unsafe execution
    if ( order_id.isEmpty() || !isPositionOrderID( order_id ) )
    {
        kDebug() << "local warning: uuid not found in positions:" << order_id << "fill_type:" << fill_type << "(hint: getorder timeout is probably too low)";
        return;
    }

    Position *const &pos = getPositionForOrderID( order_id );

    // we should never get here, because we call isPositionOrderID, but check anyways
    if ( !pos )
    {
        kDebug() << "local error: badptr in fillNQ, orderid" << order_id << "fill_type" << fill_type;
        return;
    }

    // update stats
    stats->updateStats( pos );

    // increment ping-pong "alternate_size" variable to take the place of order_size after 1 fill
    for ( int i = 0; i < pos->market_indices.size(); i++ )
    {
        // assure valid non-const access
        if ( market_info.value( pos->market ).position_index.size() <= pos->market_indices.value( i ) )
            continue;

        market_info[ pos->market ].position_index[ pos->market_indices.value( i ) ].incrementFillCount();
    }

    QString fill_str = fill_strings.value( fill_type -1, "unknown-fill" );
    if ( extra_data > 0 ) fill_str += QChar('-') + QString::number( extra_data );
    kDebug() << QString( "%1 %2" )
                  .arg( fill_str, -15 )
                  .arg( pos->stringifyPositionChange() );

    // set the next position and delete this one
    flipPosition( pos );

    // on trex, remove any 'getorder's in queue related to this uuid, to prevent spam
#if defined(EXCHANGE_BITTREX)
    rest->removeRequest( TREX_COMMAND_GET_ORDER, QString( "uuid=%1" ).arg( order_id ) ); // note: uses pos*
#endif

    deletePosition( pos );
}

void Engine::processFilledOrderRange( QVector<Position*> &filled_positions, qint8 fill_type )
{
    // mark is_invalidated and build markets list before we call setBounds
    QSet<QString> markets;
    for ( QVector<Position*>::const_iterator i = filled_positions.begin(); i != filled_positions.end(); i++ )
    {
        Position *const &pos = *i;
        pos->is_invalidated = true;
        markets.insert( pos->market );
    }

    setMarketBoundsForMarkets( markets );

    for ( QVector<Position*>::const_iterator i = filled_positions.begin(); i != filled_positions.end(); i++ )
        fillNQ( (*i)->order_number, fill_type );
}

void Engine::processFilledOrderSingle( Position *const &pos, qint8 fill_type )
{
    // do single position fill
    pos->is_invalidated = true;
    setMarketBoundsForMarkets( QSet<QString>() << pos->market );
    fillNQ( pos->order_number, fill_type );
}

void Engine::processOpenOrders( QVector<QString> &order_numbers, QMultiHash<QString, OrderInfo> &orders, qint64 request_time_sent_ms )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch(); // cache time
    qint32 ct_cancelled = 0, ct_all = 0;

    static QQueue<QString> stray_orders;
    stray_orders.clear();

    for ( QMultiHash<QString, OrderInfo>::const_iterator i = orders.begin(); i != orders.end(); i++ )
    {
        const QString &market = i.key();
        const quint8 &side = i->side;
        const QString &price = i->price;
        const QString &btc_amount = i->btc_amount;
        const QString &order_number = i->order_number;

        //kDebug() << "processing order" << order_number << market << side << btc_amount << "@" << price;

        // if we ran cancelall, try to cancel this order
        if ( is_running_cancelall )
        {
            ct_all++;

            // match our market filter arg1
            if ( cancel_market_filter != "all" &&
                 cancel_market_filter != market )
                continue;

            ct_cancelled++;

            // cancel stray orders
            if ( !isPositionOrderID( order_number ) )
            {
                kDebug() << "going to cancel order" << market << side << btc_amount << "@" << price << "id:" << order_number;

                // send a one time cancel request for orders we don't own
                rest->sendCancel( order_number );
                continue;
            }

            // if it is in our index, cancel that one
            cancelOrder( getPositionForOrderID( order_number ), false, CANCELLING_FOR_USER );
        }

        // we haven't seen this order in a buy/sell reply, we should test the order id to see if it matches a queued pos
        if ( m_settings.should_clear_stray_orders && !isPositionOrderID( order_number ) )
        {
            // if this isn't a price in any of our positions, we should ignore it
            if ( !m_settings.should_clear_stray_orders_all && !market_info[ market ].order_prices.contains( price ) )
                continue;

            // we haven't seen it, add a grace time if it doesn't match an active position
            if ( !order_grace_times.contains( order_number ) )
            {
                const Coin &btc_amount_d = btc_amount;
                Position *matching_pos = nullptr;

                // try and match a queued position to our json data
                for ( QSet<Position*>::const_iterator k = positions_queued.begin(); k != positions_queued.end(); k++ )
                {
                    Position *const &pos = *k;

                    // avoid nullptr
                    if ( !pos )
                        continue;

                    // we found a set order before we received the reply for it
                    if ( pos->market == market &&
                         pos->side == side &&
                         pos->price == price &&
                         pos->btc_amount == btc_amount &&
                         btc_amount_d >= pos->btc_amount.ratio( 0.999 ) &&
                         btc_amount_d <= pos->btc_amount.ratio( 1.001 ) )
                    {
                        matching_pos = pos;
                        break;
                    }
                }

                // check if the order details match a currently queued order
                if (  matching_pos &&
                     !isPositionOrderID( order_number ) && // order must not be assigned yet
                      matching_pos->order_request_time < current_time - 10000 ) // request must be a little old (so we don't cross scan-set different indices so much)
                {
                    // order is now set
                    setOrderMeat( matching_pos, order_number );

                    kDebug() << QString( "%1 %2" )
                                .arg( "scan-set", -15 )
                                .arg( matching_pos->stringifyOrder() );
                }
                // it doesn't match a queued order, we should still update the seen time
                else
                {
                    order_grace_times.insert( order_number, current_time );
                }
            }
            // we have seen the stray order at least once before, measure the grace time
            else if ( current_time - order_grace_times.value( order_number ) > m_settings.stray_grace_time_limit )
            {
                kDebug() << "queued cancel for stray order" << market << side << btc_amount << "@" << price << "id:" << order_number;
                stray_orders.append( order_number );
            }
        }
    }

    // if we were cancelling orders, just return here
    if ( is_running_cancelall )
    {
        kDebug() << "cancelled" << ct_cancelled << "orders," << ct_all << "orders total";
        is_running_cancelall = false; // reset state to default
        return;
    }

    // cancel stray orders
    if ( stray_orders.size() > 50 )
    {
        kDebug() << "local warning: mitigating cancelling >50 stray orders";
    }
    else
    {
        while ( stray_orders.size() > 0 )
        {
            const QString &order_number = stray_orders.takeFirst();
            rest->sendCancel( order_number );
            // reset grace time incase we see this order again from the next response
            order_grace_times.insert( order_number, current_time + m_settings.stray_grace_time_limit /* don't try to cancel again for 10m */ );
        }

    }

    // mitigate blank orderbook flash
    if ( m_settings.should_mitigate_blank_orderbook_flash &&
         !order_numbers.size() && // the orderbook is blank
         positions_active.size() > 50 ) // we have some orders, don't make it too low (if it's 2 or 3, we might fill all those orders at once, and the mitigation leads to the orders never getting filled)
    {
        kDebug() << "local warning: blank orderbook flash has been mitigated!";
        return;
    }

#if defined(EXCHANGE_BITTREX)
    qint32 filled_count = 0;
#elif defined(EXCHANGE_BINANCE) || defined(EXCHANGE_POLONIEX)
    QVector<Position*> filled_orders;
#endif

    // now we can look for local positions to invalidate based on if the order exists
    for ( QSet<Position*>::const_iterator k = positions_active.begin(); k != positions_active.end(); k++ )
    {
        Position *const &pos = *k;

        // avoid nullptr
        if ( !pos )
            continue;

        // has the order been "set"? if not, we should skip it
        if ( pos->order_set_time == 0 )
            continue;

        // check that we weren't cancelling the order
        if ( pos->order_cancel_time > 0 || pos->is_cancelling )
            continue;

        // allow for a safe period to avoid orders we just set possibly not showing up yet
        if ( pos->order_set_time > current_time - m_settings.safety_delay_time )
            continue;

        // is the order in the list of orders?
        if ( order_numbers.contains( pos->order_number ) )
            continue;

        // check that the api request timestamp was at/after our request send time
        if ( pos->order_set_time >= request_time_sent_ms )
            continue;

#if defined(EXCHANGE_BITTREX)
        // rate limiter for getorder
        if ( pos->order_getorder_time > current_time - 30000 )
            continue;

        // dopn't fill-nq, send getorder to check on the order (which could trigger fill-nq)
        rest->sendRequest( TREX_COMMAND_GET_ORDER, "uuid=" + pos->order_number, pos );
        pos->order_getorder_time = current_time;

        // rate limit so we don't fill the queue up with 'getorder' commands;
        if ( filled_count++ >= 5 )
            break;
    }
#elif defined(EXCHANGE_BINANCE) || defined(EXCHANGE_POLONIEX)
        // add orders to process
        filled_orders += pos;
    }

    processFilledOrderRange( filled_orders, FILL_GETORDER );
#endif
}

void Engine::processTicker( const QMap<QString, TickerInfo> &ticker_data, qint64 request_time_sent_ms )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // store deleted positions, because we can't delete and iterate a hash<>
    QVector<Position*> filled_orders;

    for ( QMap<QString, TickerInfo>::const_iterator i = ticker_data.begin(); i != ticker_data.end(); i++ )
    {
        const QString &market = i.key();
        const TickerInfo &ticker = i.value();
        const Coin &ask = ticker.ask_price;
        const Coin &bid = ticker.bid_price;

        // check for missing information
        if ( ticker.ask_price.isZeroOrLess() || ticker.bid_price.isZeroOrLess() )
            continue;

        market_info[ market ].highest_buy = bid;
        market_info[ market ].lowest_sell = ask;
    }

    // if this is a ticker feed, just process the ticker data. the fill feed will cause false fills when the ticker comes in just as new positions were set,
    // because we have no request time to compare the position set time to.
    if ( request_time_sent_ms <= 0 )
        return;

#if defined(EXCHANGE_POLONIEX)
    // if we read the ticker from anywhere and the websocket account feed is active, prevent it from filling positions (websocket feed is instant for fill notifications anyways)
    if ( rest->wss_1000_state )
        return;
#endif

    // did we find bid == ask (we shouldn't have)
    bool found_equal_bid_ask = false;

#if defined(EXCHANGE_BITTREX)
    qint32 filled_count = 0;
#endif
    // check for any orders that could've been filled
    for ( QSet<Position*>::const_iterator j = positions_active.begin(); j != positions_active.end(); j++ )
    {
        Position *const &pos = *j;

        if ( !pos )
            continue;

        const QString &market = pos->market;
        if ( market.isEmpty() || !ticker_data.contains( market ) )
            continue;

        const TickerInfo &ticker = ticker_data[ market ];
        const Coin &ask = ticker.ask_price;
        const Coin &bid = ticker.bid_price;

        // check for equal bid/ask
        if ( ask <= bid )
        {
            found_equal_bid_ask = true;
            continue;
        }

        // check for missing information
        if ( ask.isZeroOrLess() || bid.isZeroOrLess() )
            continue;

        // check for position price collision with ticker prices
        quint8 fill_details = 0;
        if      ( pos->side == SIDE_SELL && pos->price_hi <= bid ) // sell price <= hi buy
            fill_details = 1;
        else if ( pos->side == SIDE_BUY  && pos->price_lo >= ask ) // buy price => lo sell
            fill_details = 2;
        else if ( pos->side == SIDE_SELL && pos->price_hi < ask ) // sell price < lo sell
            fill_details = 3;
        else if ( pos->side == SIDE_BUY  && pos->price_lo > bid ) // buy price > hi buy
            fill_details = 4;

        if ( fill_details > 0 )
        {
            // is the order pretty new?
            if ( pos->order_set_time > request_time_sent_ms - m_settings.ticker_safety_delay_time || // if the request time is supplied, check that we didn't send the ticker command before the position was set
                 pos->order_set_time > current_time - m_settings.ticker_safety_delay_time ) // allow for a safe period to avoid orders we just set possibly not showing up yet
            {
                // for trex, if the order is new, check on it manually with 'getorder'
#if defined(EXCHANGE_BITTREX)
                // only send getorder every 30 seconds
                if ( pos->order_getorder_time > current_time - 30000 )
                    continue;

                // rate limit so we don't fill the queue up with getorder commands;
                if ( filled_count++ < 5 )
                {
                    // send getorder
                    rest->sendRequest( TREX_COMMAND_GET_ORDER, "uuid=" + pos->order_number, pos );
                    pos->order_getorder_time = current_time;
                }
#endif
                // for other exchanges, skip the order until it's a few seconds older
                continue;
            }

            // check that we weren't cancelling the order
            if ( pos->order_cancel_time > 0 || pos->is_cancelling )
                continue;

            // add to filled orders
            filled_orders += pos;
        }
    }

    // fill positions
    processFilledOrderRange( filled_orders, FILL_TICKER );

    // show warning we if we found equal bid/ask
    if ( found_equal_bid_ask )
        kDebug() << "local error: found ask <= bid for at least one market";
}

void Engine::processCancelledOrder( Position * const &pos )
{
    // pos must be valid!

    // we succeeded at resetting(cancelling) a slippage position, now put it back to the -same side- and at its original prices
    if ( pos->is_slippage && pos->cancel_reason == CANCELLING_FOR_SLIPPAGE_RESET )
    {
        if ( pos->is_landmark )
        {
            addLandmarkPositionFor( pos );
            deletePosition( pos );
            return;
        }
        else
        {
            const PositionData &new_pos = market_info[ pos->market ].position_index.value( pos->market_indices.value( 0 ) );

            addPosition( pos->market, pos->side, new_pos.price_lo, new_pos.price_hi, new_pos.order_size, "active",
                         pos->market_indices, false, true );

            deletePosition( pos );
            return;
        }
    }

    kDebug() << QString( "%1 %2" )
                .arg( "cancelled", -15 )
                .arg( pos->stringifyOrder() );

    // depending on the type of cancel, we should take some action
    if ( pos->cancel_reason == CANCELLING_FOR_DC )
        cancelOrderMeatDCOrder( pos );
    else if ( pos->cancel_reason == CANCELLING_FOR_SHORTLONG )
        flipPosition( pos );

    // delete position
    deletePosition( pos );
}

void Engine::cancelAll( QString market )
{
    // the arg will always be supplied; set the default arg here instead of the function def
    if ( market.isEmpty() )
        market = "all";

    // safety check to avoid s-filling local positions
    if ( ( hasActivePositions() || hasQueuedPositions() )
         && market == "all" )
    {
        kDebug() << "local error: you have open positions, did you mean cancellocal?";
        return;
    }

    // clear market index
    if ( market == "all" )
    {
        for ( QHash<QString, MarketInfo>::iterator i = market_info.begin(); i != market_info.end(); i++ )
        {
            (*i).order_prices.clear();
            (*i).position_index.clear();
        }
        kDebug() << "cleared all market indexes";
    }
    else
    {
        market_info[ market ].order_prices.clear();
        market_info[ market ].position_index.clear();
        kDebug() << "cleared" << market << "market index";
    }

    is_running_cancelall = true;
    cancel_market_filter = market;

#if defined(EXCHANGE_BITTREX)
    rest->sendRequest( TREX_COMMAND_GET_ORDERS );
#elif defined(EXCHANGE_BINANCE)
    rest->sendRequest( BNC_COMMAND_GETORDERS, "", nullptr, 40 );
#elif defined(EXCHANGE_POLONIEX)
    rest->sendRequest( POLO_COMMAND_GETORDERS, POLO_COMMAND_GETORDERS_ARGS );
#endif
}

void Engine::cancelLocal( QString market )
{
    // the arg will always be supplied; set the default arg here instead of the function def
    if ( market.isEmpty() )
        market = "all";

    // copy the list so we can iterate and delete freely
    QQueue<Position*> normal_positions;
    QQueue<Position*> landmark_positions;
    QQueue<Position*> deleted_positions;

    // cancel orders if we matched the market
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // we must match or filter, or it must be null/empty
        if ( market == "all" || pos->market == market )
        {
            // delete queued positions
            if ( isQueuedPosition( pos ) )
                deleted_positions.append( pos );
            else if ( pos->is_landmark )
                landmark_positions.append( pos );
            else
                normal_positions.append( pos );
        }
    }

    // delete queued positions
    while ( deleted_positions.size() > 0 )
        deletePosition( deleted_positions.takeFirst() );

    // delete and cancel all normal positions
    while ( normal_positions.size() > 0 )
        cancelOrder( normal_positions.takeFirst() );

    // delete and cancel all landmark positions
    while ( landmark_positions.size() > 0 )
        cancelOrder( landmark_positions.takeFirst() );

    // clear market index
    if ( market == "all" )
    {
        for ( QHash<QString, MarketInfo>::iterator i = market_info.begin(); i != market_info.end(); i++ )
        {
            (*i).order_prices.clear();
            (*i).position_index.clear();
        }
    }
    else
    {
        market_info[ market ].order_prices.clear();
        market_info[ market ].position_index.clear();
    }

    if ( !is_testing )
        kDebug() << "cleared" << market << "market indices";
}

void Engine::cancelOrder( Position *const &pos, bool quiet, quint8 cancel_reason )
{
    // check for position in ptr list
    if ( !pos || !isPosition( pos ) )
    {
        kDebug() << "local error: aborting dangerous cancel not found in positions_all";
        return;
    }

    // if testing, skip ahead to processCancelledOrder logic which just calls deletePosition();
    if ( is_testing )
    {
        deletePosition( pos );
        return;
    }

    // flag if the order was cancelling already
    const bool recancelling = pos->order_cancel_time > 0 || pos->is_cancelling;

    // set cancel reason (override if neccesary to change reason)
    pos->cancel_reason = cancel_reason;

    // check if this is a queued position so we can properly cancel the order when it gets set
    if ( isQueuedPosition( pos ) )
    {
        // let the order timeout if it doesn't have an orderid, but only after it gets set
        pos->is_cancelling = true;
        pos->order_cancel_time = 1; // set cancel time >0 to trip the next timeout check
        //kDebug() << "local warning: trying to cancel order, but order is in flight";
        return;
    }

    if ( !quiet )
    {
        const QString prefix_str = QString( "%1%2" )
                    .arg( pos->is_onetime ? "cancelling" :
                          pos->is_slippage ? "resetting " :
                          recancelling ?     "recancelling   " : "cancelling" )
                    .arg( cancel_reason == CANCELLING_LOWEST             ? " lo  " :
                          cancel_reason == CANCELLING_HIGHEST            ? " hi  " :
                          cancel_reason == CANCELLING_FOR_MAX_AGE        ? " age " :
                          cancel_reason == CANCELLING_FOR_SHORTLONG      ? " s/l " :
                                                                           "" ); // CANCELLING_FOR_SLIPPAGE_RESET

        kDebug() << QString( "%1 %2" )
                    .arg( prefix_str, -15 )
                    .arg( pos->stringifyOrder() );
    }

    // send request
    rest->sendCancel( pos->order_number, pos );
}

void Engine::cancelHighest( const QString &market )
{
    // store hi and high pointer
    Position *const &hi_pos = getHighestActivePingPong( market );

    // cancel highest order
    if ( hi_pos )
        cancelOrder( hi_pos, false, CANCELLING_HIGHEST );
}

void Engine::cancelLowest( const QString &market )
{
    // store lo and lo pointer
    Position *const &lo_pos = getLowestActivePingPong( market );

    // cancel lowest order
    if ( lo_pos )
        cancelOrder( lo_pos, false, CANCELLING_LOWEST );
}

//void Engine::cancelOrderByPrice( const QString &market, QString price )
//{
//    // now we can look for the order we should delete
//    for ( QSet<Position*>::const_iterator k = positions_active.begin(); k != positions_active.end(); k++ )
//    {
//        Position *const &pos = *k;

//        if (  pos->market == market &&
//             !pos->is_cancelling &&
//            ( pos->price_lo == price ||
//              pos->price_hi == price ) )
//        {
//            cancelOrder( pos );
//            break;
//        }
//    }
//}

void Engine::cancelOrderMeatDCOrder( Position * const &pos )
{
    QVector<Position*> cancelling_positions;
    bool new_order_is_landmark = false;
    QVector<qint32> new_indices;

    // look for our position's DC list and try to obtain it into cancelling_positions
    for ( QMap<QVector<Position*>, QPair<bool, QVector<qint32>>>::const_iterator i = diverge_converge.begin(); i != diverge_converge.end(); i++ )
    {
        const QVector<Position*> &positions = i.key();
        const QPair<bool, QVector<qint32>> &pair = i.value();

        // look for our pos
        if ( !positions.contains( pos ) )
            continue;

        // remove the key,val from the map so we can modify it
        cancelling_positions = positions;
        new_order_is_landmark = pair.first;
        new_indices = pair.second;

        diverge_converge.remove( positions );
        break;
    }

    // if we didn't find any positions, exit
    if ( cancelling_positions.isEmpty() )
        return;

    // remove the pos that we cancelled
    cancelling_positions.removeOne( pos );

    // did we empty the vector of positions? if so, we should set the orders in the indices
    if ( cancelling_positions.isEmpty() )
    {
        // a single, converged landmark order
        if ( new_order_is_landmark )
        {
            // clear from diverging_converging
            for ( int i = 0; i < new_indices.size(); i++ )
                diverging_converging[ pos->market ].removeOne( new_indices.value( i ) );

            pos->market_indices = new_indices;
            addLandmarkPositionFor( pos );
        }
        else // we diverged into multiple standard orders
        {
            for ( int i = 0; i < new_indices.size(); i++ )
            {
                qint32 idx = new_indices.value( i );

                // clear from diverging_converging
                diverging_converging[ pos->market ].removeOne( idx );

                // check for valid index data - incase we are cancelling
                if ( !market_info[ pos->market ].position_index.size() )
                    continue;

                // get position data
                const PositionData &data = market_info[ pos->market ].position_index.value( idx );

                // create a list with one single index, we can't use the constructor because it's an int
                QVector<qint32> new_index_single;
                new_index_single.append( new_indices.value( i ) );

                addPosition( pos->market, pos->side, data.price_lo, data.price_hi, data.order_size, "active",
                             new_index_single, false, true );
            }
        }
    }
    // if we didn't clear the dc list, put it back into the map to trigger next time
    else
    {
        diverge_converge.insert( cancelling_positions, qMakePair( new_order_is_landmark, new_indices ) );
    }
}

void Engine::setOrderMeat( Position *const &pos, QString order_number )
{
    // pos must be valid!

    if ( order_number.isEmpty() )
    {
        kDebug() << "local error: tried to set order with blank orderid" << pos->stringifyOrder();
        return;
    }

    // on binance, prepend market to orderid for uniqueness (don't remove this or you'll get collisions)
#if defined(EXCHANGE_BINANCE)
    pos->order_number = pos->market + order_number;
#else
    pos->order_number = order_number;
#endif

    // set the order_set_time so we can keep track of a missing order
    pos->order_set_time = QDateTime::currentMSecsSinceEpoch();

    // the order is set, unflag as new order if set
    pos->is_new_hilo_order = false;

    // insert our order number into positions
    positions_queued.remove( pos );
    positions_active.insert( pos );
    positions_by_number.insert( pos->order_number, pos );

    // check if the order was queued for a cancel (manual or automatic) while it was queued
    if ( pos->is_cancelling &&
         pos->order_cancel_time < QDateTime::currentMSecsSinceEpoch() - m_settings.cancel_timeout )
    {
        cancelOrder( pos, true, pos->cancel_reason );
    }
}

void Engine::saveMarket( QString market, qint32 num_orders )
{
    // the arg will always be supplied; set the default arg here instead of the function def
    if ( market.isEmpty() )
        market = "all";

    // enforce minimum orders
    if ( num_orders < 15 )
        num_orders = 15;

    // open dump file
    QString path = Global::getTraderPath() + QDir::separator() + QString( "index-%1.txt" ).arg( market );
    QFile savefile( path );
    bool is_open = savefile.open( QIODevice::WriteOnly | QIODevice::Text );

    if ( !is_open )
    {
        kDebug() << "local error: couldn't open savemarket file" << path;
        return;
    }

    QTextStream out_savefile( &savefile );

    qint32 saved_market_count = 0;
    for ( QHash<QString, MarketInfo>::const_iterator i = market_info.begin(); i != market_info.end(); i++ )
    {
        const QString &current_market = i.key();
        const MarketInfo &_market_info = i.value();
        const QVector<PositionData> &list = _market_info.position_index;

        // apply our market filter
        if ( market != "all" && current_market != market )
            continue;

        if ( current_market.isEmpty() || list.isEmpty() )
            continue;

        // store buy and sell indices
        qint32 highest_sell_idx = 0, lowest_sell_idx = std::numeric_limits<qint32>::max();
        QVector<qint32> buys, sells;

        for ( QSet<Position*>::const_iterator j = positions_all.begin(); j != positions_all.end(); j++ )
        {
            Position *const &pos = *j;

            // skip other markets
            if ( pos->market != current_market )
                continue;

            bool is_sell = ( pos->side == SIDE_SELL );

            for ( QVector<qint32>::const_iterator k = pos->market_indices.begin(); k != pos->market_indices.end(); k++ )
            {
                if ( is_sell )
                {
                    sells.append( *k );
                    if ( *k > highest_sell_idx ) highest_sell_idx = *k;
                    if ( *k < lowest_sell_idx ) lowest_sell_idx = *k;
                }
                else
                    buys.append( *k );
            }
        }

        // bad index check
        if ( buys.isEmpty() && sells.isEmpty() )
        {
            kDebug() << "local error: couldn't buy or sell indices for market" << current_market;
            continue;
        }

        // save each index as setorder
        qint32 current_index = 0;
        for ( QVector<PositionData>::const_iterator j = list.begin(); j != list.end(); j++ )
        {
            const PositionData &pos_data = *j;

            bool is_active = ( sells.contains( current_index ) || buys.contains( current_index ) ) &&
                             current_index > lowest_sell_idx - num_orders &&
                             current_index < lowest_sell_idx + num_orders;

            bool is_sell = sells.contains( current_index ) || // is active sell
                        ( current_index > highest_sell_idx && highest_sell_idx > 0 ); // is ghost sell

            // if the order has an "alternate_size", append it to preserve the state
            QString order_size = pos_data.order_size;
            if ( pos_data.alternate_size.size() > 0 )
                order_size += QString( "/%1" ).arg( pos_data.alternate_size );

            out_savefile << QString( "setorder %1 %2 %3 %4 %5 %6\n" )
                            .arg( current_market )
                            .arg( is_sell ? "sell" : "buy" )
                            .arg( pos_data.price_lo )
                            .arg( pos_data.price_hi )
                            .arg( order_size )
                            .arg( is_active ? "active" : "ghost" );

            current_index++;
        }

        // track number of saved markets
        if ( current_index > 0 )
            saved_market_count++;

        kDebug() << "saved market" << current_market << "with" << current_index << "indices";
    }

    // if we didn't save any markets, just exit
    if ( saved_market_count == 0 )
    {
        kDebug() << "no markets saved";
        return;
    }

    // save the buffer
    out_savefile.flush();
    savefile.close();
}

void Engine::setNextLowest( const QString &market, quint8 side, bool landmark )
{
    // somebody fucked up
    if ( side != SIDE_SELL && side != SIDE_BUY )
    {
        kDebug() << "local error: invalid order 'side'";
        return;
    }

    qint32 new_index = std::numeric_limits<qint32>::max();

    // get lowest sell from all positions
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // skip if one-time order
        if ( pos->is_onetime )
            continue;

        const qint32 pos_lowest_idx = pos->getLowestMarketIndex();

        if ( pos_lowest_idx < new_index &&
             pos->market == market )
        {
            new_index = pos_lowest_idx;
        }
    }

    // subtract 1
    new_index--;

    // check for index ok
    if ( new_index < 0 || new_index > std::numeric_limits<qint32>::max() -2 )
        return;

    const qint32 dc_val = market_info[ market ].order_dc;

    // count down until we find an index without a position
    while ( getPositionByIndex( market, new_index ) ||
            isIndexDivergingConverging( market, new_index ) )
        new_index--;

    QVector<qint32> indices = QVector<qint32>() << new_index;

    // check if we ran out of indexed positions so we don't make a bogus order
    if ( indices.value( 0 ) < 0 )
        return; // we need at least 1 valid index

    // add an index until we run out of bounds or our landmark size is matched
    while ( landmark && indices.size() < dc_val )
    {
        new_index = indices.value( indices.size() -1 ) -1;

        // are we about to add an out of bounds index?
        if ( new_index < 0 )
        {
            // preserve the indices and break here
            break;
        }

        // if we can't use the new index, go to the -next lowest- index and restart the loop
        if ( getPositionByIndex( market, new_index ) ||
             isIndexDivergingConverging( market, new_index ) )
        {
            while ( indices.size() > 1 )
                indices.removeLast();

            break;
        }

        indices.append( new_index );
    }

    // enforce full landmark size or return, except on the boundary of our positions
    if ( ( landmark && indices.size() != dc_val ) &&  // tried to set a landmark order with the wrong size
         !indices.contains( 0 ) ) // contains lowest position index
        return;

    // enforce return on normal order with >1 size
    if ( !landmark && indices.size() > 1 )
        return;

    // check for out of bounds indices[0] and [n]
    if ( indices.isEmpty() ||
         indices.value( 0 ) >= market_info[ market ].position_index.size() )
        return;

    // get the index data
    const PositionData &data = market_info[ market ].position_index.value( indices.value( 0 ) );

//    kDebug() << "adding idx" << indices.value( 0 ) << "from indices" << indices;
//    kDebug() << "adding next lo pos" << market << side << data.price_lo << data.price_hi << data.order_size;

    Position *pos = addPosition( market, side, data.price_lo, data.price_hi, data.order_size, "active",
                                     indices, landmark, true );

    // check for valid ptr
    if ( !pos )
        return;

    // flag as non-profitable api call (it's far from the spread)
    pos->is_new_hilo_order = true;

    kDebug() << QString( "setting next lo %1" )
                 .arg( pos->stringifyNewPosition() );
}

void Engine::setNextHighest( const QString &market, quint8 side, bool landmark )
{
    // somebody fucked up
    if ( side != SIDE_SELL && side != SIDE_BUY )
    {
        kDebug() << "local error: invalid order 'side'";
        return;
    }

    qint32 new_index = -1;

    // look for the highest buy index
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // skip if one-time order
        if ( pos->is_onetime )
            continue;

        const qint32 pos_highest_idx = pos->getHighestMarketIndex();

        if ( pos_highest_idx > new_index &&
             pos->market == market )
            new_index = pos_highest_idx;
    }

    // add 1
    new_index++;

    // check for index ok
    if ( new_index < 1 || new_index > std::numeric_limits<qint32>::max() -1 )
        return;

    const MarketInfo &_market_info = market_info[ market ];
    const qint32 dc_val = _market_info.order_dc;

    // count up until we find an index without a position
    while ( getPositionByIndex( market, new_index ) ||
            isIndexDivergingConverging( market, new_index ) )
        new_index++;

    QVector<qint32> indices = QVector<qint32>() << new_index;

    // check if we ran out of indexed positions
    if ( indices.value( 0 ) >= _market_info.position_index.size() )
        return;

    // add an index until we run out of bounds or our landmark size is matched
    while ( landmark && indices.size() < dc_val )
    {
        new_index = indices.value( indices.size() -1 ) +1;

        // are we about to add an out of bounds index or one that already exists?
        if ( new_index >= _market_info.position_index.size() )
        {
            // preserve the indices and break here
            break;
        }

        // if we can't use the new index, find the next valid index and restart the loop
        if ( getPositionByIndex( market, new_index ) ||
             isIndexDivergingConverging( market, new_index ) )
        {
            while ( indices.size() > 1 )
                indices.removeLast();

            break;
        }

        indices.append( new_index );
    }

    // enforce full landmark size or return, except on the boundary of our positions
    if ( ( landmark && indices.size() != dc_val ) &&  // tried to set a landmark order with the wrong size
         !indices.contains( _market_info.position_index.size() -1 ) ) // contains highest position index
        return;

    // enforce return on normal order with >1 size
    if ( !landmark && indices.size() > 1 )
        return;

    // check for out of bounds indices[0] and [n]
    if ( indices.isEmpty() ||
         indices.value( 0 ) >= _market_info.position_index.size() )
        return;

    // get the index data
    const PositionData &data = _market_info.position_index.value( indices.value( 0 ) );

//    kDebug() << "adding next hi pos" << market << side << data.price_lo << data.price_hi << data.order_size;


    Position *pos = addPosition( market, side, data.price_lo, data.price_hi, data.order_size, "active",
                                     indices, landmark, true );

    // check for valid ptr
    if ( !pos )
        return;

    // flag as non-profitable api call (it's far from the spread)
    pos->is_new_hilo_order = true;

    kDebug() << QString( "setting next hi %1" )
                .arg( pos->stringifyNewPosition() );
}

void Engine::flipPosition( Position *const &pos )
{
    // pos must be valid!

    // if it's not a ping-pong order, don't pong
    if ( pos->is_onetime )
        return;

    pos->flip(); // flip our position

    if ( pos->is_landmark ) // landmark pos
    {
        addLandmarkPositionFor( pos );
    }
    else // normal pos
    {
        // we could use the same prices, but instead we reset the data incase there was slippage
        const PositionData &new_data = market_info[ pos->market ].position_index.value( pos->market_indices.value( 0 ) );

        Position *new_pos = addPosition( pos->market, pos->side, new_data.price_lo, new_data.price_hi, new_data.order_size, "active",
                     pos->market_indices, false, true );

        // if we cancelled for shortlong, remove technical profit from the impending position
        if ( pos->cancel_reason == CANCELLING_FOR_SHORTLONG &&
             new_pos != nullptr )
            new_pos->per_trade_profit = Coin();
    }
}

void Engine::flipHiBuyPrice( const QString &market )
{
    Position *pos = getHighestActiveBuyPosByPrice( market );

    // check pos
    if ( !isActivePosition( pos ) )
        return;

    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued short    %1" )
                  .arg( pos->stringifyPositionChange() );

    cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
}

void Engine::flipHiBuyIndex( const QString &market )
{
    Position *pos = getHighestActiveBuyPosByIndex( market );

    // check pos
    if ( !isActivePosition( pos ) )
        return;

    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued short    %1" )
                  .arg( pos->stringifyPositionChange() );

    cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
}

void Engine::flipLoSellPrice( const QString &market, QString tag )
{
    Position *pos = getLowestActiveSellPosByPrice( market );

    // check pos
    if ( !isActivePosition( pos ) )
        return;

    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued long     %1" )
                  .arg( pos->stringifyPositionChange() );

    cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
}

void Engine::flipLoSellIndex( const QString &market )
{
    Position *pos = getLowestActiveSellPosByIndex( market );

    // check pos
    if ( !isActivePosition( pos ) )
        return;

    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued long     %1" )
                  .arg( pos->stringifyPositionChange() );

    cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
}

Coin Engine::getLoSell( const QString &market ) const
{
    return market_info[ market ].lowest_sell;
}

Coin Engine::getHiBuy( const QString &market ) const
{
    return market_info[ market ].highest_buy;
}

Coin Engine::getHiBuyFlipPrice( const QString &market ) const
{
    Position *pos = getHighestActiveBuyPosByPrice( market );

    // check pos
    if ( !pos || !isActivePosition( pos ) )
        return 0.;

    kDebug() << "hi_buy_flip" << pos->stringifyOrder();

    return pos->price_hi;
}

Coin Engine::getLoSellFlipPrice( const QString &market ) const
{
    Position *pos = getLowestActiveSellPosByPrice( market );

    // check pos
    if ( !pos || !isActivePosition( pos ) )
        return 0.;

    kDebug() << "lo_sell_flip" << pos->stringifyOrder();

    return pos->price_lo;
}

Position *Engine::getPositionByIndex( const QString &market, const qint32 idx ) const
{
    Position *ret = nullptr;

    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // check for idx
        if ( pos->market == market && pos->market_indices.contains( idx ) )
            return pos;
    }

    return ret;
}

Coin Engine::getHighestBuyPrice( const QString &market ) const
{
    Coin highest_buy_price;

    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // check for higher buy
        if ( pos->side == SIDE_BUY &&
            !pos->is_invalidated &&
            !pos->is_cancelling &&
             pos->price_lo > highest_buy_price &&
             pos->market == market )
            highest_buy_price = pos->price_lo;
    }

    return highest_buy_price;
}

Coin Engine::getLowestSellPrice( const QString &market ) const
{
    Coin lowest_sell_price = CoinAmount::A_LOT;

    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // check for lower sell
        if ( pos->side == SIDE_SELL &&
            !pos->is_invalidated &&
            !pos->is_cancelling &&
             pos->price_hi < lowest_sell_price &&
             pos->market == market )
            lowest_sell_price = pos->price_hi;
    }

    return lowest_sell_price;
}

inline bool Engine::isIndexDivergingConverging( const QString &market, const qint32 index ) const
{
    return diverging_converging[ market ].contains( index );
}

Position *Engine::getHighestActiveBuyPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;

        if (  pos->side != SIDE_BUY ||       // buys only
              pos->is_cancelling ||          // must not be cancelling
             !pos->order_number.size() ||    // must be set
              pos->market != market ||       // check market filter
              pos->is_onetime )              // check for one-time order
            continue;

        const qint32 pos_idx = pos->getHighestMarketIndex();
        if ( pos_idx > idx_hi_buy ) // position index is greater than our incrementor
        {
            idx_hi_buy = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *Engine::getHighestActiveSellPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;

        if (  pos->side != SIDE_SELL ||      // sells only
              pos->is_cancelling ||          // must not be cancelling
             !pos->order_number.size() ||    // must be set
              pos->market != market ||       // check market filter
              pos->is_onetime )              // check for one-time order
            continue;

        const qint32 pos_idx = pos->getHighestMarketIndex();
        if ( pos_idx > idx_hi_buy ) // position index is greater than our incrementor
        {
            idx_hi_buy = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *Engine::getLowestActiveSellPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_lo_sell = std::numeric_limits<qint32>::max();

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;

        if (  pos->is_onetime ||             // check for one-time order
              pos->side != SIDE_SELL ||      // sells only
              pos->is_cancelling ||          // must not be cancelling
             !pos->order_number.size() ||    // must be set
              pos->market != market )        // check market filter
            continue;

        const qint32 pos_idx = pos->getLowestMarketIndex();
        if ( pos_idx < idx_lo_sell ) // position index is greater than our incrementor
        {
            idx_lo_sell = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *Engine::getLowestActiveBuyPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_lo_sell = std::numeric_limits<qint32>::max();

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;

        if (  pos->is_onetime ||             // check for one-time order
              pos->side != SIDE_BUY ||       // buys only
              pos->is_cancelling ||          // must not be cancelling
             !pos->order_number.size() ||    // must be set
              pos->market != market )     // check market filter
            continue;

        const qint32 pos_idx = pos->getLowestMarketIndex();
        if ( pos_idx < idx_lo_sell ) // position index is greater than our incrementor
        {
            idx_lo_sell = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *Engine::getHighestActiveBuyPosByPrice( const QString &market ) const
{
    Position *ret = nullptr;
    Coin hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;

        if (  pos->side != SIDE_BUY ||       // buys only
              pos->is_cancelling ||          // must not be cancelling
             !pos->order_number.size() ||    // must be set
              pos->market != market )        // check market filter
            continue;

        if ( pos->price_lo > hi_buy ) // position index is greater than our incrementor
        {
            hi_buy = pos->price_lo;
            ret = pos;
        }
    }

    return ret;
}

Position *Engine::getLowestActiveSellPosByPrice( const QString &market ) const
{
    Position *ret = nullptr;
    Coin lo_sell = CoinAmount::A_LOT;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;

        if (  pos->side != SIDE_SELL ||      // sells only
              pos->is_cancelling ||          // must not be cancelling
             !pos->order_number.size() ||    // must be set
              pos->market != market )        // check market filter
            continue;

        if ( pos->price_hi < lo_sell ) // position index is less than our incrementor
        {
            lo_sell = pos->price_hi;
            ret = pos;
        }
    }

    return ret;
}

Position *Engine::getLowestActivePingPong( const QString &market ) const
{
    // store lo and lo pointer
    Position *lo_pos = nullptr;
    qint32 lo_idx = std::numeric_limits<qint32>::max();

    // look for highest position for a market
    qint32 pos_lo_idx;
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // note: cancelLowest() uses this. we must exlude one-time orders otherwise automatic ping-pong
        //       maintenance interferes with one-time orders.
        if ( pos->is_onetime )
            continue;

        pos_lo_idx = pos->getLowestMarketIndex();

        if ( pos_lo_idx < lo_idx &&
            !pos->is_cancelling &&
             pos->market == market )
        {
            lo_idx = pos_lo_idx;
            lo_pos = pos;
        }
    }

    return lo_pos;
}

Position *Engine::getHighestActivePingPong( const QString &market ) const
{
    // store hi and high pointer
    Position *hi_pos = nullptr;
    qint32 hi_idx = -1;

    // look for highest sell index for a market
    qint32 pos_hi_idx;
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // note: cancelHighest() uses this. we must exlude one-time orders otherwise automatic ping-pong
        //       maintenance interferes with one-time orders.
        if ( pos->is_onetime )
            continue;

        pos_hi_idx = pos->getHighestMarketIndex();

        if ( pos_hi_idx > hi_idx &&
            !pos->is_cancelling &&
             pos->market == market )
        {
            hi_idx = pos_hi_idx;
            hi_pos = pos;
        }
    }

    return hi_pos;
}

qint32 Engine::getMarketOrderTotal( const QString &market, bool onetime_only ) const
{
    if ( market.isEmpty() )
        return 0;

    qint32 total = 0;

    // get total order count for a market
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        const Position *const &pos = *i;

        // onetime_only, only include onetime orders
        if ( onetime_only && !pos->is_onetime )
            continue;

        if ( pos->market == market )
            total++;
    }

    return total;
}

qint32 Engine::getBuyTotal( const QString &market ) const
{
    if ( market.isEmpty() )
        return 0;

    qint32 total = 0;

    // get total order count for a market
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        if ( pos->side == SIDE_BUY &&
             pos->market == market )
            total++;
    }

    return total;
}

qint32 Engine::getSellTotal( const QString &market ) const
{
    if ( market.isEmpty() )
        return 0;

    qint32 total = 0;

    // get total order count for a market
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        if ( pos->side == SIDE_SELL &&
             pos->market == market )
            total++;
    }

    return total;
}

void Engine::cleanGraceTimes()
{
    // if the grace list is empty, skip this
    if ( order_grace_times.isEmpty() )
        return;

    const qint64 &current_time = QDateTime::currentMSecsSinceEpoch();
    QStringList removed;

    for ( QHash<QString, qint64>::const_iterator i = order_grace_times.begin(); i != order_grace_times.end(); i++ )
    {
        const QString &order = i.key();
        const qint64 &seen_time = i.value();

        // clear order ids older than timeout
        if ( seen_time < current_time - ( m_settings.stray_grace_time_limit *2 ) )
            removed.append( order );
    }

    // clear removed after iterator finishes
    while ( removed.size() > 0 )
        order_grace_times.remove( removed.takeFirst() );
}

void Engine::checkBuySellCount()
{
    static QMap<QString /*market*/, qint32> buys, sells;
    buys.clear();
    sells.clear();

    // look for highest index in active and queued positions
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        const Position *const &pos = *i;
        const QString &market = pos->market;

        if ( market.isEmpty() )
            continue;
        // tally buys
        else if ( pos->side == SIDE_BUY ) // && !pos->is_cancelling
            buys[ market ]++;
        // tally sells
        else if ( pos->side == SIDE_SELL ) // && !pos->is_cancelling
            sells[ market ]++;
    }

    // run until we stop setting new orders or flow control returns
    const QList<QString> &markets = market_info.keys();
    quint16 new_orders_ct;
    do
    {
        new_orders_ct = 0;

        // check buy counts
        for ( QList<QString>::const_iterator i = markets.begin(); i != markets.end(); i++ )
        {
            const QString &market = *i;
            const MarketInfo &_market_info = market_info[ market ];
            qint32 buy_count = buys[ market ];
            qint32 sell_count = sells[ market ];
            const qint32 &order_min = _market_info.order_min;
            const qint32 &order_max = _market_info.order_max;

            // if we are cancelling, don't set more orders
            if ( _market_info.position_index.isEmpty() )
                continue;

            // allow skipping of automation by setting a market min/max to 0
            if ( order_min <= 0 || order_max <= 0 )
                continue;

            /// check buy counts
            // did we reach the max orders for this market?
            while ( buy_count > order_max )
            {
                cancelLowest( market );
                buys[ market ]--;
                buy_count--;

                // flow control
                if ( rest->yieldToFlowControl() )
                    return;
            }

            // count < min
            if ( buy_count < order_min )
            {
                setNextLowest( market );
                buys[ market ]++;
                new_orders_ct++;
            }
            // count >= min and count < max
            else if ( _market_info.order_dc > 1 &&
                 buy_count >= order_min &&
                 buy_count < order_max - _market_info.order_landmark_thresh )
            {
                setNextLowest( market, SIDE_BUY, true );
                buys[ market ]++;
                new_orders_ct++;
            }

            // flow control
            if ( rest->yieldToFlowControl() )
                return;
            ///

            /// check sell counts
            // did we reach the max orders for this market?
            while ( sell_count > order_max )
            {
                cancelHighest( market );
                sells[ market ]--;
                sell_count--;

                // flow control
                if ( rest->yieldToFlowControl() )
                    return;
            }

            // count < min
            if ( sell_count < order_min )
            {
                setNextHighest( market );
                sells[ market ]++;
                new_orders_ct++;
            }
            // count >= min and count < max
            else if ( _market_info.order_dc > 1 &&
                 sell_count >= order_min &&
                 sell_count < order_max - _market_info.order_landmark_thresh )
            {
                setNextHighest( market, SIDE_SELL, true );
                sells[ market ]++;
                new_orders_ct++;
            }

            // flow control
            if ( rest->yieldToFlowControl() )
                return;
            ///
        }
    }
    while( new_orders_ct > 0 );

    buys.clear();
    sells.clear();
}

void Engine::checkMaintenance()
{
    if ( maintenance_triggered || maintenance_time <= 0 || maintenance_time > QDateTime::currentMSecsSinceEpoch() )
        return;

    kDebug() << "doing maintenance routine for epoch" << maintenance_time;

    saveMarket( "all" );
    cancelLocal( "all" );
    maintenance_triggered = true;

    kDebug() << "maintenance routine finished";
}

void Engine::printInternal()
{
    kDebug() << "maintenance_time:" << maintenance_time;
    kDebug() << "maintenance_triggered:" << maintenance_triggered;

    kDebug() << "diverge_converge: " << diverge_converge;
    kDebug() << "diverging_converging: " << diverging_converging;
}

void Engine::removeFromDC( Position * const &pos )
{
    // pos must be valid!

    // remove position from reverse lookup dc list
    for ( QMap<QVector<Position*>, QPair<bool, QVector<qint32>>>::const_iterator i = diverge_converge.begin(); i != diverge_converge.end(); i++ )
    {
        const QVector<Position*> &positions = i.key();

        // remove the key,val from the map so it doesn't hang around
        if ( positions.contains( pos ) )
        {
            diverge_converge.remove( positions );
            break;
        }
    }

    // remove order indices from dc list that contains indices that are converging/diverging
    for ( int i = 0; i < pos->market_indices.size(); i++ )
        diverging_converging[ pos->market ].removeOne( pos->market_indices.value( i ) );
}

void Engine::findBetterPrice( Position *const &pos )
{
#if defined(EXCHANGE_BITTREX)
    Q_UNUSED( pos )
    kDebug() << "local warning: tried to run findBetterPrice() on bittrex but does not a have post-only mode";
    return;
#else
    static const quint8 SLIPPAGE_CALCULATED = 1;
    static const quint8 SLIPPAGE_ADDITIVE = 2;

    // bad ptr check
    if ( !pos || !isPosition( pos ) )
        return;

    bool is_buy = ( pos->side == SIDE_BUY );
    const QString &market = pos->market;
    MarketInfo &_market_info = market_info[ market ];
    Coin &hi_buy = _market_info.highest_buy;
    Coin &lo_sell = _market_info.lowest_sell;
    Coin ticksize;

#if defined(EXCHANGE_BINANCE)
    ticksize = _market_info.price_ticksize;

    if ( pos->price_reset_count > 0 )
        ticksize += ticksize * qFloor( ( qPow( pos->price_reset_count, 1.110 ) ) );
#elif defined(EXCHANGE_POLONIEX)
    const qreal slippage_mul = rest->slippage_multiplier.value( market, 0. );

    if ( is_buy ) ticksize = pos->price_lo.ratio( slippage_mul ) + CoinAmount::SATOSHI;
    else          ticksize = pos->price_hi.ratio( slippage_mul ) + CoinAmount::SATOSHI;
#endif

    // check for bad ticksize
    if ( ticksize.isZeroOrLess() )
    {
        kDebug() << "local error: findBetterPrice() price_ticksize was <= 0 for market" << market;
        ticksize = CoinAmount::SATOSHI;
    }
    else if ( ticksize > CoinAmount::A_LOT )
        ticksize = CoinAmount::A_LOT;

    //kDebug() << "slippage offset" << ticksize << pos->price_lo << pos->price_hi;

    // adjust lo_sell
    if ( m_settings.should_adjust_hibuy_losell &&
         is_buy &&
         lo_sell.isGreaterThanZero() &&
         lo_sell > pos->price_lo )
    {
        if ( m_settings.is_chatty )
            kDebug() << "(lo-sell-adjust) tried to buy" << market << pos->price_lo
                     << "with lo_sell at" << lo_sell;

        // set new boundary
        _market_info.lowest_sell = pos->price_lo;
        lo_sell = pos->price_lo;
    }
    // adjust hi_buy
    else if ( m_settings.should_adjust_hibuy_losell &&
              !is_buy &&
              hi_buy.isGreaterThanZero() &&
              hi_buy < pos->price_hi )
    {
        if ( m_settings.is_chatty )
            kDebug() << "(hi-buy--adjust) tried to sell" << market << pos->price_hi
                     << "with hi_buy at" << hi_buy;

        // set new boundary
        _market_info.highest_buy = pos->price_hi;
        hi_buy = pos->price_hi;
    }

    quint8 haggle_type = 0;
    // replace buy price
    if ( is_buy )
    {
        Coin new_buy_price;

        // does our price collide with what the public orderbook says?
        if ( pos->price_reset_count < 1 && // how many times have we been here
             lo_sell.isGreaterThanZero() &&
             m_settings.should_slippage_be_calculated )
        {
            new_buy_price = lo_sell - ticksize;
            haggle_type = SLIPPAGE_CALCULATED;
        }
        // just add to the sell price
        else
        {
            new_buy_price = pos->price_lo - ticksize;
            haggle_type = SLIPPAGE_ADDITIVE;
        }

        kDebug() << QString( "(post-only) trying %1  buy price %2 tick size %3 for %4" )
                            .arg( haggle_type == SLIPPAGE_CALCULATED ? "calculated" :
                                  haggle_type == SLIPPAGE_ADDITIVE ? "additive  " : "unknown   " )
                            .arg( new_buy_price )
                            .arg( ticksize )
                            .arg( pos->stringifyOrderWithoutOrderID() );

        // set new prices
        pos->price_lo = new_buy_price;
    }
    // replace sell price
    else
    {
        Coin new_sell_price;

        // does our price collide with what the public orderbook says?
        if ( pos->price_reset_count < 1 && // how many times have we been here
             hi_buy.isGreaterThanZero() &&
             m_settings.should_slippage_be_calculated )
        {
            new_sell_price = hi_buy + ticksize;
            haggle_type = SLIPPAGE_CALCULATED;
        }
        // just add to the sell price
        else
        {
            new_sell_price = pos->price_hi + ticksize;
            haggle_type = SLIPPAGE_ADDITIVE;
        }

        kDebug() << QString( "(post-only) trying %1 sell price %2 tick size %3 for %4" )
                            .arg( haggle_type == SLIPPAGE_CALCULATED ? "calculated" :
                                  haggle_type == SLIPPAGE_ADDITIVE ? "additive  " : "unknown   " )
                            .arg( new_sell_price )
                            .arg( ticksize )
                            .arg( pos->stringifyOrderWithoutOrderID() );

        // set new prices
        pos->price_hi = new_sell_price;;
    }

    // set slippage
    pos->is_slippage = true;
    pos->price_reset_count++;

    // remove old price from prices index for detecting stray orders
    _market_info.order_prices.removeOne( pos->price );

    // reapply offset, sentiment, price
    pos->applyOffset();

    // add new price from prices index for detecting stray orders
    _market_info.order_prices.append( pos->price );
#endif
}

bool Engine::tryMoveSlippageOrder( Position* const &pos )
{
    // pos must be valid!

    const QString &market = pos->market;
    const Coin &lo_sell = getLoSell( market );
    const Coin &hi_buy = getHiBuy( market );

#if defined(EXCHANGE_BINANCE)
    Coin price_ticksize = market_info[ market ].price_ticksize;
#else
    static Coin price_ticksize = CoinAmount::SATOSHI;
#endif

    // check for bad ticksize
    if ( price_ticksize.isZeroOrLess() )
    {
        kDebug() << "local error: tryMoveSlippageOrder() price_ticksize was <= 0 for market" << market;
        price_ticksize = CoinAmount::SATOSHI;
    }
    else if ( price_ticksize > CoinAmount::A_LOT )
        price_ticksize = CoinAmount::A_LOT;

    // replace buy price
    if ( pos->side == SIDE_BUY )
    {
        // calculate a new buy price double/str
        Coin new_buy_price;

        // try to obtain new slippage buy price
        if ( pos->is_slippage &&
             lo_sell.isGreaterThanZero() )
        {
            new_buy_price = pos->price_lo;

            while ( new_buy_price >= price_ticksize && // new_buy_price >= SATOSHI
                    new_buy_price < lo_sell - price_ticksize && //  new_buy_price < lo_sell - SATOSHI
                    new_buy_price < pos->price_lo_original ) // new_buy_price < pos->price_lo_original
                new_buy_price += price_ticksize;
        }

        // new possible price is better than current price and different
        if ( new_buy_price != pos->price &&
             new_buy_price.isGreaterThanZero() && // new_buy_price > 0
             new_buy_price <= pos->price_lo_original && // new_buy_price <= pos->price_lo_original
             new_buy_price != pos->price_lo && // new_buy_price != pos->price_lo_d
             new_buy_price < lo_sell ) // new_buy_price < lo_sell
        {
            pos->price_lo = new_buy_price;
            pos->is_slippage = true;
            return true;
        }

        if ( pos->is_slippage && m_settings.is_chatty )
            kDebug() << "couldn't find better buy price for" << pos->stringifyOrder() << "new_buy_price"
                     << new_buy_price << "original_buy_price" << pos->price_lo_original
                     << "hi_buy" << hi_buy << "lo_sell" << lo_sell;
    }
    // replace sell price
    else if ( pos->side == SIDE_SELL )
    {
        // calculate a new sell price double/str
        Coin new_sell_price;

        // try to obtain new slippage sell price
        if ( pos->is_slippage &&
             hi_buy.isGreaterThanZero() )
        {
            new_sell_price = pos->price_hi;

            while ( new_sell_price > price_ticksize * 2. && // only iterate down to 2 sat for a sell
                    new_sell_price > hi_buy + price_ticksize &&
                    new_sell_price > pos->price_hi_original )
                new_sell_price -= price_ticksize;
        }

        // new possible price is better than current price and different
        if ( new_sell_price != pos->price &&
             new_sell_price > price_ticksize &&
             new_sell_price >= pos->price_hi_original &&
             new_sell_price != pos->price_hi &&
             new_sell_price > hi_buy )
        {
            pos->price_hi = new_sell_price;
            pos->is_slippage = true;
            return true;
        }

        if ( pos->is_slippage && m_settings.is_chatty )
            kDebug() << "couldn't find better sell price for" << pos->stringifyOrder() << "new_sell_price"
                     << new_sell_price << "original_sell_price" << pos->price_hi_original
                     << "hi_buy" << hi_buy << "lo_sell" << lo_sell;
    }

    return false;
}

void Engine::ensureBounds( Position *const &pos )
{
    // pos must be valid!

    bool is_buy = ( pos->side == SIDE_BUY );
    const QString &market = pos->market;
    const Coin &lo_sell = getLoSell( market );
    const Coin &hi_buy = getHiBuy( market );
    bool set_new_price = false;

#if defined(EXCHANGE_BINANCE)
    Coin price_ticksize = market_info[ market ].price_ticksize;
#else
    static Coin price_ticksize = CoinAmount::SATOSHI;
#endif

    // check for bad ticksize
    if ( price_ticksize.isZeroOrLess() )
    {
        kDebug() << "local error: ensureBounds() price_ticksize was <= 0 for market" << market;
        price_ticksize = CoinAmount::SATOSHI;
    }
    else if ( price_ticksize > CoinAmount::A_LOT )
        price_ticksize = CoinAmount::A_LOT;

    // set new buy price
    if ( is_buy &&
         !lo_sell.isZeroOrLess() &&
         pos->price_lo >= lo_sell )
    {
        Coin new_buy_price;

        new_buy_price = lo_sell - price_ticksize;

        if ( m_settings.is_chatty )
            kDebug() << "(ensure-bounds)" << market
                     << "pos->price_lo" << pos->price_lo
                     << "is gte lo_sell" << lo_sell
                     << "new price" << new_buy_price;

        pos->price_lo = new_buy_price;
        set_new_price = true;
    }

    // set new sell price
    else if ( !is_buy &&
              !hi_buy.isZeroOrLess() &&
              pos->price_hi <= hi_buy )
    {
        Coin new_sell_price;

        new_sell_price = hi_buy + price_ticksize;

        if ( m_settings.is_chatty )
            kDebug() << "(ensure-bounds)" << market
                     << "pos->price_hi" << pos->price_hi
                     << "is lte hi_buy" << hi_buy
                     << "new price" << new_sell_price;

        // set new prices
        pos->price_hi = new_sell_price;
        set_new_price = true;
    }

    if ( set_new_price )
    {
        MarketInfo &_market_info = market_info[ market ];

        // set slippage flag
        pos->is_slippage = true;

        // remove old price from prices index for detecting stray orders
        _market_info.order_prices.removeOne( pos->price );

        // reapply offset, sentiment, price
        pos->applyOffset();

        // add new price from prices index for detecting stray orders
        _market_info.order_prices.append( pos->price );
    }
}

void Engine::setMarketBoundsForPos( Position * const &pos )
{
    // pos must be valid!

    const QString &market = pos->market;
    MarketInfo &_market_info = market_info[ market ];
    bool is_buy = ( pos->side == SIDE_BUY );

#if defined(EXCHANGE_BINANCE)
    Coin ticksize = _market_info.price_ticksize;
#else
    Coin ticksize = CoinAmount::SATOSHI;
#endif

    // check for bad ticksize
    if ( ticksize.isZeroOrLess() )
    {
        kDebug() << "local error: setMarketBoundsForPos() ticksize was <= 0 for market" << market;
        ticksize = CoinAmount::SATOSHI;
    }
    else if ( ticksize > CoinAmount::A_LOT )
        ticksize = CoinAmount::A_LOT;

    // recalculate hi_buy if needed
    if ( is_buy && pos->price_lo > getHiBuy( market ) )
    {
        _market_info.highest_buy = pos->price_lo;

        // check buy_price and lo_sell collision (can happen if the ticker isn't updated)
        if ( pos->price_lo >= _market_info.lowest_sell )
        {
            _market_info.lowest_sell = pos->price_lo + ticksize;

            if ( m_settings.is_chatty )
                kDebug() << "(aggressive-spread-adjust-0)";
        }

        if ( m_settings.is_chatty )
            kDebug() << "(aggressive-in-fill-spread)" << market
                     << "hi_buy" << getHiBuy( market )
                     << "lo_sell" << getLoSell( market );
    }
    // recalculate lo_sell if needed
    else if ( !is_buy && pos->price_hi < _market_info.lowest_sell )
    {
        _market_info.lowest_sell = pos->price_hi;

        // check sell_price and hi_buy collision (can happen if the ticker isn't updated)
        if ( pos->price_hi <= _market_info.highest_buy )
        {
            _market_info.highest_buy = pos->price_hi - ticksize;

            if ( m_settings.is_chatty )
                kDebug() << "(aggressive-spread-adjust-1)";
        }

        if ( m_settings.is_chatty )
            kDebug() << "(aggressive-in-fill-spread)" << market
                << "hi_buy" << getHiBuy( market )
                << "lo_sell" << getLoSell( market );
    }
}

void Engine::setMarketBoundsForMarkets( const QSet<QString> &filled_markets )
{
    if ( !m_settings.should_use_aggressive_spread )
        return;

    for ( QSet<QString>::const_iterator k = filled_markets.begin(); k != filled_markets.end(); k++ )
    {
        const QString &market = *k;
        const Coin &hi_buy  = getHighestBuyPrice( market );
        const Coin &lo_sell = getLowestSellPrice( market );

        if ( !hi_buy.isZeroOrLess() &&
             lo_sell < CoinAmount::A_LOT )
        {
            MarketInfo &_market_info = market_info[ market ];

            _market_info.highest_buy = hi_buy;
            _market_info.lowest_sell = lo_sell;

            if ( m_settings.is_chatty )
                kDebug() << "(aggressive-pre-fill-spread)" << market
                         << "hi_buy" << hi_buy
                         << "lo_sell" <<  lo_sell;
        }
    }
}

void Engine::onCheckTimeouts()
{
    checkBuySellCount();

    // flow control
    if ( rest->yieldToFlowControl() )
        return;

    // avoid calculating timeouts if the number of queued requests is over limit_timeout_yield
    if ( rest->nam_queue.size() > rest->limit_timeout_yield )
        return;

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // look for timed out requests
    for ( QSet<Position*>::const_iterator i = positions_queued.begin(); i != positions_queued.end(); i++ )
    {
        Position *const &pos = *i;

        // make sure the order hasn't been set and the request is stale
        if ( pos->order_set_time == 0 &&
             pos->order_request_time > 0 &&
             pos->order_request_time < current_time - m_settings.request_timeout )
        {
            kDebug() << "order timeout detected, resending" << pos->stringifyOrder();

            rest->sendBuySell( pos );
            return;
        }
    }

    // look for timed out things
    for ( QSet<Position*>::const_iterator j = positions_active.begin(); j != positions_active.end(); j++ )
    {
        Position *const &pos = *j;

        // search for cancel order we should recancel
        if ( pos->is_cancelling &&
             pos->order_set_time > 0 &&
             pos->order_cancel_time > 0 &&
             pos->order_cancel_time < current_time - m_settings.cancel_timeout )
        {
            cancelOrder( pos );
            return;
        }

        // search for slippage order we should replace
        if ( pos->is_slippage &&
            !pos->is_cancelling &&
             pos->order_set_time > 0 &&
             pos->order_set_time < current_time - market_info[ pos->market ].slippage_timeout )
        {
            // reconcile slippage price according to spread hi/lo
            if ( tryMoveSlippageOrder( pos ) )
            {
                // we found a better price, mark resetting and cancel
                cancelOrder( pos, false, CANCELLING_FOR_SLIPPAGE_RESET );
                return;
            }
            else
            {
                // don't check it until new timeout occurs
                pos->order_set_time = current_time - m_settings.safety_delay_time;
            }
        }

        // search for one-time order with age > max_age_minutes
        if ( pos->is_onetime &&
             pos->order_set_time > 0 &&
             pos->max_age_minutes > 0 &&
             current_time > pos->order_set_time + ( 60000 * pos->max_age_minutes ) )
        {
            // the order has reached max age
            cancelOrder( pos, false, CANCELLING_FOR_MAX_AGE );
            return;
        }
    }
}

void Engine::onCheckDivergeConverge()
{
    checkMaintenance();
    cleanGraceTimes(); // this should happen every once in a while, might as well put it here

    // flow control
    if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
        return;

    // calculate hi_buy position for each market (if there isn't a low buy now, it will be set by checkBuySellCount)
    QMap<QString/*market*/,qint32> market_hi_buy_idx;
    // track lowest/highest non-landmark positions (so we can remove landmark/non-landmark/landmark clutter)
    QMap<QString/*market*/,qint32> market_single_lo_buy, market_single_hi_sell;
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;
        const QString &market = pos->market;

        // skip if one-time order
        if ( pos->is_onetime )
            continue;

        if ( pos->side == SIDE_BUY )
        {
            const qint32 highest_idx = pos->getHighestMarketIndex();
            const qint32 lowest_idx = pos->getLowestMarketIndex();

            // fill market_hi_buy_idx
            if ( highest_idx > market_hi_buy_idx.value( market, -1 ) )
                market_hi_buy_idx[ market ] = highest_idx;

            // fill market_single_lo_buy
            if ( lowest_idx < market_single_lo_buy.value( market, std::numeric_limits<qint32>::max() ) )
                market_single_lo_buy[ market ] = lowest_idx;
        }
        else if ( pos->side == SIDE_SELL )
        {
            const qint32 highest_idx = pos->getHighestMarketIndex();

            // fill market_single_hi_sell
            if ( highest_idx > market_single_hi_sell.value( market, -1 ) )
                market_single_hi_sell[ market ] = highest_idx;
        }
    }

    QMap<QString/*market*/,QVector<qint32>> converge_buys, converge_sells, diverge_buys, diverge_sells;

    // look for orders we should converge/diverge in order from lo->hi
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;
        const QString &market = pos->market;
        const MarketInfo &_market_info = market_info[ pos->market ];

        // skip if one-time order
        if ( pos->is_onetime )
            continue;

        // check for market dc size
        if ( _market_info.order_dc < 2 )
            continue;

        const qint32 first_idx = pos->getLowestMarketIndex();

        // check buy orders
        if (  pos->side == SIDE_BUY &&       // buys only
             !pos->is_cancelling &&          // must not be cancelling
           // ( pos->is_slippage && !should_dc_slippage_orders ) || // must not be slippage
              pos->order_number.size() &&    // must be set
             !isIndexDivergingConverging( market, first_idx ) &&
             !converge_buys[ market ].contains( first_idx ) &&
             !diverge_buys[ market ].contains( first_idx ) )
        {
            const qint32 buy_landmark_boundary = market_hi_buy_idx[ market ] - _market_info.order_landmark_start;
            const qint32 hi_idx = pos->getHighestMarketIndex();

            // normal buy that we should converge
            if     ( !pos->is_landmark &&
                     hi_idx < buy_landmark_boundary - _market_info.order_dc_nice )
            {
                converge_buys[ market ].append( first_idx );
            }
            // landmark buy that we should diverge
            else if ( pos->is_landmark &&
                      hi_idx > buy_landmark_boundary )
            {
                diverge_buys[ market ].append( first_idx );
            }
            // landmark buy that we should diverge (anti-clutter)
//            else if ( pos->is_landmark &&
//                      market_single_lo_buy[ market ] < first_idx )
//            {
//                kDebug() << "(anti-clutter)";
//                diverge_buys[ market ].append( first_idx );
//            }
        }

        //check sell orders
        if (  pos->side == SIDE_SELL &&      // sells only
             !pos->is_cancelling &&          // must not be cancelling
             //!pos->is_slippage || // must not be slippage
              pos->order_number.size() &&    // must be set
             !isIndexDivergingConverging( market, first_idx ) &&
             !converge_sells[ market ].contains( first_idx ) &&
             !diverge_sells[ market ].contains( first_idx ) )
        {
            const qint32 sell_landmark_boundary = market_hi_buy_idx[ market ] + 1 + _market_info.order_landmark_start;
            const qint32 lo_idx = pos->getLowestMarketIndex();

            // normal sell that we should converge
            if     ( !pos->is_landmark &&
                     lo_idx > sell_landmark_boundary + _market_info.order_dc_nice )
            {
                converge_sells[ market ].append( first_idx );
            }
            // landmark sell that we should diverge
            else if ( pos->is_landmark &&
                      lo_idx < sell_landmark_boundary ) // check idx
            {
                diverge_sells[ market ].append( first_idx );
            }
            // landmark sell that we should diverge (anti-clutter)
//            else if ( pos->is_landmark &&
//                      market_single_hi_sell[ market ] > pos->getHighestMarketIndex() )
//            {
//                kDebug() << "(anti-clutter)";
//                diverge_sells[ market ].append( first_idx );
//            }
        }
    }

    // converge buys (many)->(one)
    for ( QMap<QString/*market*/,QVector<qint32>>::const_iterator i = converge_buys.begin(); i != converge_buys.end(); i++ )
    {
        const QString &market = i.key();
        QVector<qint32> indices = i.value();

        const qint32 dc_value = market_info[ market ].order_dc;

        // check for indices size
        if ( indices.size() < dc_value || dc_value < 2 )
            continue;

        // walk the indices from hi->lo
        std::sort( indices.begin(), indices.end() );

        QVector<qint32> buy_order;

        for ( int j = 0; j < indices.size(); j++ )
        {
            const qint32 index = indices.value( j );

            // add the first item, if we don't have one
            if ( buy_order.isEmpty() )
                buy_order.append( index );
            // enforce sequential
            else if ( index == buy_order.value( buy_order.size() -1 ) +1 )
                buy_order.append( index );
            // we found non-sequential indices, remove index 0 and restart the loop from 0
            else
            {
                indices.removeFirst();
                buy_order.clear();

                // we still have indices, we should continue
                if ( indices.size() > 0 )
                {
                    j = -1; // restart loop from 0
                    continue;
                }
                // we ran out of indices
                else
                    break;
            }

            // check if we have enough orders to make a landmark
            if ( buy_order.size() == dc_value && buy_order.size() > 1 )
            {
                kDebug() << QString( "converging %1 %2" )
                             .arg( market, -8 )
                             .arg( Global::printVectorqint32( buy_order ) );

                // store positions we are cancelling
                QVector<Position*> positions;

                // cancel these indices
                for ( int k = 0; k < buy_order.size(); k++ )
                {
                    const qint32 idx = buy_order.value( k );
                    Position *const &pos = getPositionByIndex( market, idx );

                    cancelOrder( pos, true, CANCELLING_FOR_DC );
                    positions.append( pos );

                    // keep track of indices we should avoid autosetting
                    diverging_converging[ market ].append( idx );
                }

                // insert into a map for tracking for when cancels are complete
                diverge_converge.insert( positions, qMakePair( true, buy_order ) );

                buy_order.clear(); // clear buy_order
                break; // 1 order per market
            }
        }

        // flow control
        if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
            return;
    }

    // converge sells (many)->(one)
    for ( QMap<QString/*market*/,QVector<qint32>>::const_iterator i = converge_sells.begin(); i != converge_sells.end(); i++ )
    {
        const QString &market = i.key();
        QVector<qint32> indices = i.value();

        const qint32 dc_value = market_info[ market ].order_dc;

        // check for indices size
        if ( indices.size() < dc_value || dc_value < 2 )
            continue;

        // walk the indices from hi->lo
        std::sort( indices.rbegin(), indices.rend() );

        QVector<qint32> sell_order;

        for ( int j = 0; j < indices.size(); j++ )
        {
            const qint32 index = indices.value( j );

            // add the first item, if we don't have one
            if ( sell_order.isEmpty() )
                sell_order.append( index );
            // enforce sequential
            else if ( index == sell_order.value( sell_order.size() -1 ) -1 )
                sell_order.append( index );
            // we found non-sequential indices, remove index 0 and restart the loop from 0
            else
            {
                indices.removeFirst();
                sell_order.clear();

                // we still have indices, we should continue
                if ( indices.size() > 0 )
                {
                    j = -1; // restart loop from 0
                    continue;
                }
                // we ran out of indices
                else
                    break;
            }

            // check if we have enough orders to make a landmark
            if ( sell_order.size() == dc_value && sell_order.size() > 1 )
            {
                kDebug() << QString( "converging %1 %2" )
                             .arg( market, -8 )
                             .arg( Global::printVectorqint32( sell_order ) );

                // store positions we are cancelling
                QVector<Position*> positions;

                // cancel these indices
                for ( int k = 0; k < sell_order.size(); k++ )
                {
                    const qint32 idx = sell_order.value( k );
                    Position *const &pos = getPositionByIndex( market, idx );

                    cancelOrder( pos, true, CANCELLING_FOR_DC );
                    positions.append( pos );

                    // keep track of indices we should avoid autosetting
                    diverging_converging[ market ].append( idx );
                }

                // insert into a map for tracking for when cancels are complete
                diverge_converge.insert( positions, qMakePair( true, sell_order ) );

                sell_order.clear(); // clear buy_order
                break; // 1 order per market
            }
        }

        // flow control
        if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
            return;
    }

    // diverge buy (one)->(many)
    for ( QMap<QString/*market*/,QVector<qint32>>::const_iterator i = diverge_buys.begin(); i != diverge_buys.end(); i++ )
    {
        const QString &market = i.key();
        QVector<qint32> indices = i.value();

        // check for indices size
        if ( indices.isEmpty() )
            continue;

        // walk the indices from hi->lo
        std::sort( indices.begin(), indices.end() );

        const qint32 index = indices.value( 0 );
        Position *const &pos = getPositionByIndex( market, index ); // get position for index

        kDebug() << QString( "diverging  %1 %2" )
                     .arg( market, -8 )
                     .arg( Global::printVectorqint32( pos->market_indices ) );

        // cancel the order
        cancelOrder( pos, true, CANCELLING_FOR_DC );

        // store positions we are cancelling
        QVector<Position*> positions;
        positions.append( pos );

        // store a list of indices we must set after the cancel is complete
        for ( int k = 0; k < pos->market_indices.size(); k++ )
            diverging_converging[ market ].append( pos->market_indices.value( k ) );

        // insert into a map for tracking for when cancels are complete
        diverge_converge.insert( positions, qMakePair( false, pos->market_indices ) );

        // flow control
        if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
            return;
    }

    // diverge sell (one)->(many)
    for ( QMap<QString/*market*/,QVector<qint32>>::const_iterator i = diverge_sells.begin(); i != diverge_sells.end(); i++ )
    {
        const QString &market = i.key();
        QVector<qint32> indices = i.value();

        // check for indices size
        if ( indices.isEmpty() )
            continue;

        // walk the indices from lo->hi
        std::sort( indices.begin(), indices.end() );

        const qint32 index = indices.value( 0 );
        Position *const &pos = getPositionByIndex( market, index ); // get position for index

        kDebug() << QString( "diverging  %1 %2" )
                     .arg( market, -8 )
                     .arg( Global::printVectorqint32( pos->market_indices ) );

        // cancel the order
        cancelOrder( pos, true, CANCELLING_FOR_DC );

        // store positions we are cancelling
        QVector<Position*> positions;
        positions.append( pos );

        // store a list of indices we must set after the cancel is complete
        for ( int k = 0; k < pos->market_indices.size(); k++ )
            diverging_converging[ market ].append( pos->market_indices.value( k ) );

        // insert into a map for tracking for when cancels are complete
        diverge_converge.insert( positions, qMakePair( false, pos->market_indices ) );

        // flow control
        if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
            return;
    }
}

void Engine::setMarketSettings( QString market, qint32 order_min, qint32 order_max, qint32 order_dc, qint32 order_dc_nice,
                                qint32 landmark_start, qint32 landmark_thresh, bool market_sentiment, qreal market_offset )
{
    MarketInfo &_market_info = market_info[ market ];

    _market_info.order_min = order_min;
    _market_info.order_max = order_max;
    _market_info.order_dc = order_dc;
    _market_info.order_dc_nice = order_dc_nice;
    _market_info.order_landmark_start = landmark_start;
    _market_info.order_landmark_thresh = landmark_thresh;
    _market_info.market_sentiment = market_sentiment;
    _market_info.market_offset = market_offset;
}

void Engine::deletePosition( Position *const &pos )
{
    // prevent invalid access if pos is bad
    if ( !positions_active.contains( pos ) && !positions_queued.contains( pos ) )
    {
        kDebug() << "local error: called deletePosition with invalid position at" << &*pos;
        return;
    }

    /// step 1: clear position from diverge/converge, if we were diverging/converging
    removeFromDC( pos );

    /// step 2: make sure there's no active requests for this position
    QQueue<QPair<QNetworkReply*,Request*>> deleted_queue;

    // check nam_sent_queue for requests over timeout
    for ( QHash<QNetworkReply*,Request*>::const_iterator i = rest->nam_queue_sent.begin(); i != rest->nam_queue_sent.end(); i++ )
    {
        const Request *const &req = i.value();

        if ( !req->pos )
            continue;

        // if we found -this- position, add it to deleted queue
        if ( req->pos == pos )
            deleted_queue.append( qMakePair( i.key(), i.value() ) );
    }

    // delete replies that we handled
    while ( deleted_queue.size() > 0 )
    {
        QPair<QNetworkReply*,Request*> pair = deleted_queue.takeFirst();
        deleteReply( pair.first, pair.second );
    }

    /// step 3: remove from maps/containers
    positions_active.remove( pos ); // remove from active ptr list
    positions_queued.remove( pos ); // remove from tracking queue
    positions_all.remove( pos ); // remove from all
    positions_by_number.remove( pos->order_number ); // remove order from positions
    market_info[ pos->market ].order_prices.removeOne( pos->price ); // remove from prices

    delete pos; // we're done with this on the heap
}

void Engine::deleteReply( QNetworkReply * const &reply, Request * const &request )
{
    // remove from tracking queue
    if ( reply == nullptr || request == nullptr )
    {
        kDebug() << "local error: got bad request/reply" << &request << &reply;
        return;
    }

    delete request;

    // if we took it out, it won't be in there. remove incase it's still there.
    rest->nam_queue_sent.remove( reply );

    // send interrupt signal if we need to (if we are cleaning up replies in transit)
    if ( !reply->isFinished() )
        reply->abort();

    // delete from heap
    reply->deleteLater();
}

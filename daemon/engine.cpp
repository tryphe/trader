#include "engine.h"
#include "engine_test.h"
#include "trexrest.h"
#include "bncrest.h"
#include "polorest.h"
#include "stats.h"
#include "positionman.h"
#include "enginesettings.h"

#include <algorithm>
#include <QtMath>
#include <QVector>
#include <QSet>
#include <QMap>
#include <QQueue>
#include <QPair>
#include <QStringList>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

Engine::Engine()
    : QObject( nullptr ),
      positions( new PositionMan( this ) ),
      settings( new EngineSettings() ),
      is_running_cancelall( false ), // state
      maintenance_time( 0 ),
      maintenance_triggered( false ),
      is_testing( false ),
      verbosity( 1 ),
      wss_interface( false ),
      rest( nullptr ),
      stats( nullptr )
{
    kDebug() << "[Engine]";
}

Engine::~Engine()
{
    delete positions;
    delete settings;

    // these are deleted in trader
    rest = nullptr;
    stats = nullptr;

    kDebug() << "[Engine] done.";
}

Position *Engine::addPosition( QString market, quint8 side, QString buy_price, QString sell_price,
                               QString order_size, QString type, QString strategy_tag, QVector<qint32> indices,
                               bool landmark, bool quiet )
{
    // convert accidental underscore to dash, and vice versa
#if defined(EXCHANGE_BITTREX)
    market.replace( QChar('_'), QChar('-') );
#elif defined(EXCHANGE_POLONIEX)
    market.replace( QChar('-'), QChar('_') );
#endif

    MarketInfo &info = market_info[ market ];
    // check if bid/ask price exists
    if ( info.highest_buy.isZeroOrLess() || info.lowest_sell.isZeroOrLess() )
    {
        kDebug() << "local error: ticker has not been read yet. (try again)";
        return nullptr;
    }

    // parse alternate size from order_size, format: 0.001/0.002 (the alternate size is 0.002)
    QStringList parse = order_size.split( QChar( '/' ) );
    QString alternate_size;
    if ( parse.size() > 1 )
    {
        order_size = parse.value( 0 ); // this will be formatted below
        alternate_size = Coin( parse.value( 1 ) ); // formatted
    }
    parse.clear(); // cleanup

    const bool is_onetime = type.startsWith( "onetime" );
    const bool is_taker = type.contains( "-taker" );
    const bool is_ghost = type == GHOST;
    const bool is_active = type == ACTIVE;
    const bool is_override = type.contains( "-override" );

    // check for incorrect order type
    if ( !is_active && !is_ghost && !is_onetime )
    {
        kDebug() << "local error: please specify 'active', 'ghost', or 'onetime' for the order type";
        return nullptr;
    }

    // check for blank argument
    if ( market.isEmpty() || buy_price.isEmpty() || sell_price.isEmpty() || order_size.isEmpty() )
    {
        kDebug() << "local error: an argument was empty. mkt:" << market << "lo:" << buy_price << "hi:"
                 << sell_price << "sz:" << order_size;
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
    if ( ( !is_onetime && ( Coin( sell_price ) <= Coin( buy_price ) ||
                            Coin( buy_price ).isZeroOrLess() || Coin( sell_price ).isZeroOrLess() ) ) ||
         ( is_onetime && side == SIDE_BUY && Coin( buy_price ).isZeroOrLess() ) ||
         ( is_onetime && side == SIDE_SELL && Coin( sell_price ).isZeroOrLess() ) ||
         ( is_onetime && alternate_size.size() > 0 && Coin( alternate_size ).isZeroOrLess() ) )
    {
        kDebug() << "local error: tried to set bad" << ( is_onetime ? "one-time" : "ping-pong" ) << "order. hi price"
                 << sell_price << "lo price" << buy_price << "size" << order_size << "alternate size" << alternate_size;
        return nullptr;
    }

    // reformat strings
    QString formatted_buy_price = Coin( buy_price );
    QString formatted_sell_price = Coin( sell_price );
    QString formatted_order_size = Coin( order_size );

    //kDebug() << sell_price.size() << sell_price << formatted_sell_price.size() << formatted_sell_price;

    // anti-stupid check: did we put in price/amount decimals that didn't go into the price? abort if so
    if ( buy_price.size() > formatted_buy_price.size() ||
         sell_price.size() > formatted_sell_price.size() ||
         order_size.size() > formatted_order_size.size() )
    {
        kDebug() << "local error: too many decimals in one of these values: sell_price:"
                 << sell_price << "buy_price:" << buy_price << "order_size:" << order_size << "alternate_size:" << alternate_size;
        return nullptr;
    }

    // set values to formatted value
    buy_price = formatted_buy_price;
    sell_price = formatted_sell_price;
    order_size = formatted_order_size;

    // anti-stupid check: did we put in a taker price that's <>10% of the current bid/ask?
    if ( !is_override && is_taker &&
        ( ( side == SIDE_SELL && info.highest_buy.ratio( 0.9 ) > Coin( sell_price ) ) ||  // bid * 0.9 > sell_price
          ( side == SIDE_SELL && info.highest_buy.ratio( 1.1 ) < Coin( sell_price ) ) ||  // bid * 1.1 < sell_price
          ( side == SIDE_BUY && info.lowest_sell.ratio( 1.1 ) < Coin( buy_price ) ) ||  // ask * 1.1 < buy_price
          ( side == SIDE_BUY && info.lowest_sell.ratio( 0.9 ) > Coin( buy_price ) ) ) ) // ask * 0.9 > buy_price
    {
        kDebug() << "local error: taker sell_price:" << sell_price << "buy_price:" << buy_price << "is >10% from spread, aborting order. add '-override' if intentional.";
        return nullptr;
    }

    // figure out the market index if we didn't supply one
    if ( !is_onetime && indices.isEmpty() )
    {
        const PositionData posdata = PositionData( buy_price, sell_price, order_size, alternate_size );

        // get the next position index and append to our positions
        indices.append( info.position_index.size() );

        // add position indices to our market info
        info.position_index.append( posdata );
    }

    // if it's a ghost just exit here. we added it to the index, but don't set the order.
    if ( !is_onetime && !is_active )
        return nullptr;

    // make position object
    Position *const &pos = new Position( market, side, buy_price, sell_price, order_size, strategy_tag, indices, landmark, this );

    // check for correctly loaded position data
    if ( !pos || pos->market.isEmpty() || pos->price.isZeroOrLess() || pos->btc_amount.isZeroOrLess() || pos->quantity.isZeroOrLess() )
    {
        kDebug() << "local warning: new position failed to initialize" << market << side << buy_price << sell_price << order_size << indices << landmark;
        if ( pos ) delete pos;
        return nullptr;
    }

    // enforce PERCENT_PRICE on binance
#if defined(EXCHANGE_BINANCE)
    // respect the binance limits with a 20% padding (we don't know what the 5min avg is, so we'll just compress the range)
    Coin buy_limit = ( info.highest_buy * info.price_min_mul.ratio( 1.2 ) ).truncatedByTicksize( "0.00000001" );
    Coin sell_limit = ( info.lowest_sell * info.price_max_mul.ratio( 0.8 ) ).truncatedByTicksize( "0.00000001" );

    // regardless of the order type, enforce lo/hi price >0 to be in bounds
    if ( ( pos->side == SIDE_BUY  && pos->buy_price.isGreaterThanZero() && buy_limit.isGreaterThanZero() && pos->buy_price < buy_limit ) ||
         ( pos->side == SIDE_SELL && pos->sell_price.isGreaterThanZero() && sell_limit.isGreaterThanZero() && pos->sell_price > sell_limit ) )
    {
        if ( pos->is_onetime ) // if ping-pong, don't warn
            kDebug() << "local warning: hit PERCENT_PRICE limit for" << market << buy_limit << sell_limit << "for pos" << pos->stringifyOrderWithoutOrderID();
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
        // if we are setting a new position, try to obtain a better price
        if ( tryMoveOrder( pos ) )
        {
            pos->applyOffset();
        }

        if ( pos->side == SIDE_SELL && pos->sell_price < info.lowest_sell )
        {
            info.lowest_sell = pos->sell_price;

            // avoid collision
            if ( info.lowest_sell <= info.highest_buy )
                info.highest_buy = info.lowest_sell - info.price_ticksize;
        }

        if ( pos->side == SIDE_BUY  && pos->buy_price > info.highest_buy )
        {
            info.highest_buy = pos->buy_price;

            // avoid collision
            if ( info.highest_buy >= info.lowest_sell )
                info.lowest_sell = info.highest_buy + info.price_ticksize;
        }
    }

    // position is now queued, update engine state
    positions->add( pos );
    info.order_prices.append( pos->price );

    // if running tests, exit early
    if ( is_testing )
    {
        positions->activate( pos, pos->market + QString::number( pos->getLowestMarketIndex() ) );
        return pos;
    }

    // send rest request
    rest->sendBuySell( pos, quiet );
    return pos;
}

void Engine::addLandmarkPositionFor( Position *const &pos )
{
    // add position with dummy elements
    addPosition( pos->market, pos->side, "0.00000001", "0.00000002", "0.00000000", ACTIVE, "",
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
    if ( order_id.isEmpty() || !positions->isValidOrderID( order_id ) )
    {
        kDebug() << "local warning: uuid not found in positions:" << order_id << "fill_type:" << fill_type << "(hint: getorder timeout is probably too low)";
        return;
    }

    Position *const &pos = positions->getByOrderID( order_id );

    // we should never get here, because we call isPositionOrderID, but check anyways
    if ( !pos )
    {
        kDebug() << "local error: badptr in fillNQ, orderid" << order_id << "fill_type" << fill_type;
        return;
    }

    // update stats
    stats->updateStats( pos );

    MarketInfo &info = market_info[ pos->market ];

    // increment ping-pong "alternate_size" variable to take the place of order_size after 1 fill
    for ( int i = 0; i < pos->market_indices.size(); i++ )
    {
        // assure valid non-const access
        if ( info.position_index.size() <= pos->market_indices.value( i ) )
            continue;

        // increment fill count and resize by alternate size if one exists
        info.position_index[ pos->market_indices.value( i ) ].iterateFillCount();
    }

    if ( verbosity > 0 )
    {
        QString fill_str = fill_strings.value( fill_type -1, "unknown-fill" );
        if ( extra_data > 0 ) fill_str += QChar('-') + QString::number( extra_data );

        kDebug() << QString( "%1 %2" )
                      .arg( fill_str, -15 )
                      .arg( pos->stringifyPositionChange() );
    }

    // set the next position
    flipPosition( pos );

    // on trex, remove any 'getorder's in queue related to this uuid, to prevent spam
#if defined(EXCHANGE_BITTREX)
// if testing, don't access rest because it's null
if ( !is_testing )
{
    rest->removeRequest( TREX_COMMAND_GET_ORDER, QString( "uuid=%1" ).arg( order_id ) ); // note: uses pos*
}
#endif

    // delete
    positions->remove( pos );
}

Coin Engine::getSpreadPriceForCurrency( QString currency, QString base )
{
    QString market = base + "-" + currency;

    if ( !market_info.contains( market ) )
        return Coin();

    const MarketInfo &info = market_info[ market ];

    if ( info.highest_buy.isZeroOrLess() || info.lowest_sell.isZeroOrLess() )
        return Coin();

    Coin midprice = ( info.highest_buy + info.lowest_sell ) / 2;
    midprice.truncateByTicksize( CoinAmount::SATOSHI );

    return midprice;
}

void Engine::processFilledOrders( QVector<Position*> &to_be_filled, qint8 fill_type )
{
    static QJsonArray ext_updates;

    /// step 1: build markets list
    QMap<QString,QVector<Position*>> markets;
    for ( QVector<Position*>::const_iterator i = to_be_filled.begin(); i != to_be_filled.end(); i++ )
        if ( !(*i)->is_onetime )
            markets[ (*i)->market ] += (*i);

    /// step 2: get avg for each market and process new buys <= avg and new sells > avg
    for ( QMap<QString,QVector<Position*>>::const_iterator i = markets.begin(); i != markets.end(); i++ )
    {
        const QString &market = i.key();
        const MarketInfo &info = market_info[ market ];
        const QVector<Position*> &positions = i.value();

        // find price avg
        Coin price_avg;
        for ( QVector<Position*>::const_iterator j = positions.begin(); j != positions.end(); j++ )
            price_avg += (*j)->getFlippedPrice();

        price_avg /= positions.size();
        price_avg.truncateByTicksize( info.price_ticksize ); // floor by satoshis

        // process in-bounds orders and save outliers for later
        for ( QVector<Position*>::const_iterator j = positions.begin(); j != positions.end(); j++ )
        {
            Position *const &pos = *j;

            if ( ( pos->side == SIDE_SELL && pos->getFlippedPrice() <= price_avg ) || // new buy is lt avg
                 ( pos->side == SIDE_BUY  && pos->getFlippedPrice() >  price_avg ) )  // new sell is gte avg
            {
                to_be_filled.removeOne( pos );
                if ( wss_interface ) pos->jsonifyPositionFill( ext_updates );
                fillNQ( pos->order_number, fill_type );
            }
        }
    }

    /// step 3: fill remaining outliers
    for ( QVector<Position*>::const_iterator i = to_be_filled.begin(); i != to_be_filled.end(); i++ )
    {
        Position *const &pos = *i;

        if ( wss_interface ) pos->jsonifyPositionFill( ext_updates );
        fillNQ( pos->order_number, fill_type );
    }

    /// step 4: send updates to wss if enabled
    if ( wss_interface && !ext_updates.isEmpty() )
    {
        QString msg = Global::jsonArrayToString( ext_updates );
        emit newEngineMessage( msg );
        ext_updates = QJsonArray(); // clear static variable
    }
}

void Engine::processOpenOrders( QVector<QString> &order_numbers, QMultiHash<QString, OrderInfo> &orders, qint64 request_time_sent_ms )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch(); // cache time
    qint32 ct_cancelled = 0, ct_all = 0;

    QQueue<QString> stray_orders;

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
            if ( cancel_market_filter != ALL &&
                 cancel_market_filter != market )
                continue;

            ct_cancelled++;

            // cancel stray orders
            if ( !positions->isValidOrderID( order_number ) )
            {
                kDebug() << "cancelling non-bot order" << market << side << btc_amount << "@" << price << "id:" << order_number;

                // send a one time cancel request for orders we don't own
                rest->sendCancel( order_number );
                continue;
            }

            // if it is in our index, cancel that one
            positions->cancel( positions->getByOrderID( order_number ), false, CANCELLING_FOR_USER );
        }

        // we haven't seen this order in a buy/sell reply, we should test the order id to see if it matches a queued pos
        if ( settings->should_clear_stray_orders && !positions->isValidOrderID( order_number ) )
        {
            // if this isn't a price in any of our positions, we should ignore it
            if ( !settings->should_clear_stray_orders_all && !market_info[ market ].order_prices.contains( price ) )
                continue;

            // we haven't seen it, add a grace time if it doesn't match an active position
            if ( !order_grace_times.contains( order_number ) )
            {
                const Coin &btc_amount_d = btc_amount;
                Position *matching_pos = nullptr;

                // try and match a queued position to our json data
                for ( QSet<Position*>::const_iterator k = positions->queued().begin(); k != positions->queued().end(); k++ )
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
                     !positions->isValidOrderID( order_number ) && // order must not be assigned yet
                      matching_pos->order_request_time < current_time - 10000 ) // request must be a little old (so we don't cross scan-set different indices so much)
                {
                    // order is now set
                    positions->activate( matching_pos, order_number );
                }
                // it doesn't match a queued order, we should still update the seen time
                else
                {
                    order_grace_times.insert( order_number, current_time );
                }
            }
            // we have seen the stray order at least once before, measure the grace time
            else if ( current_time - order_grace_times.value( order_number ) > settings->stray_grace_time_limit )
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
            order_grace_times.insert( order_number, current_time + settings->stray_grace_time_limit /* don't try to cancel again for 10m */ );
        }

    }

    // mitigate blank orderbook flash
    if ( settings->should_mitigate_blank_orderbook_flash &&
         !order_numbers.size() && // the orderbook is blank
         positions->active().size() > 50 ) // we have some orders, don't make it too low (if it's 2 or 3, we might fill all those orders at once, and the mitigation leads to the orders never getting filled)
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
    for ( QSet<Position*>::const_iterator k = positions->active().begin(); k != positions->active().end(); k++ )
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
        if ( pos->order_set_time > current_time - settings->safety_delay_time )
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

    processFilledOrders( filled_orders, FILL_GETORDER );
#endif
}

void Engine::processTicker( const QMap<QString, TickerInfo> &ticker_data, qint64 request_time_sent_ms )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // store deleted positions, because we can't delete and iterate a hash<>
    QVector<Position*> filled_orders;
    static QJsonArray ext_updates;

    for ( QMap<QString, TickerInfo>::const_iterator i = ticker_data.begin(); i != ticker_data.end(); i++ )
    {
        const QString &market = i.key();
        const TickerInfo &ticker = i.value();
        const Coin &ask = ticker.ask_price;
        const Coin &bid = ticker.bid_price;

        // check for missing information
        if ( ask.isZeroOrLess() || bid.isZeroOrLess() )
            continue;

        MarketInfo &info = market_info[ market ];

        // update both values
        if      ( info.highest_buy != bid &&
                  info.lowest_sell != ask )
        {
            info.highest_buy = bid;
            info.lowest_sell = ask;

            if ( wss_interface ) info.jsonifyTicker( ext_updates, market );
        }
        // update bid price
        else if ( info.highest_buy != bid )
        {
            info.highest_buy = bid;

            if ( wss_interface ) info.jsonifyBid( ext_updates, market );
        }
        // update ask price
        else if ( info.lowest_sell != ask )
        {
            info.lowest_sell = ask;

            if ( wss_interface ) info.jsonifyAsk( ext_updates, market );
        }
    }

    if ( wss_interface && !ext_updates.isEmpty() )
    {
        QString ret = Global::jsonArrayToString( ext_updates );
        emit newEngineMessage( ret );
        ext_updates = QJsonArray(); // clear static variable
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
    for ( QSet<Position*>::const_iterator j = positions->active().begin(); j != positions->active().end(); j++ )
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
        if      ( pos->side == SIDE_SELL && pos->sell_price <= bid ) // sell price <= hi buy
            fill_details = 1;
        else if ( pos->side == SIDE_BUY  && pos->buy_price >= ask ) // buy price => lo sell
            fill_details = 2;
        else if ( pos->side == SIDE_SELL && pos->sell_price < ask ) // sell price < lo sell
            fill_details = 3;
        else if ( pos->side == SIDE_BUY  && pos->buy_price > bid ) // buy price > hi buy
            fill_details = 4;

        if ( fill_details > 0 )
        {
            // is the order pretty new?
            if ( pos->order_set_time > request_time_sent_ms - settings->ticker_safety_delay_time || // if the request time is supplied, check that we didn't send the ticker command before the position was set
                 pos->order_set_time > current_time - settings->ticker_safety_delay_time ) // allow for a safe period to avoid orders we just set possibly not showing up yet
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
    processFilledOrders( filled_orders, FILL_TICKER );

    // show warning we if we found equal bid/ask
    if ( found_equal_bid_ask )
        kDebug() << "local error: found ask <= bid for at least one market";
}

void Engine::processCancelledOrder( Position * const &pos )
{
    // pos must be valid!

    if ( wss_interface )
    {
        QJsonArray cancel_data;
        pos->jsonifyPositionCancel( cancel_data );
        QString msg = Global::jsonArrayToString( cancel_data );
        emit newEngineMessage( msg );
    }

    // we succeeded at resetting(cancelling) a slippage position, now put it back to the -same side- and at its original prices
    if ( pos->is_slippage && pos->cancel_reason == CANCELLING_FOR_SLIPPAGE_RESET )
    {
        if ( pos->is_landmark )
        {
            addLandmarkPositionFor( pos );
            positions->remove( pos );
            return;
        }
        else
        {
            const PositionData &new_pos = market_info[ pos->market ].position_index.value( pos->market_indices.value( 0 ) );

            addPosition( pos->market, pos->side, new_pos.buy_price, new_pos.sell_price, new_pos.order_size, ACTIVE, "",
                         pos->market_indices, false, true );

            positions->remove( pos );
            return;
        }
    }

    if ( verbosity > 0 )
        kDebug() << QString( "%1 %2" )
                    .arg( "cancelled", -15 )
                    .arg( pos->stringifyOrder() );

    // depending on the type of cancel, we should take some action
    if ( pos->cancel_reason == CANCELLING_FOR_DC )
        cancelOrderMeatDCOrder( pos );
    else if ( pos->cancel_reason == CANCELLING_FOR_SHORTLONG )
        flipPosition( pos );

    // delete position
    positions->remove( pos );
}

//void Engine::cancelOrderByPrice( const QString &market, QString price )
//{
//    // now we can look for the order we should delete
//    for ( QSet<Position*>::const_iterator k = active().begin(); k != active().end(); k++ )
//    {
//        Position *const &pos = *k;

//        if (  pos->market == market &&
//             !pos->is_cancelling &&
//            ( pos->buy_price == price ||
//              pos->sell_price == price ) )
//        {
//            positions->cancel( pos );
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
    for ( QMap<QVector<Position*>, QPair<bool, QVector<qint32>>>::const_iterator i = positions->diverge_converge.begin(); i != positions->diverge_converge.end(); i++ )
    {
        const QVector<Position*> &position_list = i.key();
        const QPair<bool, QVector<qint32>> &pair = i.value();

        // look for our pos
        if ( !position_list.contains( pos ) )
            continue;

        // remove the key,val from the map so we can modify it
        cancelling_positions = position_list;
        new_order_is_landmark = pair.first;
        new_indices = pair.second;

        // cheat and skip ahead for testing
        if ( is_testing )
        {
             cancelling_positions = QVector<Position*>() << pos;
             break;
        }

        positions->diverge_converge.remove( position_list );
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
                positions->diverging_converging[ pos->market ].removeOne( new_indices.value( i ) );

            pos->market_indices = new_indices;
            addLandmarkPositionFor( pos );
        }
        else // we diverged into multiple standard orders
        {
            MarketInfo &info = market_info[ pos->market ];

            for ( int i = 0; i < new_indices.size(); i++ )
            {
                qint32 idx = new_indices.value( i );

                // clear from diverging_converging
                positions->diverging_converging[ pos->market ].removeOne( idx );

                // check for valid index data - incase we are cancelling
                if ( !info.position_index.size() )
                    continue;

                // get position data
                const PositionData &data = info.position_index.value( idx );

                // create a list with one single index, we can't use the constructor because it's an int
                QVector<qint32> new_index_single;
                new_index_single.append( new_indices.value( i ) );

                addPosition( pos->market, pos->side, data.buy_price, data.sell_price, data.order_size, ACTIVE, "",
                             new_index_single, false, true );
            }
        }
    }
    // if we didn't clear the dc list, put it back into the map to trigger next time
    else
    {
        positions->diverge_converge.insert( cancelling_positions, qMakePair( new_order_is_landmark, new_indices ) );
    }
}

void Engine::saveMarket( QString market, qint32 num_orders )
{
    // the arg will always be supplied; set the default arg here instead of the function def
    if ( market.isEmpty() )
        market = ALL;

    // enforce minimum orders
    if ( num_orders < 15 )
        num_orders = 15;

    // open dump file
    QString path = Global::getTraderPath() + QDir::separator() + QString( "index-%1.txt" ).arg( market );
    QFile savefile( path );

    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't open savemarket file" << path;
        return;
    }

    QTextStream out_savefile( &savefile );

    qint32 saved_market_count = 0;
    for ( QHash<QString, MarketInfo>::const_iterator i = market_info.begin(); i != market_info.end(); i++ )
    {
        const QString &current_market = i.key();
        const MarketInfo &info = i.value();
        const QVector<PositionData> &list = info.position_index;

        // apply our market filter
        if ( market != ALL && current_market != market )
            continue;

        if ( current_market.isEmpty() || list.isEmpty() )
            continue;

        // store buy and sell indices
        qint32 highest_sell_idx = 0, lowest_sell_idx = std::numeric_limits<qint32>::max();
        QVector<qint32> buys, sells;

        for ( QSet<Position*>::const_iterator j = positions->all().begin(); j != positions->all().end(); j++ )
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
                            .arg( is_sell ? SELL : BUY )
                            .arg( pos_data.buy_price )
                            .arg( pos_data.sell_price )
                            .arg( order_size )
                            .arg( is_active ? ACTIVE : GHOST );

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

void Engine::saveSettings()
{
    // open settings file
    QString path = Global::getMarketSettingsPath();
    QFile savefile( path );

    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't open savemarket file" << path;
        return;
    }

    MarketInfo default_market;
    QTextStream out_savefile( &savefile );

    quint32 items_saved = 0;
    for ( QHash<QString, MarketInfo>::const_iterator i = market_info.begin(); i != market_info.end(); i++ )
    {
        const QString &market = i.key();
        const MarketInfo &info = i.value();

        // check if settings are not default
        if ( info.order_min != default_market.order_min ||
             info.order_max != default_market.order_max ||
             info.order_dc  != default_market.order_dc ||
             info.order_dc_nice != default_market.order_dc_nice ||
             info.order_landmark_thresh != default_market.order_landmark_thresh ||
             info.order_landmark_start != default_market.order_landmark_start ||
             info.market_sentiment != default_market.market_sentiment ||
             info.market_offset != default_market.market_offset )
        {
            out_savefile << QString( "setmarketsettings %1 %2 %3 %4 %5 %6 %7 %8 %9\n" )
                            .arg( market )
                            .arg( info.order_min )
                            .arg( info.order_max )
                            .arg( info.order_dc )
                            .arg( info.order_dc_nice )
                            .arg( info.order_landmark_thresh )
                            .arg( info.order_landmark_start )
                            .arg( info.market_sentiment )
                            .arg( info.market_offset );

            items_saved++;
        }
    }

    // save spruce state
    if ( spruce.isActive() )
    {
        out_savefile << spruce.getSaveState();
        items_saved++;
    }

    // if we didn't save anything, just exit
    if ( items_saved == 0 )
        return;

    // save the buffer
    out_savefile.flush();
    savefile.close();
}

void Engine::loadSettings()
{
    QString path = Global::getMarketSettingsPath();
    QFile loadfile( path );

    if ( !loadfile.open( QIODevice::ReadWrite | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't load savemarket file" << path;
        return;
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    spruce = Spruce(); // cleanup leftover spruce

    // emit new lines
    QString data = loadfile.readAll();
    kDebug() << "[Engine] loaded settings," << data.size() << "bytes.";

    emit gotUserCommandChunk( data );
    return;
}

void Engine::flipPosition( Position *const &pos )
{
    // pos must be valid!

    // if it's not a ping-pong order, don't pong
    if ( pos->is_onetime )
        return;

    pos->flip(); // flip our position

    // we cancelled for shortlong, track stats related to this strategy tag
    if ( pos->cancel_reason == CANCELLING_FOR_SHORTLONG )
        stats->addStrategyStats( pos );

    if ( pos->is_landmark ) // landmark pos
    {
        addLandmarkPositionFor( pos );
    }
    else // normal pos
    {
        // we could use the same prices, but instead we reset the data incase there was slippage
        const PositionData &new_data = market_info[ pos->market ].position_index.value( pos->market_indices.value( 0 ) );

        addPosition( pos->market, pos->side, new_data.buy_price, new_data.sell_price, new_data.order_size, ACTIVE, "",
                     pos->market_indices, false, true );
    }
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
        if ( seen_time < current_time - ( settings->stray_grace_time_limit *2 ) )
            removed.append( order );
    }

    // clear removed after iterator finishes
    while ( removed.size() > 0 )
        order_grace_times.remove( removed.takeFirst() );
}


void Engine::checkMaintenance()
{
    if ( maintenance_triggered || maintenance_time <= 0 || maintenance_time > QDateTime::currentMSecsSinceEpoch() )
        return;

    kDebug() << "doing maintenance routine for epoch" << maintenance_time;

    saveMarket( ALL );
    positions->cancelLocal( ALL );
    maintenance_triggered = true;

    kDebug() << "maintenance routine finished";
}

void Engine::printInternal()
{
    kDebug() << "maintenance_time:" << maintenance_time;
    kDebug() << "maintenance_triggered:" << maintenance_triggered;

    kDebug() << "diverge_converge: " << positions->diverge_converge;
    kDebug() << "diverging_converging: " << positions->diverging_converging;
}

QPair<Coin, Coin> Engine::getSpruceSpread( const QString &market )
{
    // calculate a profitable spread
    const MarketInfo &info = market_info[ market ];
    const Coin &ticksize = info.price_ticksize;
    QPair<Coin, Coin> spread;
    spread.first = info.highest_buy;
    spread.second = info.lowest_sell;

    Coin &buy_price = spread.first;
    Coin &sell_price = spread.second;

    // ensure the spread is more profitable than fee*2
    int j = 0;
    while ( buy_price > sell_price * spruce.getOrderGreed() )
    {
        if ( j++ % 2 == 0 )
            buy_price -= ticksize;
        else
            sell_price += ticksize;
    }

    return spread;
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
    if ( !pos || !positions->isValid( pos ) )
        return;

    bool is_buy = ( pos->side == SIDE_BUY );
    const QString &market = pos->market;
    MarketInfo &info = market_info[ market ];
    Coin &hi_buy = info.highest_buy;
    Coin &lo_sell = info.lowest_sell;
    Coin ticksize;

#if defined(EXCHANGE_BINANCE)
    ticksize = info.price_ticksize;

    if ( pos->price_reset_count > 0 )
        ticksize += ticksize * qFloor( ( qPow( pos->price_reset_count, 1.110 ) ) );
#elif defined(EXCHANGE_POLONIEX)
    const qreal slippage_mul = rest->slippage_multiplier.value( market, 0. );

    if ( is_buy ) ticksize = pos->buy_price.ratio( slippage_mul ) + CoinAmount::SATOSHI;
    else          ticksize = pos->sell_price.ratio( slippage_mul ) + CoinAmount::SATOSHI;
#endif

    //kDebug() << "slippage offset" << ticksize << pos->buy_price << pos->sell_price;

    // adjust lo_sell
    if ( settings->should_adjust_hibuy_losell &&
         is_buy &&
         lo_sell.isGreaterThanZero() &&
         lo_sell > pos->buy_price )
    {
        if ( settings->is_chatty )
            kDebug() << "(lo-sell-adjust) tried to buy" << market << pos->buy_price
                     << "with lo_sell at" << lo_sell;

        // set new boundary
        info.lowest_sell = pos->buy_price;
        lo_sell = pos->buy_price;

        // avoid collision
        if ( info.lowest_sell <= info.highest_buy )
            info.highest_buy = info.lowest_sell - info.price_ticksize;
    }
    // adjust hi_buy
    else if ( settings->should_adjust_hibuy_losell &&
              !is_buy &&
              hi_buy.isGreaterThanZero() &&
              hi_buy < pos->sell_price )
    {
        if ( settings->is_chatty )
            kDebug() << "(hi-buy--adjust) tried to sell" << market << pos->sell_price
                     << "with hi_buy at" << hi_buy;

        // set new boundary
        info.highest_buy = pos->sell_price;
        hi_buy = pos->sell_price;

        // avoid collision
        if ( info.highest_buy >= info.lowest_sell )
            info.lowest_sell = info.highest_buy + info.price_ticksize;
    }

    quint8 haggle_type = 0;
    // replace buy price
    if ( is_buy )
    {
        Coin new_buy_price;

        // does our price collide with what the public orderbook says?
        if ( pos->price_reset_count < 1 && // how many times have we been here
             lo_sell.isGreaterThanZero() &&
             settings->should_slippage_be_calculated )
        {
            new_buy_price = lo_sell - ticksize;
            haggle_type = SLIPPAGE_CALCULATED;
        }
        // just add to the sell price
        else
        {
            new_buy_price = pos->buy_price - ticksize;
            haggle_type = SLIPPAGE_ADDITIVE;
        }

        kDebug() << QString( "(post-only) trying %1  buy price %2 tick size %3 for %4" )
                            .arg( haggle_type == SLIPPAGE_CALCULATED ? "calculated" :
                                  haggle_type == SLIPPAGE_ADDITIVE ? "additive  " : "unknown   " )
                            .arg( new_buy_price )
                            .arg( ticksize )
                            .arg( pos->stringifyOrderWithoutOrderID() );

        // set new prices
        pos->buy_price = new_buy_price;
    }
    // replace sell price
    else
    {
        Coin new_sell_price;

        // does our price collide with what the public orderbook says?
        if ( pos->price_reset_count < 1 && // how many times have we been here
             hi_buy.isGreaterThanZero() &&
             settings->should_slippage_be_calculated )
        {
            new_sell_price = hi_buy + ticksize;
            haggle_type = SLIPPAGE_CALCULATED;
        }
        // just add to the sell price
        else
        {
            new_sell_price = pos->sell_price + ticksize;
            haggle_type = SLIPPAGE_ADDITIVE;
        }

        kDebug() << QString( "(post-only) trying %1 sell price %2 tick size %3 for %4" )
                            .arg( haggle_type == SLIPPAGE_CALCULATED ? "calculated" :
                                  haggle_type == SLIPPAGE_ADDITIVE ? "additive  " : "unknown   " )
                            .arg( new_sell_price )
                            .arg( ticksize )
                            .arg( pos->stringifyOrderWithoutOrderID() );

        // set new prices
        pos->sell_price = new_sell_price;;
    }

    // set slippage
    pos->is_slippage = true;
    pos->price_reset_count++;

    // remove old price from prices index for detecting stray orders
    info.order_prices.removeOne( pos->price );

    // reapply offset, sentiment, price
    pos->applyOffset();

    // add new price from prices index for detecting stray orders
    info.order_prices.append( pos->price );
#endif
}

bool Engine::tryMoveOrder( Position* const &pos )
{
    // pos must be valid!

    const QString &market = pos->market;
    MarketInfo &info = market_info[ market ];
    Coin &hi_buy = info.highest_buy;
    Coin &lo_sell = info.lowest_sell;

    // return early when no ticker is set
    if ( hi_buy.isZeroOrLess() || lo_sell.isZeroOrLess() )
    {
        //kDebug() << "local warning: couldn't call tryMoveOrder because ticker is not yet set";
        return false;
    }

    const Coin &ticksize = info.price_ticksize;

    // replace buy price
    if ( pos->side == SIDE_BUY &&
         lo_sell >= ticksize *2 ) // sanity bounds check
    {
        // recalculate buy if needed - don't interfere with spread
        if ( pos->buy_price >= lo_sell ) // lo_sell <= ticksize shouldn't happen but is triggerable in tests
        {
            // set buy price to low sell - ticksize
            pos->buy_price = lo_sell - ticksize;
            pos->is_slippage = true;
            return true;
        }

        // try to obtain better buy price
        Coin new_buy_price = pos->buy_price;

        while ( new_buy_price >= ticksize && // new_buy_price >= SATOSHI
                new_buy_price < lo_sell - ticksize && //  new_buy_price < lo_sell - SATOSHI
                new_buy_price < pos->buy_price_original ) // new_buy_price < pos->buy_price_original
            new_buy_price += ticksize;

        // new possible price is better than current price and different
        if ( new_buy_price != pos->price &&
             new_buy_price.isGreaterThanZero() && // new_buy_price > 0
             new_buy_price <= pos->buy_price_original && // new_buy_price <= pos->buy_price_original
             new_buy_price != pos->buy_price && // new_buy_price != pos->buy_price_d
             new_buy_price < lo_sell ) // new_buy_price < lo_sell
        {
            pos->buy_price = new_buy_price;
            pos->is_slippage = true;
            return true;
        }

        if ( settings->is_chatty )
            kDebug() << "couldn't find better buy price for" << pos->stringifyOrder() << "new_buy_price"
                     << new_buy_price << "original_buy_price" << pos->buy_price_original
                     << "hi_buy" << hi_buy << "lo_sell" << lo_sell;
    }
    // replace sell price
    else if ( pos->side == SIDE_SELL &&
              hi_buy >= ticksize ) // sanity bounds check
    {
        // recalculate sell if needed - don't interfere with spread
        if ( pos->sell_price <= hi_buy )
        {
            // set sell price to high buy + ticksize;
            pos->sell_price = hi_buy + ticksize;
            pos->is_slippage = true;
            return true;
        }

        // try to obtain a better sell price
        Coin new_sell_price = pos->sell_price;

        while ( new_sell_price > ticksize * 2. && // only iterate down to 2 sat for a sell
                new_sell_price > hi_buy + ticksize &&
                new_sell_price > pos->sell_price_original )
            new_sell_price -= ticksize;

        // new possible price is better than current price and different
        if ( new_sell_price != pos->price &&
             new_sell_price > ticksize &&
             new_sell_price >= pos->sell_price_original &&
             new_sell_price != pos->sell_price &&
             new_sell_price > hi_buy )
        {
            pos->sell_price = new_sell_price;
            pos->is_slippage = true;
            return true;
        }

        if ( settings->is_chatty )
            kDebug() << "couldn't find better sell price for" << pos->stringifyOrder() << "new_sell_price"
                     << new_sell_price << "original_sell_price" << pos->sell_price_original
                     << "hi_buy" << hi_buy << "lo_sell" << lo_sell;
    }

    return false;
}

void Engine::onCheckTimeouts()
{
    positions->checkBuySellCount();

    // flow control
    if ( rest->yieldToFlowControl() )
        return;

    // avoid calculating timeouts if the number of queued requests is over limit_timeout_yield
    if ( rest->nam_queue.size() > rest->limit_timeout_yield )
        return;

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // store active spruce positions, but only if spruce is active
    static QMap<QString, Coin> spruce_offset;
    if ( spruce.isActive() )
        spruce_offset = positions->getActiveSpruceOrdersOffset();

    // look for timed out requests
    for ( QSet<Position*>::const_iterator i = positions->queued().begin(); i != positions->queued().end(); i++ )
    {
        Position *const &pos = *i;

        // make sure the order hasn't been set and the request is stale
        if ( pos->order_set_time == 0 &&
             pos->order_request_time > 0 &&
             pos->order_request_time + settings->request_timeout < current_time )
        {
            kDebug() << "order timeout detected, resending" << pos->stringifyOrder();

            rest->sendBuySell( pos );
            return;
        }
    }

    // look for timed out things
    for ( QSet<Position*>::const_iterator j = positions->active().begin(); j != positions->active().end(); j++ )
    {
        Position *const &pos = *j;

        // search for cancel order we should recancel
        if ( pos->is_cancelling &&
             pos->order_set_time > 0 &&
             pos->order_cancel_time > 0 &&
             pos->order_cancel_time < current_time - settings->cancel_timeout )
        {
            positions->cancel( pos );
            return;
        }

        // search for slippage order we should replace
        if ( pos->is_slippage &&
            !pos->is_cancelling &&
             pos->order_set_time > 0 &&
             pos->order_set_time < current_time - market_info[ pos->market ].slippage_timeout )
        {
            // reconcile slippage price according to spread hi/lo
            if ( tryMoveOrder( pos ) )
            {
                // we found a better price, mark resetting and cancel
                positions->cancel( pos, false, CANCELLING_FOR_SLIPPAGE_RESET );
                return;
            }
            else
            {
                // don't check it until new timeout occurs
                pos->order_set_time = current_time - settings->safety_delay_time;
            }
        }

        // search for one-time order with age > max_age_minutes
        if ( pos->is_onetime &&
             pos->order_set_time > 0 &&
             pos->max_age_minutes > 0 &&
             current_time > pos->order_set_time + ( 60000 * pos->max_age_minutes ) )
        {
            // the order has reached max age
            positions->cancel( pos, false, CANCELLING_FOR_MAX_AGE );
            return;
        }

        // search for stale spruce order
        if ( pos->is_onetime &&
             pos->order_set_time > 0 &&
             pos->strategy_tag == "spruce" )
        {
            /// step 1: look for bad/stale prices
            // get spread price for new spruce order
            const QString &market = pos->market;
            QPair<Coin,Coin> spread = getSpruceSpread( market );
            const Coin &buy_price = spread.first;
            const Coin &sell_price = spread.second;
            const Coin amount_to_shortlong = spruce.getAmountToShortLongNow( market );
            const Coin order_size = spruce.getOrderSize( market );

            // if the price is suboptimal, we should cancel it
            if ( pos->order_set_time < current_time - ( 20 * 60000 ) &&
                 buy_price.isGreaterThanZero() && sell_price.isGreaterThanZero() &&
                 ( ( pos->side == SIDE_BUY  && pos->price < buy_price.ratio( 0.97 ) ) ||
                   ( pos->side == SIDE_SELL && pos->price > sell_price.ratio( 1.03 ) ) ) )
            {
                positions->cancel( pos, false, CANCELLING_FOR_SPRUCE );
                return;
            }

            // if the order is active but our rating is the opposite polarity, cancel it
            if ( ( amount_to_shortlong > order_size *2              && pos->side == SIDE_BUY ) ||
                 ( amount_to_shortlong < Coin() - ( order_size *2 ) && pos->side == SIDE_SELL ) )
            {
                positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_2 );
                return;
            }

            /// step 2: look for spruce active <> what we should short/long
            if ( ( pos->side == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
                   amount_to_shortlong + spruce_offset.value( market ) > order_size *2 ) ||
                 ( pos->side == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
                   amount_to_shortlong + spruce_offset.value( market ) < Coin() - ( order_size *2 ) ) )
            {
                // if it's a buy, we should cancel the lowest price. if it's a sell, cancel the highets price.
                Position *const &pos_to_cancel = pos->side == SIDE_BUY ? positions->getLowestSpruceBuy( market ) :
                                                                         positions->getHighestSpruceSell( market );

                // check badptr just incase, but should be impossible to get here
                if ( !pos )
                    continue;

                positions->cancel( pos_to_cancel, false, CANCELLING_FOR_SPRUCE_3 );
                return;
            }
        }
    }
}

void Engine::onCheckDivergeConverge()
{
    checkMaintenance(); // this should probably be somewhere else, but we'll piggyback this timer
    cleanGraceTimes(); // this should happen every once in a while, might as well put it here

    // flow control
    if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
        return;

    QMap<QString/*market*/,qint32> market_hi_buy_idx; // calculate hi_buy position for each market
    QMap<QString/*market*/,bool> market_has_slippage; // track if market has a slippage order
    for ( QSet<Position*>::const_iterator i = positions->all().begin(); i != positions->all().end(); i++ )
    {
        Position *const &pos = *i;
        const QString &market = pos->market;

        // track market_has_slippage
        if ( pos->is_slippage )
        {
            market_has_slippage[ market ] = true;
            continue;
        }

        // skip if one-time order
        if ( pos->is_onetime )
            continue;

        if ( pos->side == SIDE_BUY )
        {
            const qint32 highest_idx = pos->getHighestMarketIndex();

            // fill market_hi_buy_idx
            if ( highest_idx > market_hi_buy_idx.value( market, -1 ) )
                market_hi_buy_idx[ market ] = highest_idx;
        }
    }

    QMap<QString/*market*/,QVector<qint32>> converge_buys, converge_sells, diverge_buys, diverge_sells;

    // look for orders we should converge/diverge in order from lo->hi
    for ( QSet<Position*>::const_iterator i = positions->all().begin(); i != positions->all().end(); i++ )
    {
        Position *const &pos = *i;
        const QString &market = pos->market;
        const MarketInfo &info = market_info[ pos->market ];

        // skip if one-time order or market has slippage
        if ( pos->is_onetime || market_has_slippage.value( market ) )
            continue;

        // check for market dc size
        if ( info.order_dc < 2 )
            continue;

        const qint32 first_idx = pos->getLowestMarketIndex();

        // check buy orders
        if (  pos->side == SIDE_BUY &&                              // buys only
             !pos->is_cancelling &&                                 // must not be cancelling
             !( !settings->should_dc_slippage_orders && pos->is_slippage ) && // must not be slippage
              pos->order_number.size() &&                           // must be set
             !positions->isDivergingConverging( market, first_idx ) &&
             !converge_buys[ market ].contains( first_idx ) &&
             !diverge_buys[ market ].contains( first_idx ) )
        {
            const qint32 buy_landmark_boundary = market_hi_buy_idx[ market ] - info.order_landmark_start;
            const qint32 hi_idx = pos->getHighestMarketIndex();

            // normal buy that we should converge
            if     ( !pos->is_landmark &&
                     hi_idx < buy_landmark_boundary - info.order_dc_nice )
            {
                converge_buys[ market ].append( first_idx );
            }
            // landmark buy that we should diverge
            else if ( pos->is_landmark &&
                      hi_idx > buy_landmark_boundary )
            {
                diverge_buys[ market ].append( first_idx );
            }
        }

        //check sell orders
        if (  pos->side == SIDE_SELL &&                             // sells only
             !pos->is_cancelling &&                                 // must not be cancelling
             !( !settings->should_dc_slippage_orders && pos->is_slippage ) && // must not be slippage
              pos->order_number.size() &&                           // must be set
             !positions->isDivergingConverging( market, first_idx ) &&
             !converge_sells[ market ].contains( first_idx ) &&
             !diverge_sells[ market ].contains( first_idx ) )
        {
            const qint32 sell_landmark_boundary = market_hi_buy_idx[ market ] + 1 + info.order_landmark_start;
            const qint32 lo_idx = pos->getLowestMarketIndex();

            // normal sell that we should converge
            if     ( !pos->is_landmark &&
                     lo_idx > sell_landmark_boundary + info.order_dc_nice )
            {
                converge_sells[ market ].append( first_idx );
            }
            // landmark sell that we should diverge
            else if ( pos->is_landmark &&
                      lo_idx < sell_landmark_boundary ) // check idx
            {
                diverge_sells[ market ].append( first_idx );
            }
        }
    }

    converge( converge_buys, SIDE_BUY ); // converge buys (many)->(one)
    converge( converge_sells, SIDE_SELL ); // converge sells (many)->(one)

    diverge( diverge_buys ); // diverge buy (one)->(many)
    diverge( diverge_sells ); // diverge sell (one)->(many)
}

void Engine::onSpruceUp()
{
    if ( !spruce.isActive() )
        return;

    spruce.clearLiveNodes();
    QMap<QString/*currency*/,Coin> spread_price;
    const QList<QString> &currencies = spruce.getCurrencies();
    for ( QList<QString>::const_iterator i = currencies.begin(); i != currencies.end(); i++ )
    {
        const QString &currency = *i;
        Coin price = getSpreadPriceForCurrency( currency, spruce.getBaseCurrency() );

        // if the ticker isn't updated, just skip this whole function
        if ( price.isZeroOrLess() )
        {
            kDebug() << "no ticker for currency" << spruce.getBaseCurrency() << currency;
            return;
        }

        spread_price.insert( currency, price );
        spruce.addLiveNode( currency, price );
    }

    spruce.calculateAmountToShortLong();

    // count value of spruce positions for each market
    QMap<QString,Coin> spruce_active = positions->getActiveSpruceOrdersTotal();

    const QMap<QString,Coin> &amount_to_shortlong_map = spruce.getAmountToShortLongMap();
    const Coin &shortlong_total = spruce.getAmountToShortLongTotal();

    Coin coeff_diff_pct = spruce.startCoeffs().hi_coeff / spruce.startCoeffs().lo_coeff;
    kDebug() << QString( "[Spruce] coeffs[%1 %2 %3 %4] diff[%5%] shortlong[%6] leverage[%7]" )
                    .arg( spruce.startCoeffs().lo_currency )
                    .arg( spruce.startCoeffs().lo_coeff )
                    .arg( spruce.startCoeffs().hi_currency )
                    .arg( spruce.startCoeffs().hi_coeff )
                    .arg( coeff_diff_pct )
                    .arg( shortlong_total )
                    .arg( spruce.getActiveLeverage() );

    const Coin long_max = spruce.getLongMax();
    const Coin short_max = spruce.getShortMax();

    for ( QMap<QString,Coin>::const_iterator i = amount_to_shortlong_map.begin(); i != amount_to_shortlong_map.end(); i++ )
    {
        const QString &market = i.key();
        const Coin &amount_to_shortlong = i.value();

        kDebug() << QString( "[Spruce] %1 rating %2 on-order %3" )
                       .arg( market, 10 )
                       .arg( amount_to_shortlong, 13 )
                       .arg( spruce_active.value( market ), 13 );

        const bool is_buy = amount_to_shortlong.isZeroOrLess();
        const Coin order_size = spruce.getOrderSize( market );
        const Coin order_max = spruce.getMarketMax( market );
        const Coin amount_to_shortlong_abs = amount_to_shortlong.abs();

        // skip noisy amount
        if ( amount_to_shortlong_abs < order_size *2 )
            continue;

        // don't go over our per-market max
        if ( spruce_active.value( market ) + order_size >= order_max )
            continue;

        // don't go over the abs value of our new projected position
        if ( spruce_active.value( market ) + order_size *2 >= amount_to_shortlong_abs )
            continue;

        // are we too long/short to place another order on this side?
        if ( is_buy && shortlong_total > long_max )
        {
            kDebug() << "[Spruce] info: too long for now";
            continue;
        }
        else if ( !is_buy && shortlong_total < short_max )
        {
            kDebug() << "[Spruce] info: too short for now";
            continue;
        }

        // get spread price for new spruce order
        const QPair<Coin,Coin> spread = getSpruceSpread( market );
        const Coin &buy_price = spread.first;
        const Coin &sell_price = spread.second;


        // cancel conflicting positions
        for ( QSet<Position*>::const_iterator j = positions->all().begin(); j != positions->all().end(); j++ )
        {
            Position *const &pos = *j;

            if ( pos->market != market )
                continue;

            if ( (  is_buy && pos->side == SIDE_SELL && buy_price  >= pos->sell_price.ratio( 0.9945 ) ) ||
                 ( !is_buy && pos->side == SIDE_BUY  && sell_price <= pos->buy_price.ratio( 1.0055 ) ) )
            {
                kDebug() << "[Spruce] cancelling conflicting order" << pos->order_number;
                positions->cancel( pos );
            }
        }

        // queue the order quietly
        addPosition( market, is_buy ? SIDE_BUY : SIDE_SELL, buy_price, sell_price, order_size,
                     "onetime", "spruce", QVector<qint32>(), false, true );
    }
}

void Engine::converge( QMap<QString, QVector<qint32>> &market_map, quint8 side )
{
    int index_offset = side == SIDE_BUY ? 1 : -1;

    for ( QMap<QString/*market*/,QVector<qint32>>::const_iterator i = market_map.begin(); i != market_map.end(); i++ )
    {
        const QString &market = i.key();
        QVector<qint32> indices = i.value();

        const qint32 dc_value = market_info[ market ].order_dc;

        // check for indices size
        if ( indices.size() < dc_value || dc_value < 2 )
            continue;

        // walk the indices from hi->lo
        if ( side == SIDE_BUY )
            std::sort( indices.begin(), indices.end() );
        else // reverse sort for sells
            std::sort( indices.rbegin(), indices.rend() );

        QVector<qint32> new_order;

        for ( int j = 0; j < indices.size(); j++ )
        {
            const qint32 index = indices.value( j );

            // add the first item, if we don't have one
            if ( new_order.isEmpty() )
                new_order.append( index );
            // enforce sequential
            else if ( index == new_order.value( new_order.size() -1 ) + index_offset )
                new_order.append( index );
            // we found non-sequential indices, remove index 0 and restart the loop from 0
            else
            {
                indices.removeFirst();
                new_order.clear();

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
            if ( new_order.size() == dc_value )
            {
                if ( verbosity > 0 )
                    kDebug() << QString( "converging %1 %2" )
                                 .arg( market, -8 )
                                 .arg( Global::printVectorqint32( new_order ) );

                // store positions we are cancelling
                QVector<Position*> position_list;

                // cancel these indices
                for ( int k = 0; k < new_order.size(); k++ )
                {
                    const qint32 idx = new_order.value( k );
                    Position *const &pos = positions->getByIndex( market, idx );

                    position_list.append( pos );

                    // keep track of indices we should avoid autosetting
                    positions->diverging_converging[ market ].append( idx );

                    // insert into a map for tracking for when we meet dc size (note: inside the loop for tests)
                    if ( position_list.size() == dc_value )
                        positions->diverge_converge.insert( position_list, qMakePair( true, new_order ) );

                    positions->cancel( pos, true, CANCELLING_FOR_DC );
                }

                new_order.clear(); // clear new_order
                break; // 1 order per market
            }
        }

        // flow control
        if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
            return;
    }
}

void Engine::diverge( QMap<QString, QVector<qint32> > &market_map )
{
    for ( QMap<QString/*market*/,QVector<qint32>>::const_iterator i = market_map.begin(); i != market_map.end(); i++ )
    {
        const QString &market = i.key();
        QVector<qint32> indices = i.value();

        // check for indices size
        if ( indices.isEmpty() )
            continue;

        // walk the indices from hi->lo
        std::sort( indices.begin(), indices.end() );

        const qint32 index = indices.value( 0 );
        Position *const &pos = positions->getByIndex( market, index ); // get position for index

        if ( verbosity > 0 )
            kDebug() << QString( "diverging  %1 %2" )
                         .arg( market, -8 )
                         .arg( Global::printVectorqint32( pos->market_indices ) );

        // store a list of indices we must set after the cancel is complete
        for ( int k = 0; k < pos->market_indices.size(); k++ )
            positions->diverging_converging[ market ].append( pos->market_indices.value( k ) );

        // insert into a map for tracking for when cancels are complete
        positions->diverge_converge.insert( QVector<Position*>() << pos, qMakePair( false, pos->market_indices ) );

        // cancel the order
        positions->cancel( pos, true, CANCELLING_FOR_DC );

        // flow control
        if ( rest->yieldToFlowControl() || rest->nam_queue.size() >= rest->limit_commands_queued_dc_check )
            return;
    }
}

void Engine::handleUserMessage( const QString &str )
{
    kDebug() << QString( "[Engine] got WSS command: %1" )
                .arg( str );

    // init current state
    if ( str == "getstate" )
    {
        QJsonArray reply;

        for( QHash<QString, MarketInfo>::const_iterator i = market_info.begin(); i != market_info.end(); i++ )
        {
            const QString &market = i.key();
            const MarketInfo &info = i.value();

            info.jsonifyTicker( reply, market );
        }

        for( QSet<Position*>::const_iterator i = positions->all().begin(); i != positions->all().end(); i++ )
        {
            (*i)->jsonifyPositionSet( reply );
        }

        QString msg = Global::jsonArrayToString( reply );
        emit newEngineMessage( msg );
    }
}

void Engine::setMarketSettings( QString market, qint32 order_min, qint32 order_max, qint32 order_dc, qint32 order_dc_nice,
                                qint32 landmark_start, qint32 landmark_thresh, bool market_sentiment, qreal market_offset )
{
    MarketInfo &info = market_info[ market ];

    info.order_min = order_min;
    info.order_max = order_max;
    info.order_dc = order_dc;
    info.order_dc_nice = order_dc_nice;
    info.order_landmark_start = landmark_start;
    info.order_landmark_thresh = landmark_thresh;
    info.market_sentiment = market_sentiment;
    info.market_offset = market_offset;
}

#include "engine.h"
#include "engine_test.h"
#include "trexrest.h"
#include "bncrest.h"
#include "polorest.h"
#include "wavesrest.h"
#include "positionman.h"
#include "enginesettings.h"
#include "market.h"
#include "alphatracker.h"
#include "sprucev2.h"

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
#include <QTimer>

Engine::Engine( const quint8 _engine_type )
    : QObject( nullptr ),
      positions( new PositionMan( this ) ),
      settings( new EngineSettings( _engine_type ) )
{
    engine_type = _engine_type;

    kDebug() << "[Engine" << engine_type << "]";

    start_time = QDateTime::currentDateTime();

    // engine maintenance timer
    maintenance_timer = new QTimer( this );
    connect( maintenance_timer, &QTimer::timeout, this, &Engine::onEngineMaintenance );
    maintenance_timer->setTimerType( Qt::VeryCoarseTimer );
    maintenance_timer->start( 60000 );
}

Engine::~Engine()
{
    maintenance_timer->stop();

    delete maintenance_timer;
    delete positions;
    delete settings;

    maintenance_timer = nullptr;
    positions = nullptr;
    settings = nullptr;

    kDebug() << "[Engine] done.";
}

Position *Engine::addPosition( QString market_input, quint8 side, QString buy_price, QString sell_price,
                               QString order_size, QString type, QString strategy_tag, QVector<qint32> indices,
                               bool landmark, bool quiet )
{
    // don't add a position on an exchange without a key and secret
    if ( rest_arr.value( engine_type )->isKeyOrSecretUnset() )
    {
        kDebug() << "local error: tried to add a new position on exchange" << engine_type << "but key or secret is unset";
        return nullptr;
    }

    // convert to universal market format <base>_<quote>
    Market market = Market( market_input );
    if ( !market.isValid() )
    {
        kDebug() << "local error: incorrect market format. you used '" << market_input
                 << "'. please use universal market format 'base_quote' or 'base-quote'. for example 'BTC_DOGE' or 'BTC-DOGE'";
        return nullptr;
    }

    if ( !getMarketInfo( market ).is_tradeable )
    {
        // invert market pair
        market = market.getInverse();

        // flip side
        side = ( side == SIDE_BUY ) ? SIDE_SELL :
                                      SIDE_BUY;

        // invert cross-prices
        const Coin new_buy_price = Coin( sell_price ).isZeroOrLess() ? Coin() : CoinAmount::COIN / sell_price;
        const Coin new_sell_price = Coin( buy_price ).isZeroOrLess() ? Coin() : CoinAmount::COIN / buy_price;
        const Coin new_size = ( side == SIDE_BUY ) ? new_buy_price * order_size :
                                                     new_sell_price * order_size;

        // set new prices/size
        buy_price = new_buy_price;
        sell_price = new_sell_price;
        order_size = new_size;

//        kDebug() << "market" << market << "is not tradeable, converting to tradeable market"
//                 << market_inverse << "new_buy:" << new_buy_price << "new_sell:" << new_sell_price
//                 << "new_size:" << new_size;
    }

    MarketInfo &info = market_info[ market ];

    // check if bid/ask price exists
    if ( !info.spread.isValid() )
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
    const bool is_ghost = type.startsWith( GHOST );
    const bool is_active = type.startsWith( ACTIVE );
    const bool is_override = type.contains( "-override" );

    // check for incorrect order type
    if ( !is_active && !is_ghost && !is_onetime )
    {
        kDebug() << "local error: please specify 'active', 'ghost', or 'onetime' for the order type";
        return nullptr;
    }

    // check for blank argument
    if ( buy_price.isEmpty() || sell_price.isEmpty() || order_size.isEmpty() )
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
         ( is_onetime && !alternate_size.isEmpty() && Coin( alternate_size ).isZeroOrLess() ) )
    {
        kDebug() << "local error: tried to set bad" << ( is_onetime ? "one-time" : "ping-pong" ) << "order. hi price"
                 << sell_price << "lo price" << buy_price << "size" << order_size << "alternate size" << alternate_size;
        return nullptr;
    }
    // reformat strings
    QString formatted_buy_price = Coin( buy_price );
    QString formatted_sell_price = Coin( sell_price );
    QString formatted_order_size = Coin( order_size );

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
        ( ( side == SIDE_SELL && info.spread.ask.ratio( 0.8 ) > Coin( sell_price ) ) || // ask * 0.9 > sell_price
          ( side == SIDE_SELL && info.spread.ask.ratio( 1.2 ) < Coin( sell_price ) ) || // ask * 1.1 < sell_price
          ( side == SIDE_BUY && info.spread.bid.ratio( 1.2 ) < Coin( buy_price ) ) ||   // bid * 1.1 < buy_price
          ( side == SIDE_BUY && info.spread.bid.ratio( 0.8 ) < Coin( buy_price ) ) ) )  // bid * 0.9 > buy_price
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

    // check for correctly loaded position data and size
    if ( !pos ||
         !pos->market.isValid() ||
          pos->price.isZeroOrLess() ||
          pos->amount.isZeroOrLess() ||
          pos->quantity.isZeroOrLess() )
    {
        kDebug() << "local warning: failed to set order because of invalid value:" << market << pos->side << pos->buy_price << pos->sell_price << pos->amount << pos->quantity << indices << landmark;
        if ( pos ) delete pos;
        return nullptr;
    }

    // check for minimum position size
    static const Coin minimum_order_size = engine_type == ENGINE_BITTREX  ? Coin( BITTREX_MINIMUM_ORDER_SIZE ) :
                                           engine_type == ENGINE_BINANCE  ? Coin( BINANCE_MINIMUM_ORDER_SIZE ) :
                                           engine_type == ENGINE_POLONIEX ? Coin( POLONIEX_MINIMUM_ORDER_SIZE ) :
                                           engine_type == ENGINE_WAVES    ? Coin( WAVES_MINIMUM_ORDER_SIZE ) :
                                                                            Coin();

    if ( pos->amount < minimum_order_size - CoinAmount::SATOSHI )
    {
        kDebug() << "local warning: failed to set order: size" << pos->amount << "is under the minimum size" << minimum_order_size;
        return nullptr;
    }

    // enforce PERCENT_PRICE on binance
    if ( engine_type == ENGINE_BINANCE )
    {
        // respect the binance limits with a 20% padding (we don't know what the 5min avg is, so we'll just compress the range)
        Coin buy_limit = ( info.spread.bid * info.price_min_mul.ratio( 1.2 ) ).truncatedByTicksize( "0.00000001" );
        Coin sell_limit = ( info.spread.ask * info.price_max_mul.ratio( 0.8 ) ).truncatedByTicksize( "0.00000001" );

        // regardless of the order type, enforce lo/hi price >0 to be in bounds
        if ( ( pos->side == SIDE_BUY  && pos->buy_price.isGreaterThanZero() && buy_limit.isGreaterThanZero() && pos->buy_price < buy_limit ) ||
             ( pos->side == SIDE_SELL && pos->sell_price.isGreaterThanZero() && sell_limit.isGreaterThanZero() && pos->sell_price > sell_limit ) )
        {
            if ( pos->is_onetime ) // if ping-pong, don't warn
                kDebug() << "local warning: hit PERCENT_PRICE limit for" << market << buy_limit << sell_limit << "for pos" << pos->stringifyOrderWithoutOrderID();
            delete pos;
            return nullptr;
        }
    }

    pos->is_onetime = is_onetime;
    pos->is_taker = is_taker;

    // allow one-time orders to set a timeout
    if ( type.contains( "-timeout" ) )
    {
        bool ok = true;
        int read_from = type.indexOf( "-timeout" ) + 8;
        int timeout = type.mid( read_from, type.size() - read_from ).toInt( &ok );

        if ( ok && timeout > 0 )
            pos->max_age_epoch = QDateTime::currentMSecsSinceEpoch() + ( timeout * 60000 );
    }

    // TODO: if the positon is simulated in an inverted market, don't run this. otherwise, run it
    // if it's not a taker order, enable local post-only mode
//    if ( info.is_tradeable && !is_taker )
//    {
//        // if we are setting a new position, try to obtain a better price
//        if ( tryMoveOrder( pos ) )
//        {
//            pos->applyOffset();
//        }

//        if ( pos->side == SIDE_SELL && pos->sell_price < info.spread.ask )
//        {
//            info.spread.ask = pos->sell_price;

//            // avoid collision
//            if ( info.lowest_sell <= info.bid )
//                info.bid = info.spread.ask - info.price_ticksize;
//        }

//        if ( pos->side == SIDE_BUY  && pos->buy_price > info.bid )
//        {
//            info.bid = pos->buy_price;

//            // avoid collision
//            if ( info.bid >= info.spread.ask )
//                info.spread.ask = info.bid + info.price_ticksize;
//        }
//    }

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
    sendBuySell( pos, quiet );
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
    // 1 = getorder
    // 2 = history
    // 3 = ticker
    // 4 = cancel
    // 5 = wss
    // 6 = order list

    static const QStringList fill_strings = QStringList()
            << "getorder"
            << "history"
            << "ticker"
            << "cancel"
            << "wss"
            << "ordlist";

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

    MarketInfo &info = market_info[ pos->market ];

    Coin new_price;
#if defined(SPREAD_EXPAND_FULL)
    new_price = pos->price;
#elif defined(SPREAD_EXPAND_HALF)
    new_price = pos->side == SIDE_SELL ? ( pos->price + info.spread.ask ) /2 :
                                         ( pos->price + info.bid ) /2;
#endif

    // widen spread if we filled a buy < bid_price or sell > ask_price
    if ( new_price.isGreaterThanZero() )
    {
        if      ( pos->side == SIDE_BUY  && pos->price < info.spread.bid )
            info.spread.bid = new_price;
        else if ( pos->side == SIDE_SELL && pos->price > info.spread.ask )
            info.spread.ask = new_price;
    }

    // increment ping-pong "alternate_size" variable to take the place of order_size after 1 fill
    for ( int i = 0; i < pos->market_indices.size(); i++ )
    {
        // assure valid non-const access
        if ( info.position_index.size() <= pos->market_indices.value( i ) )
            continue;

        // increment fill count and resize by alternate size if one exists
        info.position_index[ pos->market_indices.value( i ) ].iterateFillCount();
    }

    QString fill_str = fill_strings.value( fill_type -1, "unknown" );
    if ( extra_data > 0 ) fill_str += QChar('-') + QString::number( extra_data );

    // update stats and print
    // note: btc_commission is set in rest->parseOrderHistory
    updateStatsAndPrintFill( fill_str, pos->market, pos->order_number, pos->side, pos->strategy_tag, pos->amount, Coin(), pos->price, pos->btc_commission );

    // set the next position
    flipPosition( pos );

    // on trex, remove any 'getorder's in queue related to this uuid, to prevent spam
    // if testing, don't access rest because it's null
    if ( !is_testing && engine_type == ENGINE_BITTREX )
        rest_arr.value( ENGINE_BITTREX )->removeRequest( TREX_COMMAND_GET_ORDER, QString( "uuid=%1" ).arg( order_id ) ); // note: uses pos*

    // delete
    positions->remove( pos );
}

void Engine::updateStatsAndPrintFill( const QString &fill_type, Market market, const QString &order_id, quint8 side,
                                      const QString &strategy_tag, Coin amount, Coin quantity, Coin price,
                                      const Coin &btc_commission )
{
    const QString base_currency = ( spruce->getBaseCurrency() == "disabled" ) ? "BTC" : spruce->getBaseCurrency();

    // check for valid inputs. amount or quantity must exist, and all others must be valid
    if ( amount.isZeroOrLess() && quantity.isZeroOrLess() )
    {
        kDebug() << "engine error: amount and quantity are both <= zero for" << market << order_id << "amount:" << amount << "qty:" << quantity << "@" << price;
        return;
    }
    else if ( price.isZeroOrLess() )
    {
        kDebug() << "engine error: price is <= zero for" << market << order_id << "amount:" << amount << "qty:" << quantity << "@" << price;
        return;
    }

    Market alpha_market_0, alpha_market_1;
    Coin market_0_quantity;
    /// found beta level trade, convert prices and volumes to base currency using an estimated conversion rate
    if ( market.getBase() != base_currency &&
         market.getQuote() != base_currency )
    {
        alpha_market_0 = Market( base_currency, market.getBase() );
        alpha_market_1 = Market( base_currency, market.getQuote() );

        const Coin price_in_btc = getMarketInfo( Market( base_currency, market.getBase() ) ).spread.bid;

        // check for valid price
        if ( !is_testing && !price_in_btc.isGreaterThanZero() )
        {
            kDebug() << "engine error: ticker price is <= zero for" << Market( base_currency, market.getBase() ) << order_id << "amount:" << amount << "qty:" << quantity << "@" << price << "ticker:" << price_in_btc;
            return;
        }

        // if the quantity wasn't supplied, transform qty to the market base
        if ( quantity.isZeroOrLess() )
        {
            market_0_quantity = amount;
            quantity = amount / price;
            amount = quantity * price_in_btc;
        }
        // vice versa, do the opposite
        else if ( amount.isZeroOrLess() )
        {
            amount = quantity * price;
            market_0_quantity = amount;
            quantity = amount / price_in_btc;
        }
    }
    /// found a market with our base currency in it, use direct conversion rate
    else
    {
        // one of these values should be zero, unless the exchange supplies both
        if ( amount.isZero() )
            amount = quantity * price;
        else if ( quantity.isZero() )
            quantity = amount / price;

        // if base market is not btc, but the quote is, calculate btc price with inverse
        if ( market.getBase() != base_currency &&
             market.getQuote() == base_currency )
        {
            // invert market to make stats show properly
            market = market.getInverse();

            // invert side
            side = ( side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY;

            // invert price, recalculate amt/qty
            const Coin amount_tmp = amount;

            price = CoinAmount::COIN / price;
            amount = quantity;
            quantity = amount_tmp;
        }

        // negate commission from final qty and calculate final amounts
        amount = amount - btc_commission;
        quantity = amount / price;
    }

    // add stats changes to alpha tracker (note: volume before commission is used)
    alpha->addAlpha( market, side, amount, price );
    alpha->addDailyVolume( QDateTime::currentSecsSinceEpoch(), amount );

//    if ( strategy_tag.contains( "flux" ) )
//    {
    // beta order, adjust both quantities. assume all fills are strategy orders
    if ( alpha_market_0.isValid() && alpha_market_1.isValid() )
    {
        const Coin quantity_offset_0 = ( side == SIDE_BUY ) ? -market_0_quantity
                                                            :  market_0_quantity;
        const Coin quantity_offset_1 = ( side == SIDE_BUY ) ?  quantity
                                                            : -quantity;

        spruce->adjustCurrentQty( alpha_market_0.getQuote(), quantity_offset_0 );
        spruce->adjustCurrentQty( alpha_market_1.getQuote(), quantity_offset_1 );
    }
    // normal order, subtract the qty of the alt (base doesn't need changing)
    else
    {
        // add qty changes to spruce strat
        const Coin quantity_offset = ( side == SIDE_BUY ) ?  quantity
                                                          : -quantity;
        const Coin amount_offset = ( side == SIDE_BUY ) ? -amount
                                                        :  amount;

        spruce->adjustCurrentQty( market.getQuote(), quantity_offset );
        spruce->adjustCurrentQty( market.getBase(), amount_offset );
    }
//    }

    if ( getVerbosity() > 0 )
    {
        const bool is_buy = ( side == SIDE_BUY );
        const QString side_str = QString( "%1%2>>>none<<<" )
                                 .arg( is_buy ? ">>>grn<<<" : ">>>red<<<" )
                                 .arg( is_buy ? "buy " : "sell" );

        kDebug() << QString( "fill-%1: %2 %3 a %4 q %5 @ %6 s %7 o %8" )
                    .arg( fill_type, -8 )
                    .arg( side_str )
                    .arg( market, -MARKET_STRING_WIDTH )
                    .arg( amount, -PRICE_WIDTH )
                    .arg( quantity, -PRICE_WIDTH )
                    .arg( price, -PRICE_WIDTH )
                    .arg( strategy_tag, -9 - MARKET_STRING_WIDTH )
                    .arg( order_id );
    }
}

void Engine::processFilledOrders( QVector<Position*> &to_be_filled, qint8 fill_type )
{
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
                fillNQ( pos->order_number, fill_type );
            }
        }
    }

    /// step 3: fill remaining outliers
    for ( QVector<Position*>::const_iterator i = to_be_filled.begin(); i != to_be_filled.end(); i++ )
    {
        Position *const &pos = *i;
        fillNQ( pos->order_number, fill_type );
    }
}

void Engine::processOpenOrders( QVector<QString> &order_numbers, QMultiHash<QString, OrderInfo> &orders, qint64 request_time_sent_ms )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch(); // cache time
    qint32 ct_cancelled = 0, ct_all = 0;

    QQueue<QString> stray_orders;
    QQueue<Market> stray_orders_markets;

    for ( QMultiHash<QString, OrderInfo>::const_iterator i = orders.begin(); i != orders.end(); i++ )
    {
        const QString &market = i.key();
        const OrderInfo &info = i.value();
        const quint8 &side = info.side;
        const QString &price = info.price;
        const QString &amount = info.amount;
        const QString &order_number = info.order_number;

        //kDebug() << "processing order" << order_number << market << side << amount << "@" << price;

        // if we ran cancelall, try to cancel this order
        if ( positions->isRunningCancelAll() )
        {
            ct_all++;

            // match our market filter arg1
            if ( positions->getCancelMarketFilter() != ALL &&
                 positions->getCancelMarketFilter() != market )
                continue;

            ct_cancelled++;

            // cancel stray orders
            if ( !positions->isValidOrderID( order_number ) )
            {
                kDebug() << "cancelling non-bot order" << market << side << amount << "@" << price << "id:" << order_number;

                // send a one time cancel request for orders we don't own
                sendCancel( order_number, nullptr, market );
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
                const Coin &amount_d = amount;
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
                         pos->amount == amount &&
                         amount_d >= pos->amount.ratio( 0.999 ) &&
                         amount_d <= pos->amount.ratio( 1.001 ) )
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
                kDebug() << "queued cancel for stray order" << market << side << amount << "@" << price << "id:" << order_number;
                stray_orders += order_number;

                // for waves, we need to record the market also
                if ( engine_type == ENGINE_WAVES )
                    stray_orders_markets += market;
            }
        }
    }

    // if we were cancelling orders, just return here
    if ( positions->isRunningCancelAll() )
    {
        kDebug() << "cancelled" << ct_cancelled << "orders," << ct_all << "orders total";
        positions->setRunningCancelAll( false ); // reset state to default
        return;
    }

    // cancel stray orders
    if ( stray_orders.size() > 50 )
    {
        kDebug() << "local warning: mitigating cancelling >50 stray orders";
    }
    else
    {
        while ( !stray_orders.isEmpty() )
        {
            const QString &order_number = stray_orders.takeFirst();
            const Market &market = engine_type == ENGINE_WAVES ? stray_orders_markets.takeFirst() : Market();

            sendCancel( order_number, nullptr, market );
            // reset grace time incase we see this order again from the next response
            order_grace_times.insert( order_number, current_time + settings->stray_grace_time_limit /* don't try to cancel again for 10m */ );
        }

    }

    // mitigate blank orderbook flash
    if ( settings->should_mitigate_blank_orderbook_flash &&
         !order_numbers.size() && // the orderbook is blank
         positions->active().size() >= 20 ) // we have some orders, don't make it too low (if it's 2 or 3, we might fill all those orders at once, and the mitigation leads to the orders never getting filled)
    {
        kDebug() << "local warning: blank orderbook flash has been mitigated!";
        return;
    }

    // now we can look for local positions to invalidate based on if the order exists
    // (except bittrex, which is laggy. we'll just use the fill history there for fills)
    if ( engine_type != ENGINE_BITTREX )
    {
        QVector<Position*> filled_orders;

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

            // on waves, just call getorder
            if ( engine_type == ENGINE_WAVES )
            {
                if ( !orders_for_polling.contains( pos->order_number ) )
                    orders_for_polling += pos->order_number;
            }
            // add orders to process
            else
            {
                filled_orders += pos;
            }
        }

        // on other exchanges, process filled order
        processFilledOrders( filled_orders, FILL_ORDERLIST );
    }

    // set last orderbook processed time
    m_last_orderbook_processed_time = current_time;
}

void Engine::processTicker( BaseREST *base_rest_module, const QMap<QString, Spread> &ticker_data, qint64 request_time_sent_ms )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // update ticker update time
    base_rest_module->ticker_update_time = current_time;

    // store deleted positions, because we can't delete and iterate a hash<>

    for ( QMap<QString, Spread>::const_iterator i = ticker_data.begin(); i != ticker_data.end(); i++ )
    {
        const Market market = i.key();
        const Spread &spread = i.value();
        const Coin &ask = spread.ask;
        const Coin &bid = spread.bid;

        // check for missing information
        if ( ask.isZeroOrLess() || bid.isZeroOrLess() )
            continue;

        // update values for market
        MarketInfo &info = market_info[ market ];

        info.spread.bid = bid;
        info.spread.ask = ask;
        info.is_tradeable = true;

        // TODO: make this faster
        // update values for inverse market, if it is not tradeable
        Market market_inverse = market.getInverse();
        MarketInfo &info_inverse = market_info[ market_inverse ];

        // if it doesn't have an active ticker, update it with the inverse market ticker
        if ( !info_inverse.is_tradeable )
        {
            // cross prices
            info_inverse.spread.bid = CoinAmount::COIN / ask;
            info_inverse.spread.ask = CoinAmount::COIN / bid;

            // cross ticksizes (probably not needed)
//            info_inverse.price_ticksize = info.quantity_ticksize;
//            info_inverse.quantity_ticksize = info.price_ticksize;
        }
    }

    // if this is a ticker feed, just process the ticker data. the fill feed will cause false fills when the ticker comes in just as new positions were set,
    // because we have no request time to compare the position set time to.
    if ( request_time_sent_ms <= 0 )
        return;

    if ( engine_type == ENGINE_POLONIEX )
    {
        // if we read the ticker from anywhere and the websocket account feed is active, prevent it from filling positions (websocket feed is instant for fill notifications anyways)
        if ( reinterpret_cast<PoloREST*>( rest_arr.value( ENGINE_POLONIEX ) )->getWSS1000State() )
            return;
    }

    if ( engine_type == ENGINE_POLONIEX ||
         engine_type == ENGINE_BINANCE )
    {
        QVector<Position*> filled_orders;

        // did we find bid == ask (we shouldn't have)
        bool found_equal_bid_ask = false;

        // check for any orders that could've been filled
        // (note: removed because ping-pong is deprecated, history-fill is preferred)
        for ( QSet<Position*>::const_iterator j = positions->active().begin(); j != positions->active().end(); j++ )
        {
            Position *const &pos = *j;

            if ( !pos )
                continue;

            const QString &market = pos->market;
            if ( market.isEmpty() || !ticker_data.contains( market ) )
                continue;

            const Spread &spread = ticker_data[ market ];

            const Coin &ask = spread.ask;
            const Coin &bid = spread.bid;

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
                    // skip the order until it's a few seconds older
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
}

void Engine::processCancelledOrder( Position * const &pos )
{
    // pos must be valid!

    // we succeeded at cancelling a slippage position or timed out position, now put it back to the -same side- and at its original prices
    if ( ( pos->is_slippage && pos->cancel_reason == CANCELLING_FOR_SLIPPAGE_RESET ) ||
         ( !pos->is_onetime && pos->cancel_reason == CANCELLING_FOR_MAX_AGE ) )
    {
        if ( pos->strategy_tag.contains( "flux" ) )
        {
            addPosition( pos->market, pos->side, pos->buy_price_original, pos->sell_price_original, pos->original_size, "onetime", pos->strategy_tag,
                         QVector<qint32>(), false, true );
        }
        else if ( pos->is_landmark )
        {
            addLandmarkPositionFor( pos );
        }
        else
        {
            const PositionData &new_pos = market_info[ pos->market ].position_index.value( pos->market_indices.value( 0 ) );

            addPosition( pos->market, pos->side, new_pos.buy_price, new_pos.sell_price, new_pos.order_size, ACTIVE, "",
                         pos->market_indices, false, true );
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

void Engine::cancelOrderMeatDCOrder( Position * const &pos )
{
    QVector<Position*> cancelling_positions;
    bool new_order_is_landmark = false;
    QVector<qint32> new_indices;

    // look for our position's DC list and try to obtain it into cancelling_positions
    for ( QMap<QVector<Position*>, QPair<bool, QVector<qint32>>>::const_iterator i = positions->getDCMap().begin(); i != positions->getDCMap().end(); i++ )
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

        positions->getDCMap().remove( position_list );
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
                positions->getDCPending()[ pos->market ].removeOne( new_indices.value( i ) );

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
                positions->getDCPending()[ pos->market ].removeOne( idx );

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
        positions->getDCMap().insert( cancelling_positions, qMakePair( new_order_is_landmark, new_indices ) );
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
            if ( !pos_data.alternate_size.isEmpty() )
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

void Engine::loadSettings()
{
    const QString path = getSettingsPath();
    QFile loadfile( path );

    if ( !loadfile.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        kDebug() << "local warning: couldn't load optional engine settings file" << path;
        return;
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    // emit new lines
    QString data = loadfile.readAll();
    kDebug() << "[Engine] loaded optional engine settings," << data.size() << "bytes.";

    emit gotUserCommandChunk( data );
}

void Engine::flipPosition( Position *const &pos )
{
    // pos must be valid!

    // if it's not a ping-pong order, don't pong
    if ( pos->is_onetime )
        return;

    pos->flip(); // flip our position

    // we cancelled for shortlong, track stats related to this strategy tag
//    if ( pos->cancel_reason == CANCELLING_FOR_SHORTLONG )
//        stats->addStrategyStats( pos );

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
    while ( !removed.isEmpty() )
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

    kDebug() << "diverge_converge: " << positions->getDCPending();
    kDebug() << "diverging_converging: " << positions->getDCMap();
}

bool Engine::isOrderBookResponsive() const
{
    return m_last_orderbook_processed_time > QDateTime::currentMSecsSinceEpoch() - settings->orderbook_stale_time;
}

void Engine::findBetterPrice( Position *const &pos )
{
    if ( engine_type == ENGINE_BITTREX )
    {
        kDebug() << "local warning: tried to run findBetterPrice() on bittrex but does not a have post-only mode";
        return;
    }

    static const quint8 SLIPPAGE_CALCULATED = 1;
    static const quint8 SLIPPAGE_ADDITIVE = 2;

    // bad ptr check
    if ( !pos || !positions->isValid( pos ) )
        return;

    bool is_buy = ( pos->side == SIDE_BUY );
    const QString &market = pos->market;
    MarketInfo &info = market_info[ market ];
    Coin &hi_buy = info.spread.bid;
    Coin &lo_sell = info.spread.ask;
    Coin ticksize;

    if ( engine_type == ENGINE_BINANCE )
    {
        ticksize = info.price_ticksize;

        if ( pos->price_reset_count > 0 )
            ticksize += ticksize * qFloor( ( qPow( pos->price_reset_count, 1.110 ) ) );
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        const qreal slippage_mul = reinterpret_cast<PoloREST*>( rest_arr.value( ENGINE_POLONIEX ) )->getSlippageMul( market );

        if ( is_buy ) ticksize = pos->buy_price.ratio( slippage_mul ) + CoinAmount::SATOSHI;
        else          ticksize = pos->sell_price.ratio( slippage_mul ) + CoinAmount::SATOSHI;
    }

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
        info.spread.ask = pos->buy_price;
        lo_sell = pos->buy_price;

        // avoid collision
        if ( info.spread.ask <= info.spread.bid )
            info.spread.bid = info.spread.ask - info.price_ticksize;
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
        info.spread.bid = pos->sell_price;
        hi_buy = pos->sell_price;

        // avoid collision
        if ( info.spread.bid >= info.spread.ask )
            info.spread.ask = info.spread.bid + info.price_ticksize;
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
}

void Engine::sendBuySell( Position * const &pos , bool quiet )
{
    if ( engine_type == ENGINE_BITTREX )
        reinterpret_cast<TrexREST*>( rest_arr.value( ENGINE_BITTREX ) )->sendBuySell( pos, quiet );
    else if ( engine_type == ENGINE_BINANCE )
        reinterpret_cast<BncREST*>( rest_arr.value( ENGINE_BINANCE ) )->sendBuySell( pos, quiet );
    else if ( engine_type == ENGINE_POLONIEX )
        reinterpret_cast<PoloREST*>( rest_arr.value( ENGINE_POLONIEX ) )->sendBuySell( pos, quiet );
    else if ( engine_type == ENGINE_WAVES )
        reinterpret_cast<WavesREST*>( rest_arr.value( ENGINE_WAVES ) )->sendBuySell( pos, quiet );
}

void Engine::sendCancel( const QString &order_number, Position * const &pos, const Market &market )
{
    if ( engine_type == ENGINE_BITTREX )
        reinterpret_cast<TrexREST*>( rest_arr.value( ENGINE_BITTREX ) )->sendCancel( order_number, pos );
    else if ( engine_type == ENGINE_BINANCE )
        reinterpret_cast<BncREST*>( rest_arr.value( ENGINE_BINANCE ) )->sendCancel( order_number, pos );
    else if ( engine_type == ENGINE_POLONIEX )
        reinterpret_cast<PoloREST*>( rest_arr.value( ENGINE_POLONIEX ) )->sendCancel( order_number, pos );
    else if ( engine_type == ENGINE_WAVES )
        reinterpret_cast<WavesREST*>( rest_arr.value( ENGINE_WAVES ) )->sendCancel( order_number, pos, market );
}

bool Engine::yieldToFlowControl()
{
    return rest_arr.value( engine_type ) != nullptr ? rest_arr.value( engine_type )->yieldToFlowControl() :
                                                      false;
}

void Engine::onEngineMaintenance()
{
    checkMaintenance(); // do maintenance routine
    cleanGraceTimes(); // cleanup stray order ids
}

bool Engine::tryMoveOrder( Position* const &pos )
{
    // pos must be valid!

    const QString &market = pos->market;
    MarketInfo &info = market_info[ market ];
    Coin &hi_buy = info.spread.bid;
    Coin &lo_sell = info.spread.ask;

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

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // look for timed out requests
    for ( QSet<Position*>::const_iterator i = positions->queued().begin(); i != positions->queued().end(); i++ )
    {
        // flow control
        if ( yieldToFlowControl() )
            return;

        Position *const &pos = *i;

        // make sure the order hasn't been set and the request is stale
        if ( pos->order_set_time == 0 &&
             pos->order_request_time > 0 &&
             pos->order_request_time + settings->order_timeout < current_time )
        {
            kDebug() << "order timeout detected, resending" << pos->stringifyOrder();

            sendBuySell( pos );
        }
    }

    // look for timed out things
    const QSet<Position*>::const_iterator begin = positions->active().begin(),
                                          end = positions->active().end();
    for ( QSet<Position*>::const_iterator j = begin; j != end; j++ )
    {
        // flow control
        if ( yieldToFlowControl() )
            return;

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
        if (  pos->is_slippage &&
             !pos->is_cancelling &&
              pos->order_set_time > 0 &&
              pos->order_set_time < current_time - market_info[ pos->market ].slippage_timeout )
        {
            // reconcile slippage price according to spread hi/lo
            if ( tryMoveOrder( pos ) )
            {
                // we found a better price, mark resetting and cancel
                positions->cancel( pos, false, CANCELLING_FOR_SLIPPAGE_RESET );
            }
            else
            {
                // don't check it until new timeout occurs
                pos->order_set_time = current_time - settings->safety_delay_time;
            }
        }

        // search for one-time order with age > max_age_minutes
        if ( !pos->is_cancelling &&
              pos->order_set_time > 0 &&
              pos->max_age_epoch > 0 &&
              current_time >= pos->max_age_epoch )
        {
            // the order has reached max age
            positions->cancel( pos, false, CANCELLING_FOR_MAX_AGE );
        }
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

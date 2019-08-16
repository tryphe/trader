#include "position.h"
#include "trexrest.h"
#include "engine.h"
#include "enginesettings.h"
#include "bncrest.h"
#include "polorest.h"
#include "trexrest.h"
#include "global.h"

#include <algorithm>
#include <QDebug>
#include <QJsonArray>

Position::Position( QString _market, quint8 _side, QString _buy_price, QString _sell_price,
                    QString _order_size, QString _strategy_tag, QVector<qint32> _market_indices,
                    bool _landmark, Engine *_engine )
{
    // local info
    engine = _engine;
    market = _market;
    side = _side;
    cancel_reason = 0;
    order_set_time = 0;
    order_request_time = 0;
    order_cancel_time = 0;
    order_getorder_time = 0;
    market_indices = _market_indices;
    is_cancelling = false;
    is_landmark = _landmark;
    is_slippage = false;
    is_new_hilo_order = false;
    price_reset_count = 0;
    max_age_minutes = 0;
    strategy_tag = _strategy_tag;

    if ( engine && is_landmark && market_indices.size() > 1 )
    {
        // ordersize stuff
        QMap<int, Coin> ordersize_weights;
        Coin ordersize_weight_total, ordersize_amount_total;
        Coin lo_ordersize = CoinAmount::A_LOT;

        // track weight totals for lo/hi price
        Coin hi_price_weight_total, lo_price_weight_total;

        // measure the lowest amount supplied
        int i;
        for ( i = 0; i < market_indices.size(); i++ )
        {
            const PositionData &data = engine->getMarketInfo( market ).position_index[ market_indices.value( i ) ];
            Coin ordersize = data.order_size;

            // measure lowest order size
            if ( ordersize < lo_ordersize )
                lo_ordersize = ordersize;

            // add to ordersize total
            ordersize_amount_total += ordersize;
        }

        // use the lowest amount as weight 1 and size others correspondingly
        for ( i = 0; i < market_indices.size(); i++ )
        {
            const PositionData &data = engine->getMarketInfo( market ).position_index[ market_indices.value( i ) ];

            Coin current_weight;
            if ( lo_ordersize < CoinAmount::A_LOT )
                current_weight = Coin( data.order_size ) / lo_ordersize;

            //kDebug() << "idx:" << i << "price:" << data.order_size;

            // calculate ordersize weight
            ordersize_weight_total += current_weight;
            ordersize_weights.insert( i, current_weight );

            // add to price weight totals
            hi_price_weight_total += Coin( data.sell_price ) * current_weight;
            lo_price_weight_total += Coin( data.buy_price ) * current_weight;
        }

        //kDebug() << "hi_price_weight_total:" << hi_price_weight_total;

//        kDebug() << "indices" << market_indices;
//        kDebug() << "landmark size:" << original_size;
//        kDebug() << "landmark price:" << buy_price;
//        kDebug() << "landmark weights:" << ordersize_weights;

#if defined(EXCHANGE_BINANCE)
        // we want to "round" up or down each landmark order, in order to preserve our weighted profit ratio
        const Coin &shim = !engine ? CoinAmount::ORDER_SHIM : engine->getMarketInfo( market ).price_ticksize.ratio( 0.5 );
#else
        // default 0.5 satoshi for other exchanges
        const Coin &shim = CoinAmount::ORDER_SHIM;
#endif
        // apply the weighted price, rounded to the nearest half-satoshi
        buy_price = ( lo_price_weight_total / ordersize_weight_total ) - shim;
        sell_price = ( hi_price_weight_total / ordersize_weight_total ) + shim;
        original_size = ordersize_amount_total;

        // catch bad buy price due to shim
        if ( CoinAmount::SATOSHI > buy_price )
            buy_price = CoinAmount::SATOSHI;
    }
    else
    {
        // convert to coin for satoshi formatting
        buy_price = Coin( _buy_price );
        sell_price = Coin( _sell_price );
        original_size = Coin( _order_size );
    }

    // truncate order by exchange tick size
#if defined(EXCHANGE_BINANCE)
    const Coin &ticksize = !engine ? CoinAmount::SATOSHI : engine->getMarketInfo( market ).price_ticksize;
    buy_price.truncateByTicksize( ticksize );
    sell_price.truncateByTicksize( ticksize );

    // prevent buy_price from being less than ticksize
    if ( !ticksize.isZeroOrLess() && buy_price < ticksize )
        buy_price = ticksize;
#endif

    // set original prices, so that if we set slippage, we can go back towards these prices
    buy_price_original = buy_price;
    sell_price_original = sell_price;

    // sort the indices so we can print a nicer looking string
    std::sort( market_indices.begin(), market_indices.end() );

    // setup indices_str
    for ( QVector<qint32>::const_iterator i = market_indices.begin(); i != market_indices.end(); i++ )
        indices_str += QString::number( *i ) + ' ';

    // get rid of trailing space
    if ( indices_str.size() > 0 )
    {
        indices_str.prepend( "i " );
        indices_str.truncate( indices_str.size() -1 );
    }

    applyOffset();
}

Position::~Position()
{
}

void Position::calculateQuantity()
{
    // q = btc / price;
    Coin p = Coin( price );
    quantity = btc_amount / p;

    // polo doesn't do this... do it anyways
#if defined(EXCHANGE_BINANCE)
    const Coin &ticksize = !engine ? CoinAmount::SATOSHI : engine->getMarketInfo( market ).price_ticksize;

    quantity.truncateByTicksize( ticksize );
#endif
    btc_amount = quantity * p;


    //kDebug() << "quantity:" << quantity;
}

void Position::flip()
{
    // set position size to original size
    btc_amount = original_size;

    // buy->sell
    if ( side == SIDE_BUY )
    {
        side = SIDE_SELL;
        price = sell_price;
    }
    else // sell->buy
    {
        side = SIDE_BUY;
        price = buy_price;
    }

    // the object is deleted after this, no need to set/clear stuff
}

bool Position::applyPriceSide()
{
    // set our price
    if ( side == SIDE_SELL ) // sell hi
    {
        price = sell_price;
        return true;
    }
    else // buy lo
    {
        price = buy_price;
        return true;
    }

    kDebug() << "local error: invalid order 'side'" << stringifyOrder();
    return false;
}

void Position::applyOffset()
{
    qreal offset;
    bool sentiment;

    if ( engine )
    {
        MarketInfo &market_info = engine->getMarketInfo( market );
        offset = market_info.market_offset;
        sentiment = market_info.market_sentiment;
    }
    else // for tests
    {
        offset = 0.0;
        sentiment = true;
    }

    applyOffset( offset, sentiment );
}

void Position::applyOffset( qreal _offset, bool sentiment )
{
    bool is_buy = ( side == SIDE_BUY );

    // apply the offset in a way where hi_scalar / lo_scalar = 1 + fee_pct (ie. 1.0015)
    // for an offset value of 0.0015, lo_scalar = 0.99925, hi_scalar = 1.000748875
    const qreal offset = _offset / 2;
    const qreal lo_scalar = 1. - offset;
    const qreal hi_scalar = 1. + offset;

    btc_amount = original_size;
    // bullish sentiment on buy
    if ( sentiment && is_buy )
        btc_amount.applyRatio( hi_scalar );
    // bullish sentiment on sell
    else if ( sentiment && !is_buy )
        btc_amount.applyRatio( lo_scalar );
    // bearish sentiment on buy
    else if ( !sentiment && is_buy )
        btc_amount.applyRatio( lo_scalar );
    // bearish sentiment on sell
    else// if ( !sentiment && !is_buy )
        btc_amount.applyRatio( hi_scalar );

    // calculate lo/hi amounts
    Coin amount_lo_d;
    Coin amount_hi_d;

    if ( sentiment )
    {
        amount_lo_d = btc_amount * hi_scalar;
        amount_hi_d = btc_amount * lo_scalar;
    }
    else
    {
        amount_lo_d = btc_amount * lo_scalar;
        amount_hi_d = btc_amount * hi_scalar;
    }
    //

    const Coin fee = engine != nullptr ? engine->settings->fee : Coin();

    // calculate profit multipler while avoiding div0
    Coin profit_multiplier;
    if ( buy_price.isGreaterThanZero() )
        profit_multiplier = ( ( sell_price / buy_price ) - 1 ) / 2;

    // the average trade amount is the difference /2
    Coin avg_trade_amt = ( amount_hi_d + amount_lo_d ) / 2;
    Coin avg_fee = avg_trade_amt * fee;

    per_trade_profit = avg_trade_amt;
    per_trade_profit *= profit_multiplier;
    per_trade_profit -= avg_fee;

    if ( profit_multiplier.isGreaterThanZero() )
    {
        profit_margin = profit_multiplier;
        profit_margin -= avg_fee;
    }

    //kDebug() << "per_trade_profit_ratio:" << profit_margin.toAmountString();

    // if we fail to set the price, leave quantity blank so this order will get rejected
    if ( !applyPriceSide() ) // set price
        return;

    calculateQuantity(); // set quantity
}

void Position::jsonifyPositionFill( QJsonArray &arr )
{
    arr += "f";
    arr += order_number;
}

void Position::jsonifyPositionSet( QJsonArray &arr )
{
    arr += "s";
    arr += order_number;
    arr += market;
    arr += is_onetime;
    arr += is_landmark;
    arr += is_slippage;
    arr += side;
    arr += price;
    arr += original_size;
}

void Position::jsonifyPositionCancel( QJsonArray &arr )
{
    arr += "c";
    arr += order_number;
    //arr += cancel_reason;
}

//QJsonObject Position::jsonify()
//{ // unused
//    QJsonObject ret;

//    // order info
//    ret["price"] = price;
//    ret["size"] = btc_amount;
//    ret["side"] = side;

//    // local info
//    ret["buy_price"] = buy_price;
//    ret["sell_price"] = sell_price;

//    return ret;
//}

QString Position::stringifyOrder()
{
    const QString &order_number_str = Global::getOrderString( order_number );

    QString ret = QString( "%1%2  %3 %4 %5 @ %6               o %7 %8")
                .arg( is_landmark ? "L" : is_onetime ? "O" : " " )
                .arg( is_slippage ? "S" : " " )
                .arg( sideStr(), -4 )
                .arg( market, 8 )
                .arg( btc_amount, 11 )
                .arg( price, 10 )
                .arg( order_number_str, ORDER_STRING_SIZE )
                .arg( indices_str );

    return ret;
}

QString Position::stringifyOrderWithoutOrderID()
{
    QString ret = QString( "%1%2  %3 %4 %5 @ %6 %7")
                .arg( is_landmark ? "L" : is_onetime ? "O" : " " )
                .arg( is_slippage ? "S" : " " )
                .arg( sideStr(), -4 )
                .arg( market, 8 )
                .arg( btc_amount, 11 )
                .arg( price, 10 )
                .arg( indices_str );

    return ret;
}

QString Position::stringifyNewPosition()
{
    QString ret = QString( "%1%2  %3 %4 %5 @ %6 %7 %8")
                    .arg( is_landmark ? "L" : " " )
                    .arg( is_slippage ? "S" : " " )
                    .arg( sideStr(), -4 )
                    .arg( market, 8 )
                    .arg( btc_amount, 11 )
                    .arg( buy_price, 10 )
                    .arg( sell_price, -16 - ORDER_STRING_SIZE )
                    .arg( indices_str );

    return ret;
}

QString Position::stringifyPositionChange()
{
    const QString &order_number_str = Global::getOrderString( order_number );

    // make a string that looks like 0.00009999 -> 0.00120000
    //                               <price>       <next price>
    bool is_buy = ( side == SIDE_BUY );
    Coin &price = is_buy ? buy_price : sell_price;
    Coin &next_price = is_buy ? sell_price : buy_price;

    QString price_str = price;

    // don't display "next" price for onetime order
    if ( !is_onetime )
        price_str += QString( " -> %1" )
                     .arg( next_price );

    // there's an extra padded space here for the color magic to use it
    return QString( "%1%2  %3%4>>>none<<< %5 %6 @ %7 o %8 %9%10" )
            .arg( is_landmark ? "L" : " " )
            .arg( is_slippage ? "S" : " " )
            .arg( is_buy ? ">>>grn<<<" : ">>>red<<<" )
            .arg( sideStr(), -4 )
            .arg( market, 8 )
            .arg( btc_amount, 11 )
            .arg( price_str, -24 )
            .arg( order_number_str, ORDER_STRING_SIZE )
            .arg( indices_str )
            .arg( per_trade_profit.isGreaterThanZero() ? " p " + per_trade_profit : "" );
}

qint32 Position::getLowestMarketIndex() const
{
    // optimization for non-landmark orders
    if ( market_indices.size() == 1 ) return market_indices.value( 0 );

    qint32 idx_lo = std::numeric_limits<qint32>::max();

    for ( QVector<qint32>::const_iterator i = market_indices.begin(); i != market_indices.end(); i++ )
        if ( *i < idx_lo ) idx_lo = *i;

    return idx_lo;
}

qint32 Position::getHighestMarketIndex() const
{
    // optimization for non-landmark orders
    if ( market_indices.size() == 1 ) return market_indices.value( 0 );

    qint32 idx_hi = -1;

    for ( QVector<qint32>::const_iterator i = market_indices.begin(); i != market_indices.end(); i++ )
        if ( *i > idx_hi ) idx_hi = *i;

    return idx_hi;
}

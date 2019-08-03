#include "position.h"
#include "trexrest.h"
#include "engine.h"
#include "bncrest.h"
#include "polorest.h"
#include "trexrest.h"
#include "global.h"

#include <algorithm>
#include <QDebug>

Position::Position( QString _market, quint8 _side, QString _price_lo, QString _price_hi,
                    QString _order_size, QVector<qint32> _market_indices,
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
    is_invalidated = false;
    price_reset_count = 0;
    max_age_minutes = 0;

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
            hi_price_weight_total += Coin( Coin( data.price_hi ) * current_weight );
            lo_price_weight_total += Coin( Coin( data.price_lo ) * current_weight );
        }

        //kDebug() << "hi_price_weight_total:" << hi_price_weight_total;

//        kDebug() << "indices" << market_indices;
//        kDebug() << "landmark size:" << original_size;
//        kDebug() << "landmark price:" << price_lo;
//        kDebug() << "landmark weights:" << ordersize_weights;

#if defined(EXCHANGE_BINANCE)
        // we want to "round" up or down each landmark order, in order to preserve our weighted profit ratio
        const Coin shim = !engine ? CoinAmount::ORDER_SHIM : engine->getMarketInfo( market ).price_ticksize.toDouble() / 2.0;
#else
        // default 0.5 satoshi for other exchanges
        const Coin shim = CoinAmount::ORDER_SHIM;
#endif
        // apply the weighted price, rounded to the nearest half-satoshi
        price_lo = ( lo_price_weight_total / ordersize_weight_total ) - shim;
        price_hi = ( hi_price_weight_total / ordersize_weight_total ) + shim;
        original_size = ordersize_amount_total;

        // catch bad buy price due to shim
        if ( Coin( CoinAmount::SATOSHI ) > price_lo )
            price_lo = CoinAmount::SATOSHI;
    }
    else
    {
        price_lo = CoinAmount::toSatoshiFormatStr( _price_lo );
        price_hi = CoinAmount::toSatoshiFormatStr( _price_hi );
        original_size = CoinAmount::toSatoshiFormatExpr( _order_size.toDouble() );
    }

    // truncate order by exchange tick size
#if defined(EXCHANGE_BINANCE)
    Coin ticksize = !engine ? CoinAmount::SATOSHI : Coin( engine->getMarketInfo( market ).price_ticksize );
    price_lo.truncateValueByTicksize( ticksize );
    price_hi.truncateValueByTicksize( ticksize );

    // prevent price_lo from being less than ticksize
    if ( !ticksize.isZeroOrLess() && price_lo < ticksize )
        price_lo = ticksize;
#endif

    // set original prices, so that if we set slippage, we can go back towards these prices
    price_lo_original = price_lo;
    price_hi_original = price_hi;

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
    const QString &ticksize = !engine ? CoinAmount::SATOSHI_STR : engine->getMarketInfo( market ).price_ticksize;

    quantity.truncateValueByTicksize( ticksize );
#endif
    btc_amount = quantity * p;


    //kDebug() << "quantity:" << quantity;
}

void Position::flip()
{
    // set position size to original size
    btc_amount = original_size;

    if ( side == SIDE_BUY )
    {
        side = SIDE_SELL;
        price = price_hi;
    }
    else if ( side == SIDE_SELL )
    {
        side = SIDE_BUY;
        price = price_lo;
    }
    else
    {
        kDebug() << "local error: position flip failed!" << stringifyOrder();
        return;
    }

    // the object is deleted after this, no need to set/clear stuff
}

bool Position::applyPriceSide()
{
    // set our price
    if ( side == SIDE_SELL ) // sell hi
    {
        price = price_hi;
        return true;
    }
    else if ( side == SIDE_BUY ) // buy lo
    {
        price = price_lo;
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

    //btc_amount = btc_amount.toAmountString().toDouble();

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

    const Coin fee = engine ? engine->settings().fee : Coin();

    // calculate profit multipler while avoiding div0
    Coin profit_multiplier;
    if ( price_lo.isGreaterThanZero() )
        profit_multiplier = ( ( price_hi / price_lo ) - 1 ) / 2;

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

//QJsonObject Position::jsonify()
//{ // unused
//    QJsonObject ret;

//    // order info
//    ret["price"] = price;
//    ret["size"] = btc_amount;
//    ret["side"] = side;

//    // local info
//    ret["price_lo"] = price_lo;
//    ret["price_hi"] = price_hi;

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
                    .arg( price_lo, 10 )
                    .arg( price_hi, -16 - ORDER_STRING_SIZE )
                    .arg( indices_str );

    return ret;
}

QString Position::stringifyPositionChange()
{
    const QString &order_number_str = Global::getOrderString( order_number );

    // make a string that looks like 0.00009999 -> 0.00120000
    //                               <price>       <next price>
    bool is_buy = ( side == SIDE_BUY );
    Coin &price = is_buy ? price_lo : price_hi;
    Coin &next_price = is_buy ? price_hi : price_lo;

    QString price_str = price;

    if ( next_price.isGreaterThanZero() )
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

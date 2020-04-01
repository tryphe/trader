#include "spruceoverseer_test.h"
#include "engine.h"
#include "spruce.h"
#include "spruceoverseer.h"

#include <QDebug>

void SpruceOverseerTest::test( SpruceOverseer *o, Engine *engine )
{
    const QString TEST_MARKET = "TEST_1";

    /// imitate ticker
    engine->market_info[ TEST_MARKET ].ticker.bid = "0.00010000";
    engine->market_info[ TEST_MARKET ].ticker.ask = "0.00010100";

    // update ticker update time
    for ( quint8 type = 0; type < 4; type++ )
        if ( engine->rest_arr.value( type ) != nullptr )
            engine->rest_arr.value( type )->ticker_update_time = QDateTime::currentMSecsSinceEpoch();

    /// ensure that getSpreadLimit() ratio == regular spread ratio
    // override some settings
    const Coin order_random_buy = o->spruce->getOrderRandomBuy();
    const Coin order_random_sell = o->spruce->getOrderRandomSell();
    o->spruce->setOrderRandomBuy( Coin() );
    o->spruce->setOrderRandomSell( Coin() );

    const TickerInfo regular_spread = o->getSpreadLimit( TEST_MARKET, true );

    assert( regular_spread.bid / regular_spread.ask <= o->spruce->getOrderGreed() );

    // set randomness to 10%
    o->spruce->setOrderRandomBuy( Coin( "0.1" ) );
    o->spruce->setOrderRandomSell( Coin( "0.1" ) );

    const TickerInfo expanded_spread = o->getSpreadLimit( TEST_MARKET, true );

    // ensure completely expanded spread is <= the maximum ratio of the trailing limit
    assert( expanded_spread.bid / expanded_spread.ask <= o->spruce->getOrderTrailingLimit( SIDE_BUY ) );
    assert( expanded_spread.bid / expanded_spread.ask <= o->spruce->getOrderTrailingLimit( SIDE_SELL ) );

    // restore settings
    o->spruce->setOrderRandomBuy( order_random_buy );
    o->spruce->setOrderRandomSell( order_random_sell );

    /// ensure getSpreadForSide() ratio == regular spread ratio for buy and sell side
    const TickerInfo buy_spread = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false );
    const TickerInfo sell_spread = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false );

    assert( buy_spread.bid / buy_spread.ask <= o->spruce->getOrderGreed() );
    assert( sell_spread.bid / sell_spread.ask <= o->spruce->getOrderGreed() );

    /// ensure getSpreadForSide() random prices are in bounds of getSpreadLimit()
    const TickerInfo buy_spread_random = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false, true, true );
    const TickerInfo sell_spread_random = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false, true, true );

    assert( buy_spread_random.bid >= expanded_spread.bid &&
            buy_spread_random.ask <= expanded_spread.ask );

    assert( sell_spread_random.bid >= expanded_spread.bid &&
            sell_spread_random.ask <= expanded_spread.ask );

    /// ensure getSpreadForSide() contracted prices are in closer together than getSpreadLimit()
    const TickerInfo buy_spread_contracted = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false, false, false, Coin( "0.01" ) );
    const TickerInfo sell_spread_contracted = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false, false, false, Coin( "0.01" ) );

    assert( buy_spread_contracted.bid >= expanded_spread.bid );
    assert( sell_spread_contracted.ask <= expanded_spread.ask );

    /// ensure getSpreadLimit() with duplicity disabled gives us the same prices
    const TickerInfo midspread_test = o->getSpreadLimit( TEST_MARKET, false );
    assert( midspread_test.bid == midspread_test.ask );

    /// ensure getSpreadLimit() with taker enabled gives us crossed prices

    const TickerInfo taker_spread_buy = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, true );
    const TickerInfo taker_spread_sell = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, true );

    // ensure crossed prices match expanded spread
    assert( regular_spread.bid == taker_spread_buy.ask &&
            regular_spread.ask == taker_spread_buy.bid ) ;

    // ensure taker spread has bid > ask
    assert( taker_spread_buy.bid > taker_spread_buy.ask );

    // ensure mid spreads are equal for the same spread expansion
    assert( taker_spread_buy.bid == taker_spread_sell.bid &&
            taker_spread_sell.ask == taker_spread_buy.ask );

    // ensure inverse of base taker spread ratio matches base greed
    assert( taker_spread_buy.ask / taker_spread_buy.bid <= o->spruce->getOrderGreed() );

    const TickerInfo taker_spread_buy_rand = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, true, true );
    const TickerInfo taker_spread_sell_rand = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, true, true );

    assert( taker_spread_buy_rand.bid > taker_spread_buy_rand.ask );
    assert( taker_spread_sell_rand.bid > taker_spread_sell_rand.ask );

    /// test spread limit
    const TickerInfo spreadlimit = o->getSpreadLimit( TEST_MARKET, true );

    // ensure spread limit is better than our ticker prices
    assert( spreadlimit.bid < engine->market_info[ TEST_MARKET ].ticker.bid &&
            spreadlimit.ask > engine->market_info[ TEST_MARKET ].ticker.ask );

//    kDebug() << "      spread limit:" << o->getSpreadLimit( TEST_MARKET, true, false );
//    kDebug() << "        buy spread:" << o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false );
//    kDebug() << " buy spread random:" << o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false, true, true );
//    kDebug() << " buy spread reduce:" << o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false, false, false, Coin( "0.01" ) );
//    kDebug() << "       sell spread:" << o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false );
//    kDebug() << "sell spread random:" << o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false, true, true );
//    kDebug() << "sell spread reduce:" << o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false, false, false, Coin( "0.01" ) );

    // invalidate ticker update time
    for ( quint8 type = 0; type < 4; type++ )
        if ( engine->rest_arr.value( type ) != nullptr )
            engine->rest_arr.value( type )->ticker_update_time = 0;
}

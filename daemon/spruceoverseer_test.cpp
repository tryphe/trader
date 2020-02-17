#include "spruceoverseer_test.h"
#include "engine.h"
#include "spruce.h"
#include "spruceoverseer.h"

#include <QDebug>

void SpruceOverseerTest::test( SpruceOverseer *o, Engine *engine )
{
    const QString TEST_MARKET = "TEST_1";

    /// imitate ticker
    engine->market_info[ TEST_MARKET ].highest_buy = "0.00010000";
    engine->market_info[ TEST_MARKET ].lowest_sell = "0.00010100";

    /// ensure that getSpreadLimit() ratio == regular spread ratio
    // override some settings
    const Coin order_random_buy = o->spruce->getOrderRandomBuy();
    const Coin order_random_sell = o->spruce->getOrderRandomSell();
    o->spruce->setOrderRandomBuy( Coin() );
    o->spruce->setOrderRandomSell( Coin() );

    const TickerInfo regular_spread = o->getSpreadLimit( TEST_MARKET, true );

    assert( regular_spread.bid_price / regular_spread.ask_price == o->spruce->getOrderGreed() );

    // set randomness to 10%
    o->spruce->setOrderRandomBuy( Coin( "0.1" ) );
    o->spruce->setOrderRandomSell( Coin( "0.1" ) );

    const TickerInfo expanded_spread = o->getSpreadLimit( TEST_MARKET, true );

    // ensure completely expanded spread is 0.95 * 0.8 == 76 (note: 0.8 == 100%-20%, the random spread vars)
    // why isn't it exactly 0.76? not sure...
    assert( ( expanded_spread.bid_price / expanded_spread.ask_price ).toAmountString() == Coin( "0.76047479" ) );

    // restore settings
    o->spruce->setOrderRandomBuy( order_random_buy );
    o->spruce->setOrderRandomSell( order_random_sell );

    /// ensure getSpreadForSide() ratio == regular spread ratio for buy and sell side
    const TickerInfo buy_spread = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false );
    const TickerInfo sell_spread = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false );

    assert( buy_spread.bid_price / buy_spread.ask_price == o->spruce->getOrderGreed() );
    assert( sell_spread.bid_price / sell_spread.ask_price == o->spruce->getOrderGreed() );

    /// ensure getSpreadForSide() random prices are in bounds of getSpreadLimit()
    const TickerInfo buy_spread_random = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false, true, true );
    const TickerInfo sell_spread_random = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false, true, true );

    assert( buy_spread_random.bid_price >= expanded_spread.bid_price &&
            buy_spread_random.ask_price <= expanded_spread.ask_price );

    assert( sell_spread_random.bid_price >= expanded_spread.bid_price &&
            sell_spread_random.ask_price <= expanded_spread.ask_price );

    /// ensure getSpreadForSide() contracted prices are in closer together than getSpreadLimit()
    const TickerInfo buy_spread_contracted = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, false, false, false, Coin( "0.01" ) );
    const TickerInfo sell_spread_contracted = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, false, false, false, Coin( "0.01" ) );

    assert( buy_spread_contracted.bid_price > expanded_spread.bid_price );
    assert( sell_spread_contracted.ask_price < expanded_spread.ask_price );

    /// ensure getSpreadLimit() with duplicity disabled gives us the same prices
    const TickerInfo midspread_test = o->getSpreadLimit( TEST_MARKET, false );
    assert( midspread_test.bid_price == midspread_test.ask_price );
    //kDebug() << midspread_test.bid_price;

    /// ensure getSpreadLimit() with taker enabled gives us crossed prices
    const TickerInfo taker_spreadlimit = o->getSpreadLimit( TEST_MARKET, true );
    const TickerInfo taker_spread_buy = o->getSpreadForSide( TEST_MARKET, SIDE_BUY, true, true );
    const TickerInfo taker_spread_sell = o->getSpreadForSide( TEST_MARKET, SIDE_SELL, true, true );
    kDebug() << taker_spreadlimit.bid_price << taker_spreadlimit.ask_price;
    kDebug() << taker_spread_buy.bid_price << taker_spread_buy.ask_price;
    kDebug() << taker_spread_sell.bid_price << taker_spread_sell.ask_price;

    // ensure crossed prices match expanded spread
    assert( regular_spread.bid_price == taker_spread_buy.ask_price &&
            regular_spread.ask_price == taker_spread_buy.bid_price );

    // ensure taker spread has bid > ask
    assert( taker_spread_buy.bid_price > taker_spread_buy.ask_price );

    // ensure mid spreads are equal for the same spread expansion
    assert( taker_spread_buy.bid_price == taker_spread_buy.bid_price &&
            taker_spread_sell.ask_price == taker_spread_sell.ask_price );

    // ensure inverse of base taker spread ratio matches base greed
    assert( taker_spread_buy.ask_price / taker_spread_buy.bid_price == o->spruce->getOrderGreed() );

}

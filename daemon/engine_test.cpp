#include "engine_test.h"
#include "engine.h"
#include "global.h"
#include "position.h"
#include "positionman.h"
#include "coinamount.h"
#include "stats.h"

#include <algorithm>
#include <assert.h>
#include <QDebug>
#include <QVector>


void EngineTest::test( Engine *e )
{
    const QString TEST_MARKET = "TEST_1";

    // Market::operator QString()
    assert( TEST_MARKET == Market( TEST_MARKET ) ); // test default format
    assert( TEST_MARKET == Market( "TEST-1" ) ); // test alternate format
    assert( Market( "test_" ).operator QString().isEmpty() ); // test empty quote currency
    assert( Market( "_test" ).operator QString().isEmpty() ); // test empty base currency

    // make sure we are ready to start
    assert( e->getRest() != nullptr );
    assert( e->getStats() != nullptr );

    // disable verbosity during testing
    const int preserved_verbosity = e->verbosity;
    e->setVerbosity( 0 );
    e->setTesting( true );

    // test sorting for diverge/converge
    QVector<qint32> indices = QVector<qint32>() << 3 << 1 << 5 << 2 << 4;
    std::sort( indices.begin(), indices.end() );
    assert( indices == QVector<qint32>() << 1 << 2 << 3 << 4 << 5 );

    // test reverse sorting
    indices.insert( 2, 6 );
    std::sort( indices.rbegin(), indices.rend() );
    assert( indices == QVector<qint32>() << 6 << 5 << 4 << 3 << 2 << 1 );

    Position p = Position( TEST_MARKET, SIDE_BUY, "0.00001000", "0.00009000", "0.1" );
    assert( p.market == TEST_MARKET );
    assert( p.side == SIDE_BUY );
    assert( p.sideStr() == BUY );
    assert( p.is_landmark == false );
    assert( p.btc_amount == "0.10000000" );
    assert( p.original_size == "0.10000000" );
    assert( p.buy_price == "0.00001000" );
    assert( p.sell_price == "0.00009000" );

    // theoretical profit per trade, if we execute both sides
    // (((9 / 1) - 1) * 0.1) / 2 = 0.4
    assert( p.per_trade_profit == "0.40000000" );

    p.applyOffset( 0.0025, false );
    assert( p.quantity == "9987.50000000" );

    p.applyOffset( 0.0025, true );
    assert( p.quantity == "10012.50000000" );

    p.flip();
    assert( p.side == SIDE_SELL );
    assert( p.price == p.sell_price );

    p.applyOffset( 0.02, false );
    assert( p.quantity == "1122.22222222" );

    p.applyOffset( 0.02, true );
    assert( p.quantity == "1100.00000000" );

    Position p2 = Position( TEST_MARKET, SIDE_BUY, "0.00001777", "0.00009999", "0.07777777" );
    assert( p2.market == TEST_MARKET );
    assert( p2.side == SIDE_BUY );
    assert( p2.sideStr() == BUY );
    assert( p2.is_landmark == false );
    assert( p2.btc_amount == "0.07777776" );
    assert( p2.original_size == "0.07777777" );
    assert( p2.quantity == "4376.91446257" );
    assert( p2.buy_price == "0.00001777" );
    assert( p2.sell_price == "0.00009999" );

    // per trade profit
    // avg trade amt = 0.07777777
    // profit mul = ((0.00009999 / 0.00001777) -1) / 2 = 2.3134496342149690489589195272932
    // avg trade amt * profit mul = 0.17993495355655599324704558244232
    assert( p2.per_trade_profit == "0.17993495" );
    assert( p2.profit_margin == "2.31344963" );

    // test landmark position using the engine
    QVector<PositionData> &test_index = e->getMarketInfo( TEST_MARKET ).position_index;
    test_index += PositionData( "0.00000005", "0.00000050", "0.01", QLatin1String() ); // idx 0
    test_index += PositionData( "0.00000005", "0.00000060", "0.02", QLatin1String() ); // idx 1
    test_index += PositionData( "0.00000005", "0.00000070", "0.03", QLatin1String() ); // idx 2
    QVector<qint32> landmark_indices = QVector<qint32>() << 0 << 1 << 2;
    Position p3 = Position( TEST_MARKET, SIDE_SELL, "0.00000001", "0.00000002", "1.0", "", landmark_indices, true, e );

    // weight total = (50 * 0.01) + (60 * 0.02) + (70 * 0.03) = 3.8
    // size total =   0.01 + 0.02 + 0.03 = 0.06
    //
    // hi price = weight total / size total = 3.8 / 0.06 = 63 + shim
    assert( p3.market == TEST_MARKET );
    assert( p3.side == SIDE_SELL );
    assert( p3.sideStr() == SELL );
    assert( p3.is_landmark == true );
    assert( p3.market_indices == landmark_indices );
    assert( p3.btc_amount == "0.05999999" );
    assert( p3.original_size == "0.06000000" );
    assert( p3.quantity == "95238.09523809" );
    assert( p3.buy_price == "0.00000004" );
    assert( p3.sell_price == "0.00000063" );

    // insert ticker price to pass sanity check
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000001";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000100";

    // test addPosition()
    assert( e->positions->all().size() == 0 );
    Position *p4 = e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000000", "0.00000010", "0.02000000", "onetime" );
    assert( p4 != nullptr );
    assert( p4->btc_amount == "0.02000000" );
    assert( p4->quantity == "200000.00000000" );

    // Engine::deletePosition
    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );

    // test non-zero landmark buy price because of shim
    QVector<PositionData> &test_index_1 = e->getMarketInfo( TEST_MARKET ).position_index;
    test_index_1 += PositionData( "0.00000001", "0.00000050", "0.01", QLatin1String() ); // idx 0
    test_index_1 += PositionData( "0.00000001", "0.00000060", "0.02", QLatin1String() ); // idx 1
    test_index_1 += PositionData( "0.00000001", "0.00000070", "0.03", QLatin1String() ); // idx 2
    landmark_indices = QVector<qint32>() << 0 << 1 << 2;
    Position *p5 = e->addPosition( TEST_MARKET, SIDE_BUY, "0.00000001", "0.00000002", "0.00000000", ACTIVE, "test-strat", landmark_indices, true );
    assert( p5 != nullptr );
    assert( p5->btc_amount == "0.06000000" );
    assert( p5->quantity == "6000000.00000000" );
    assert( p5->buy_price == "0.00000001" );
    assert( p5->sell_price == "0.00000063" );
    assert( p5->strategy_tag == "test-strat" );

    // test getBuyTotal/getSellTotal
    assert( e->positions->getBuyTotal( TEST_MARKET ) == 1 );
    assert( e->positions->getSellTotal( TEST_MARKET ) == 0 );

    // cancel positions and clear mappings
    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );

    /// run basic ping-pong test for sell price equal to ticker ask price
    ///
    /// if bid|asks are at 83|84, our buy fill at 83 goes to 84
    QVector<Position*> pp;
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000083";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000084";

    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000083", "0.00000084", "0.1", ACTIVE ); // 0

    // simulate fills
    e->processFilledOrders( pp, FILL_WSS );

    // orders are filled, revamp list
    pp = e->positions->all().values().toVector();

    assert( pp.value( 0 )->price == "0.00000084" );

    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );
    ///

    /// run ticker slippage test for buy price colliding with asks
    ///
    /// if asks are at 100, our bid at 105 goes to 99
    pp.clear();
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000095";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000100";

    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000105", "0.00000200", "0.1", ACTIVE ); // 0

    assert( pp.value( 0 )->price == "0.00000099" );

    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );
    ///

    /// run ticker slippage test for sell price colliding with bids
    ///
    /// if bids are at 50, our ask at 45 goes to 51
    pp.clear();
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000050";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000055";

    pp += e->addPosition( TEST_MARKET, SIDE_SELL,  "0.00000030", "0.00000045", "0.1", ACTIVE ); // 0

    assert( pp.value( 0 )->price == "0.00000051" );

    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );
    ///

    /// run ping-pong bulk fill test
    ///
    ///   BUYS  |  SELLS
    ///   1 2 3 | 5 6 7 8
    ///        \_/
    ///
    /// 4 4 4 4 | 5 5 5
    ///
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000004";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000005";
    pp.clear();
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000001", "0.00000002", "0.1", ACTIVE ); // 0
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000002", "0.00000003", "0.1", ACTIVE ); // 1
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000003", "0.00000004", "0.1", ACTIVE ); // 2
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000004", "0.00000005", "0.1", ACTIVE ); // 3
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000005", "0.00000006", "0.1", ACTIVE ); // 4
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000006", "0.00000007", "0.1", ACTIVE ); // 5
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000007", "0.00000008", "0.1", ACTIVE ); // 6

    // simulate fills
    e->processFilledOrders( pp, FILL_WSS );

    QMap<QString,qint32> price_count;
    for ( QSet<Position*>::const_iterator i = e->positions->all().begin(); i != e->positions->all().end(); i++ )
        price_count[ (*i)->price ]++;

    assert( price_count[ QLatin1String( "0.00000004" ) ] == 4 ); // 4 buys at 4
    assert( price_count[ QLatin1String( "0.00000005" ) ] == 3 ); // 3 sells at 5

    assert( e->positions->all().size() == 7 );
    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );
    ///

    /// run "criss-cross" spread test
    /// suppose our spread simultaneously fills at 55|60 and we set 55->57 and 60->58, ->58 slips to 56(57-1)
    /// we should get 56|57 and not fill our own orders
    ///
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000055";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000060";
    pp.clear();
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000055", "0.00000057", "0.1", ACTIVE ); // 0
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000058", "0.00000060", "0.1", ACTIVE ); // 2

    // simulate fills
    e->processFilledOrders( pp, FILL_WSS );

    price_count.clear();
    for ( QSet<Position*>::const_iterator i = e->positions->all().begin(); i != e->positions->all().end(); i++ )
        price_count[ (*i)->price ]++;

    assert( price_count[ QLatin1String( "0.00000056" ) ] == 1 ); // buy at 56
    assert( price_count[ QLatin1String( "0.00000057" ) ] == 1 ); // sell at 57

    assert( e->positions->all().size() == 2 );
    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );
    ///

    /// run "non-chalant spread fill" spread test
    /// rather than converging to a point, sort our pre-fill by a more optimal value: the average of flipped prices
    ///
    ///   BUYS   |  SELLS
    ///   1 2 3  |  14 15 16 17
    ///         \_/
    ///
    ///  7 8 8 8 | 9 9 9
    ///
    /// hi/lo sort method would converge at 6|7 which would be suboptimal, 8|9 is much better
    ///
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000004";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000010";
    pp.clear();
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000001", "0.00000007", "0.1", ACTIVE ); // 0
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000002", "0.00000008", "0.1", ACTIVE ); // 1
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000003", "0.00000009", "0.1", ACTIVE ); // 2
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000007", "0.00000014", "0.1", ACTIVE ); // 3
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000008", "0.00000015", "0.1", ACTIVE ); // 4
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000009", "0.00000016", "0.1", ACTIVE ); // 5
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000010", "0.00000017", "0.1", ACTIVE ); // 6

    // simulate fills
    e->processFilledOrders( pp, FILL_WSS );

    // spread at 8|9
    price_count.clear();
    for ( QSet<Position*>::const_iterator i = e->positions->all().begin(); i != e->positions->all().end(); i++ )
        price_count[ (*i)->price ]++;

    assert( price_count[ QLatin1String( "0.00000007" ) ] == 1 ); // 1 buy at 7
    assert( price_count[ QLatin1String( "0.00000008" ) ] == 3 ); // 3 buys at 8
    assert( price_count[ QLatin1String( "0.00000009" ) ] == 3 ); // 3 sells at 9

    assert( e->positions->all().size() == 7 );
    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );

    /// test diverge/converge
    ///
    e->market_info[ TEST_MARKET ].order_dc = 5;
    e->market_info[ TEST_MARKET ].order_dc_nice = 0;
    e->market_info[ TEST_MARKET ].order_landmark_start = 2;
    e->market_info[ TEST_MARKET ].order_landmark_thresh = 2;
    e->market_info[ TEST_MARKET ].order_max = 40;
    e->market_info[ TEST_MARKET ].order_min = 15;
    e->market_info[ TEST_MARKET ].highest_buy = "0.00000023";
    e->market_info[ TEST_MARKET ].lowest_sell = "0.00000033";
    pp.clear();
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000006", "0.00000015", "0.1", ACTIVE ); // 0
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000007", "0.00000016", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000008", "0.00000017", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000009", "0.00000018", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000010", "0.00000019", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000011", "0.00000020", "0.1", ACTIVE ); // 5
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000012", "0.00000021", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000013", "0.00000022", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000014", "0.00000023", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000015", "0.00000024", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000016", "0.00000025", "0.1", ACTIVE ); // 10
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000017", "0.00000026", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000018", "0.00000027", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000019", "0.00000028", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000020", "0.00000029", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000021", "0.00000030", "0.1", ACTIVE ); // 15
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000022", "0.00000031", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_BUY,  "0.00000023", "0.00000032", "0.1", ACTIVE );

    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000024", "0.00000033", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000025", "0.00000034", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000026", "0.00000035", "0.1", ACTIVE ); // 20
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000027", "0.00000036", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000028", "0.00000037", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000029", "0.00000038", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000030", "0.00000039", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000031", "0.00000040", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000032", "0.00000041", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000033", "0.00000042", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000034", "0.00000043", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000035", "0.00000044", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000036", "0.00000045", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000037", "0.00000046", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000038", "0.00000047", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000039", "0.00000048", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000040", "0.00000049", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000041", "0.00000050", "0.1", ACTIVE );
    pp += e->addPosition( TEST_MARKET, SIDE_SELL, "0.00000042", "0.00000051", "0.1", ACTIVE );

    e->positions->divergeConverge();
    e->positions->divergeConverge();
    e->positions->divergeConverge();

    quint16 buy_count = 0, sell_count = 0;
    for ( QSet<Position*>::const_iterator i = e->positions->all().begin(); i != e->positions->all().end(); i++ )
        ( (*i)->side == SIDE_BUY ) ? buy_count++ : sell_count++;

    // there's 3 landmark orders of 5 orders each, on both sides, plus 3 buys and 4 sells
    assert( buy_count == 6 );
    assert( sell_count == 7 );

    QVector<Position*> filled = QVector<Position*>();
    filled += e->positions->getByIndex( TEST_MARKET, 18 );
    filled += e->positions->getByIndex( TEST_MARKET, 19 );
    filled += e->positions->getByIndex( TEST_MARKET, 20 );
    filled += e->positions->getByIndex( TEST_MARKET, 21 );

    e->processFilledOrders( filled, FILL_WSS ); // fill 4 sells

    buy_count = 0, sell_count = 0;
    for ( QSet<Position*>::const_iterator i = e->positions->all().begin(); i != e->positions->all().end(); i++ )
        ( (*i)->side == SIDE_BUY ) ? buy_count++ : sell_count++;

    // there's 3 landmark orders of 5 orders each, on both sides, plus 7 buys and 0 sells
    assert( buy_count == 10 );
    assert( sell_count == 3 );

    e->positions->divergeConverge(); // diverge landmark sell

    buy_count = 0, sell_count = 0;
    for ( QSet<Position*>::const_iterator i = e->positions->all().begin(); i != e->positions->all().end(); i++ )
        ( (*i)->side == SIDE_BUY ) ? buy_count++ : sell_count++;

    // there's 3 landmark buys and 2 landmark sells, plus 7 buys and 5 sells
    assert( buy_count == 10 );
    assert( sell_count == 7 );

    e->positions->cancelLocal();
    assert( e->positions->all().size() == 0 );
    ///

    // clear some stuff and disable test mode
    e->getMarketInfoStructure().clear(); // clear TEST_MARKET market from market settings
    e->positions->diverge_converge.clear(); // clear TEST_MARKET from dc market index
    e->positions->diverging_converging.clear(); // clear TEST_MARKET from dc market index
    e->cancelled_orders_for_polling.clear();

    // clear TEST_MARKET market stats
    e->stats->clearAll();

    // make sure the engine was cleared of our test positions and markets
    assert( e->positions->queued().size() == 0 );
    assert( e->getMarketInfoStructure().size() == 0 );
    assert( e->positions->getDCCount() == 0 );
    assert( e->positions->diverging_converging.size() == 0 );
    assert( e->cancelled_orders_for_polling.size() == 0 );

    // disable test mode
    e->setVerbosity( preserved_verbosity );
    e->setTesting( false );

    // enable wss interface flag here, since we need it disabled during tests anyways
#ifdef WSS_INTERFACE
    e->wss_interface = true;
#endif
}

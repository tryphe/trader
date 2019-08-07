#include "engine_test.h"
#include "engine.h"
#include "global.h"
#include "position.h"
#include "coinamount.h"
#include "stats.h"

#include <algorithm>
#include <assert.h>
#include <QDebug>
#include <QVector>


void EngineTest::test( Engine *e )
{
    e->setTesting( true );
    e->setVerbosity( 0 );

    // test sorting for diverge/converge
    QVector<qint32> indices = QVector<qint32>() << 3 << 1 << 5 << 2 << 4;
    std::sort( indices.begin(), indices.end() );
    assert( indices == QVector<qint32>() << 1 << 2 << 3 << 4 << 5 );

    // test reverse sorting
    indices.insert( 2, 6 );
    std::sort( indices.rbegin(), indices.rend() );
    assert( indices == QVector<qint32>() << 6 << 5 << 4 << 3 << 2 << 1 );

    Position p = Position( "TEST", SIDE_BUY, "0.00001000", "0.00009000", "0.1" );
    assert( p.market == "TEST" );
    assert( p.side == SIDE_BUY );
    assert( p.sideStr() == BUY );
    assert( p.is_landmark == false );
    assert( p.btc_amount == "0.10000000" );
    assert( p.original_size == "0.10000000" );
    assert( p.price_lo == "0.00001000" );
    assert( p.price_hi == "0.00009000" );

    // theoretical profit per trade, if we execute both sides
    // (((9 / 1) - 1) * 0.1) / 2 = 0.4
    assert( p.per_trade_profit == "0.40000000" );

    p.applyOffset( 0.0025, false );
    assert( p.quantity == "9987.50000000" );

    p.applyOffset( 0.0025, true );
    assert( p.quantity == "10012.50000000" );

    p.flip();
    assert( p.side == SIDE_SELL );
    assert( p.price == p.price_hi );

    p.applyOffset( 0.02, false );
    assert( p.quantity == "1122.22222222" );

    p.applyOffset( 0.02, true );
    assert( p.quantity == "1100.00000000" );

    Position p2 = Position( "TEST", SIDE_BUY, "0.00001777", "0.00009999", "0.07777777" );
    assert( p2.market == "TEST" );
    assert( p2.side == SIDE_BUY );
    assert( p2.sideStr() == BUY );
    assert( p2.is_landmark == false );
    assert( p2.btc_amount == "0.07777776" );
    assert( p2.original_size == "0.07777777" );
    assert( p2.quantity == "4376.91446257" );
    assert( p2.price_lo == "0.00001777" );
    assert( p2.price_hi == "0.00009999" );

    // per trade profit
    // avg trade amt = 0.07777777
    // profit mul = ((0.00009999 / 0.00001777) -1) / 2 = 2.3134496342149690489589195272932
    // avg trade amt * profit mul = 0.17993495355655599324704558244232
    assert( p2.per_trade_profit == "0.17993495" );
    assert( p2.profit_margin == "2.31344963" );

    // test landmark position using the engine
    QVector<PositionData> &test_index = e->getMarketInfo( "TEST" ).position_index;
    test_index += PositionData( "0.00000005", "0.00000050", "0.01", QLatin1String() ); // idx 0
    test_index += PositionData( "0.00000005", "0.00000060", "0.02", QLatin1String() ); // idx 1
    test_index += PositionData( "0.00000005", "0.00000070", "0.03", QLatin1String() ); // idx 2
    QVector<qint32> landmark_indices = QVector<qint32>() << 0 << 1 << 2;
    Position p3 = Position( "TEST", SIDE_SELL, "0.00000001", "0.00000002", "1.0", "", landmark_indices, true, e );

    // weight total = (50 * 0.01) + (60 * 0.02) + (70 * 0.03) = 3.8
    // size total =   0.01 + 0.02 + 0.03 = 0.06
    //
    // hi price = weight total / size total = 3.8 / 0.06 = 63 + shim
    assert( p3.market == "TEST" );
    assert( p3.side == SIDE_SELL );
    assert( p3.sideStr() == SELL );
    assert( p3.is_landmark == true );
    assert( p3.market_indices == landmark_indices );
    assert( p3.btc_amount == "0.05999999" );
    assert( p3.original_size == "0.06000000" );
    assert( p3.quantity == "95238.09523809" );
    assert( p3.price_lo == "0.00000004" );
    assert( p3.price_hi == "0.00000063" );

    // Engine::deletePosition
    e->cancelLocal( "TEST" );
    assert( e->positionsAll().size() == 0 );

    // test addPosition()
    Position *p4 = e->addPosition( "TEST", SIDE_SELL, "0.00000000", "0.00000010", "0.02000000", "onetime" );
    assert( p4 != nullptr );
    assert( p4->btc_amount == "0.02000000" );
    assert( p4->quantity == "200000.00000000" );

    // Engine::deletePosition
    e->cancelLocal( "TEST" );
    assert( e->positionsAll().size() == 0 );

    // test non-zero landmark buy price because of shim
    QVector<PositionData> &test_index_1 = e->getMarketInfo( "TEST" ).position_index;
    test_index_1 += PositionData( "0.00000001", "0.00000050", "0.01", QLatin1String() ); // idx 0
    test_index_1 += PositionData( "0.00000001", "0.00000060", "0.02", QLatin1String() ); // idx 1
    test_index_1 += PositionData( "0.00000001", "0.00000070", "0.03", QLatin1String() ); // idx 2
    landmark_indices = QVector<qint32>() << 0 << 1 << 2;
    Position *p5 = e->addPosition( "TEST", SIDE_BUY, "0.00000001", "0.00000002", "0.00000000", "active", "test-strat", landmark_indices, true );
    assert( p5 != nullptr );
    assert( p5->btc_amount == "0.06000000" );
    assert( p5->quantity == "6000000.00000000" );
    assert( p5->price_lo == "0.00000001" );
    assert( p5->price_hi == "0.00000063" );
    assert( p5->strategy_tag == "test-strat" );

    // test getBuyTotal/getSellTotal
    assert( e->getBuyTotal( "TEST" ) == 1 );
    assert( e->getSellTotal( "TEST" ) == 0 );

    // cancel positions and clear mappings
    e->cancelLocal();
    assert( e->positionsAll().size() == 0 );

    /// run ping-pong fill test
    ///
    ///   BUYS  |  SELLS
    ///   1 2 3 | 5 6 7 8
    ///        \_/
    ///
    /// 4 4 4 4 | 5 5 5
    ///
    e->market_info[ "TEST" ].highest_buy = "0.00000004";
    e->market_info[ "TEST" ].lowest_sell = "0.00000005";
    QVector<Position*> pp;
    pp += e->addPosition( "TEST", SIDE_BUY,  "0.00000001", "0.00000002", "0.1", "active" ); // 0
    pp += e->addPosition( "TEST", SIDE_BUY,  "0.00000002", "0.00000003", "0.1", "active" ); // 1
    pp += e->addPosition( "TEST", SIDE_BUY,  "0.00000003", "0.00000004", "0.1", "active" ); // 2
    pp += e->addPosition( "TEST", SIDE_SELL, "0.00000004", "0.00000005", "0.1", "active" ); // 3
    pp += e->addPosition( "TEST", SIDE_SELL, "0.00000005", "0.00000006", "0.1", "active" ); // 4
    pp += e->addPosition( "TEST", SIDE_SELL, "0.00000006", "0.00000007", "0.1", "active" ); // 5
    pp += e->addPosition( "TEST", SIDE_SELL, "0.00000007", "0.00000008", "0.1", "active" ); // 6

    // simulate fills
    e->processFilledOrders( pp, FILL_WSS );
    //e->stats->printOrdersByIndex( "TEST" );

//    kDebug() << "hi_buy: " << e->market_info[ "TEST" ].highest_buy;
//    kDebug() << "lo_sell:" << e->market_info[ "TEST" ].lowest_sell;

    // orders are filled, revamp list
    pp.clear();
    pp += e->positionsAll().values().toVector();

    for ( QSet<Position*>::const_iterator i = e->positionsAll().begin(); i != e->positionsAll().end(); i++ )
    {
        Position *pos = *i;
        if ( pos->side == SIDE_BUY )
            assert( pos->price == "0.00000004" );
        else
            assert( pos->price == "0.00000005" );
    }

    e->cancelLocal();
    assert( e->positionsAll().size() == 0 );
    ///

//    /// run post-fill spread adjustment test
//    /// bid ticker = 100
//    /// ask ticker = 110
//    ///
//    /// index 0 is a buy at 97
//    /// index 1 is a sell at 112
//    /// once index 0 fills, we know the exchange's bid is at most 97,   so index 1's buy  at 98 is now 97
//    /// once index 1 fills, we know the exchange's ask is at least 112, so index 0's sell at 111 is now 112
//    ///
//    /// even though our post-fill bounds are 98|111, we set them to
//    ///
//    e->market_info[ "TEST" ].highest_buy = "0.00000100";
//    e->market_info[ "TEST" ].lowest_sell = "0.00000110";
//    pp.clear();
//    pp += e->addPosition( "TEST", SIDE_BUY,  "0.00000097", "0.00000111", "0.1", "active" ); // 0
//    pp += e->addPosition( "TEST", SIDE_SELL, "0.00000098", "0.00000112", "0.1", "active" ); // 1

//    // simulate fills
//    e->processFilledOrders( pp, FILL_WSS );

//    kDebug() << "hi_buy: " << e->market_info[ "TEST" ].highest_buy;
//    kDebug() << "lo_sell:" << e->market_info[ "TEST" ].lowest_sell;

////    assert( e->market_info[ "TEST" ].highest_buy == "0.00000097" );
////    assert( e->market_info[ "TEST" ].lowest_sell == "0.00000112" );

//    e->cancelLocal();
//    ///

    // clear some stuff and disable test mode
    e->getMarketInfoStructure().clear(); // clear "TEST" market from market settings
    e->diverging_converging.clear(); // clear "TEST" from dc market index
    e->setTesting( false );
    e->setVerbosity( 1 );

    // make sure the engine was cleared of our test positions and markets
    assert( e->positions_queued.size() == 0 );
    assert( e->positionsAll().size() == 0 );
    assert( e->getMarketInfoStructure().size() == 0 );
    assert( e->diverge_converge.size() == 0 );
    assert( e->diverging_converging.size() == 0 );

    // make sure we are ready to start
    assert( e->getRest() != nullptr );
    assert( e->getStats() != nullptr );
}

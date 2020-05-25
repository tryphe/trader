#include "diffusionphaseman_test.h"
#include "diffusionphaseman.h"

#include <QString>
#include <QVector>
#include <QMap>

void DiffusionPhaseManTest::test()
{
    QVector<QString> markets;
    markets += "BTC_TEST0";
    markets += "BTC_TEST1";
    markets += "BTC_TEST2";
    markets += "BTC_TEST3";
    markets += "BTC_TEST4";

    DiffusionPhaseMan pm( markets );
    pm.addPhase( NO_FLUX );
    pm.addPhase( FLUX_PER_MARKET );

    int buy_count = 0;
    int sell_count = 0;
    int flux_count = 0;
    int noflux_count = 0;
    QMap<QString,int> flux_markets_accounted_for;

    for ( pm.begin(); !pm.atEnd(); pm.next() )
    {
        const quint8 side = pm.getCurrentPhaseSide();
        const bool is_flux_phase = ( pm.getCurrentPhase() == FLUX_PER_MARKET );

        // count iterations of different phase types
        if ( is_flux_phase )
            flux_count++;
        else
            noflux_count++;

        // count phases per size
        if ( side == SIDE_BUY )
            buy_count++;
        else
            sell_count++;

        // make sure the execution market matches the correct market count
        if ( is_flux_phase )
        {
            const QString &current_active_market = pm.getCurrentPhaseMarkets().value( 0 );
            flux_markets_accounted_for[ current_active_market ]++;

            assert( pm.getCurrentPhaseMarkets().size() == 1 ); // make sure it's a single market
            assert( ( side == SIDE_BUY  && flux_markets_accounted_for.value( current_active_market ) == 1 ) ||
                    ( side == SIDE_SELL && flux_markets_accounted_for.value( current_active_market ) == 2 ) ); // ensure market is fluxed once on each side
        }
        else
            assert( pm.getCurrentPhaseMarkets() == markets );
    }

    assert( flux_markets_accounted_for.size() == markets.size() ); // verify we fluxed as many markets as supplied
    assert( flux_count == markets.size() *2 ); // one buy/sell flux phase for each market
    assert( noflux_count == 2 ); // noflux always has 1 buy/sell phase
    assert( buy_count == sell_count );
    assert( buy_count == markets.size() +1 ); // buy/sell count == 1+1m, 1 for noflux phase and 1m for each market
}

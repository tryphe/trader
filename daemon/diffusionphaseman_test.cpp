#include "diffusionphaseman_test.h"
#include "diffusionphaseman.h"

#include <QString>
#include <QVector>

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

    for ( pm.begin(); !pm.atEnd(); pm.next() )
    {
        const quint8 side = pm.getCurrentPhaseSide();
        const bool is_flux_phase = ( pm.getCurrentPhase() == FLUX_PER_MARKET );

        if ( is_flux_phase )
            flux_count++;
        else
            noflux_count++;

        if ( side == SIDE_BUY )
            buy_count++;
        else
            sell_count++;
    }

    assert( flux_count == markets.size() *2 ); // one buy/sell flux phase for each market
    assert( noflux_count == 2 ); // noflux always has 1 buy/sell phase
    assert( buy_count == sell_count );
    assert( buy_count == markets.size() +1 ); // buy/sell count == 1+1m, 1 for noflux phase and 1m for each market
}

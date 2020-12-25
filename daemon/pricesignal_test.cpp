#include "pricesignal_test.h"
#include "pricesignal.h"

#include <QDebug>

void PriceSignalTest::test()
{
    /// test SMA with max samples = 0
    PriceSignal sma( SMA );
    sma.addSample( Coin("1.5") );
    sma.addSample( Coin("2.5") );
    sma.addSample( Coin("3.5") );
    sma.addSample( Coin("4.5") );
    sma.addSample( Coin("5.5") );
    assert( sma.getSignal() == Coin("3.5") );

    /// test SMA with max_samples = 10
    sma.setSignalArgs( SMA, 10 );
    sma.addSample( Coin("1") );
    sma.addSample( Coin("2") );
    sma.addSample( Coin("3") );

    assert( sma.getSignal() == Coin("2") );

    sma.addSample( Coin("4") );
    sma.addSample( Coin("5") );
    sma.addSample( Coin("6") );
    assert( sma.getSignal() == Coin("3.5") );

    sma.addSample( Coin("7") );
    sma.addSample( Coin("8") );
    sma.addSample( Coin("9") );
    sma.addSample( Coin("10") );
    sma.addSample( Coin("11") );
    assert( sma.getSignal() == Coin("6.5") ); // note: 2:11 / 10 == 6.5

    sma.addSample( Coin("12") );
    sma.addSample( Coin("13") );
    assert( sma.getSignal() == Coin("8.5") ); // note: 4:13 / 10 == 8.5
    assert( sma.getCurrentSamples() == 10 );
    assert( sma.getMaxSamples() == 10 );

    sma.clear();
    assert( sma.getCurrentSamples() == 0 );

    /// test WMA with samples 2,3
    PriceSignal wma( WMA, 3 );

    wma.addSample( CoinAmount::COIN *2 );
    wma.addSample( CoinAmount::COIN *3 );
    assert( wma.getSignal() == "2.66666666" ); // note ((2 * 1) + (3 * 2)) / 3 == 2.66

    /// test WMA with samples 2,3,4
    wma.clear();
    wma.addSample( CoinAmount::COIN *2 );
    wma.addSample( CoinAmount::COIN *3 );
    wma.addSample( CoinAmount::COIN *4 );
    assert( wma.getSignal() == "3.33333333" ); // note: ((2 * 1) + (3 * 2) + (4 * 3)) / 6 == 3.33

    /// test WMA with samples 3,4,5
    wma.addSample( CoinAmount::COIN *5 );
    assert( wma.getSignal() == "4.33333333" ); // note: ((3 * 1) + (4 * 2) + (5 * 3)) / 6 == 4.33

    /// test EMA with samples 2,3
    PriceSignal ema( EMA, 3 );

    ema.addSample( CoinAmount::COIN *2 );
    ema.addSample( CoinAmount::COIN *3 );
    assert( ema.getSignal() == "2.80000000" ); // note: ((2 * 1) + (3 * (2 * 2))) / 5 == 2.8

    /// test EMA with samples 2,3,4
    ema.addSample( CoinAmount::COIN *4 );
    assert( ema.getSignal() == "3.57142857" ); // note: ((2 * 1) + (3 * (2 * 2)) + (4 * (3 * 3))) / 14 == 3.57142857

    /// test RSI
    PriceSignal rsi( RSI, 14 );

    rsi.addSample( Coin("44.34" ) );
    rsi.addSample( Coin("44.09" ) );
    rsi.addSample( Coin("44.15" ) );
    rsi.addSample( Coin("43.61" ) );
    rsi.addSample( Coin("44.33" ) );
    rsi.addSample( Coin("44.83" ) );
    rsi.addSample( Coin("45.10" ) );
    rsi.addSample( Coin("45.42" ) );
    rsi.addSample( Coin("45.84" ) );
    rsi.addSample( Coin("46.08" ) );
    rsi.addSample( Coin("45.89" ) );
    rsi.addSample( Coin("46.03" ) );
    rsi.addSample( Coin("45.61" ) );
    rsi.addSample( Coin("46.28" ) );
    rsi.addSample( Coin("46.28" ) );
    assert( rsi.getSignal() == "70.46413502" );
    rsi.addSample( Coin("46.00" ) );
    assert( rsi.getSignal() == "66.24961855" );
    rsi.addSample( Coin("46.03" ) );
    assert( rsi.getSignal() == "66.48094183" );
    rsi.addSample( Coin("46.41" ) );
    assert( rsi.getSignal() == "69.34685316" );
    rsi.addSample( Coin("46.22" ) );
    assert( rsi.getSignal() == "66.29471265" );
    rsi.addSample( Coin("45.64" ) );
    assert( rsi.getSignal() == "57.91502067" );

    /// test SMARatio
    PriceSignal smar( SMARatio, 5, 10 );
    smar.addSample( CoinAmount::COIN *1 );
    smar.addSample( CoinAmount::COIN *2 );
    smar.addSample( CoinAmount::COIN *3 );
    smar.addSample( CoinAmount::COIN *4 );
    smar.addSample( CoinAmount::COIN *5 );
    smar.addSample( CoinAmount::COIN *6 );
    smar.addSample( CoinAmount::COIN *7 );
    smar.addSample( CoinAmount::COIN *8 );
    smar.addSample( CoinAmount::COIN *9 );
    smar.addSample( CoinAmount::COIN *10 );
    assert( smar.getSignal() == "1.45454545" ); // ( 6:10 / 5 ) / ( 1:10 / 10 )

    /// test WMARatio
    PriceSignal wmar( WMARatio, 3, 6 );
    wmar.addSample( CoinAmount::COIN *2 );
    wmar.addSample( CoinAmount::COIN *3 );
    wmar.addSample( CoinAmount::COIN *4 );
    wmar.addSample( CoinAmount::COIN *5 );
    wmar.addSample( CoinAmount::COIN *6 );
    wmar.addSample( CoinAmount::COIN *7 );
    assert( wmar.getSignal() == "1.18750000" ); // (((5 * 1) + (6 * 2) + (7 * 3)) / 6) / (((2 * 1) + (3 * 2) + (4 * 3) + (5 * 4) + (6 * 5) + (7 * 6)) / 21)

    /// test EMARatio
    PriceSignal emar( EMARatio, 2, 4 );
    emar.addSample( CoinAmount::COIN *2 );
    emar.addSample( CoinAmount::COIN *3 );
    emar.addSample( CoinAmount::COIN *4 );
    emar.addSample( CoinAmount::COIN *5 );
    assert( emar.getSignal() == "1.10769230" ); // (((4 * 1) + (5 * (2 * 2))) / 5) / (((2 * 1) + (3 * (2 * 2)) + (4 * (3 * 3)) + (5 * (4 * 4))) / 30)

    /// test RSIRatio
    PriceSignal rsir( RSIRatio, 7, 14 );
    rsir.addSample( Coin("44.34" ) );
    rsir.addSample( Coin("44.09" ) );
    rsir.addSample( Coin("44.15" ) );
    rsir.addSample( Coin("43.61" ) );
    rsir.addSample( Coin("44.33" ) );
    rsir.addSample( Coin("44.83" ) );
    rsir.addSample( Coin("45.10" ) );
    rsir.addSample( Coin("45.42" ) );
    rsir.addSample( Coin("45.84" ) );
    rsir.addSample( Coin("46.08" ) );
    rsir.addSample( Coin("45.89" ) );
    rsir.addSample( Coin("46.03" ) );
    rsir.addSample( Coin("45.61" ) );
    rsir.addSample( Coin("46.28" ) );
    rsir.addSample( Coin("46.28" ) );
    assert( rsir.getSignal() == "1.00858312" );

    rsir.addSample( Coin("46.00" ) );
    rsir.addSample( Coin("46.03" ) );
    rsir.addSample( Coin("46.41" ) );
    rsir.addSample( Coin("46.22" ) );
    rsir.addSample( Coin("45.64" ) );
    assert( rsir.getSignal() == "1.26056324" );

    // calculate harmonic mean
    PriceSignal hma( HMA, 3 );
    hma.addSample( Coin("4" ) );
    hma.addSample( Coin("4" ) );
    hma.addSample( Coin("1" ) );
    assert( hma.getSignal() == "2.00000000" );
}

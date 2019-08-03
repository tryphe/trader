#include "coinamount_test.h"
#include "global.h"
#include "coinamount.h"

#include <QtMath>


void CoinAmountTest::test()
{
    // floats suck #1
    // synopsis: double is okay to store bitcoin quantities, but not other altcoins with larger supplies, or
    //           coins with a different number of coin parts, or prices with a certain amount of decimals, or
    //           tick sizes.
    assert( (double) 2100000000000000.0 != (double) 2099999999999999.0 ); // bitcoin max satoshis != max -1
    assert( (double) 2251799813685248.0 == (double) 2251799813685247.9 ); // 2^51 == 2^51 - 0.1
    assert( (double) 100000000.00000010 == (double) 100000000.00000011 ); // however, an altcoin with 100m coins or large ticksize would lose precision

    // floats suck #2
    // synopsis: using comparators with float's internal half-rounding is bad
    double r = 0.00000001;
    while ( r < 0.00000010 ) // when we're at 10 satoshis, it's actually 9.99999999... so we iterate one over
        r += 0.00000001;
    assert( CoinAmount::toSatoshiFormat( r ) == "0.00000011" ); // r == 11 satoshis

    // coin is good #1
    // Coin::operator ==( const QString &s )
    // Coin::operator !=( const QString &s )
    assert( Coin( "100000000100000000100000000100000000100000000.00000010" ) == "100000000100000000100000000100000000100000000.00000010" );
    assert( Coin( "100000000100000000100000000100000000100000000.00000010" ) != "100000000100000000100000000100000000100000000.00000011" );

    // coin is good #2
    Coin s( CoinAmount::SATOSHI );
    while ( s < CoinAmount::SATOSHI * 10 )
        s += CoinAmount::SATOSHI;
    assert( s == "0.00000010" );

    // some type limits
    assert( Coin( std::numeric_limits<qreal>::lowest() ) == "-179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.00000000" );
    assert( Coin( std::numeric_limits<qreal>::epsilon() ).toSubSatoshiString() == "0.0000000000000002" );
    assert( Coin( std::numeric_limits<qreal>::min() ) == "0.00000000" );
    assert( Coin( std::numeric_limits<qreal>::max() ) == "179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.00000000" );
    assert( Coin( std::numeric_limits<quint16>::lowest() ) == "0.00000000" );
    assert( Coin( std::numeric_limits<quint16>::min() ) == "0.00000000" );
    assert( Coin( std::numeric_limits<quint16>::max() ) == "65535.00000000" );
    assert( Coin( std::numeric_limits<quint32>::lowest() ) == "0.00000000" );
    assert( Coin( std::numeric_limits<quint32>::min() ) == "0.00000000" );
    assert( Coin( std::numeric_limits<quint32>::max() ) == "4294967295.00000000" );
    assert( Coin( std::numeric_limits<quint64>::lowest() ) == "0.00000000" );
    assert( Coin( std::numeric_limits<quint64>::min() ) == "0.00000000" );
    assert( Coin( std::numeric_limits<quint64>::max() ) == "18446744073709551616.00000000" );

    // test infinity trap
    assert( CoinAmount::toSubsatoshiFormatExpr( std::numeric_limits<qreal>::infinity() ) == "0.0000000000000000" );
    assert( CoinAmount::toSubsatoshiFormatExpr( -std::numeric_limits<qreal>::infinity() ) == "0.0000000000000000" );
    assert( CoinAmount::toSatoshiFormatExpr( std::numeric_limits<qreal>::infinity() ) == "0.00000000" );
    assert( CoinAmount::toSatoshiFormatExpr( -std::numeric_limits<qreal>::infinity() ) == "0.00000000" );

    // sanity testing
    assert( CoinAmount::COIN.isGreaterThanZero() ); // also avoids div0 in tests
    assert( CoinAmount::COIN_PARTS.isGreaterThanZero() );
    assert( CoinAmount::COIN_PARTS_DIV.isGreaterThanZero() );
    assert( CoinAmount::toSatoshiFormatExpr( CoinAmount::SATOSHI_STR.toDouble() ) == CoinAmount::SATOSHI_STR );
    assert( CoinAmount::toSatoshiFormatExpr( CoinAmount::SATOSHI_REAL ) == CoinAmount::SATOSHI_STR );
    assert( CoinAmount::toSatoshiFormatExpr( CoinAmount::SATOSHI_REAL ).toDouble() == CoinAmount::SATOSHI_STR.toDouble() );

    // Coin( QString amount )
    // Coin( qreal amount )
    assert( Coin() == "0.00000000" );
    assert( Coin( "0" ) == "0.00000000" );
    assert( Coin( "0.0" ) == "0.00000000" );
    assert( Coin( "0.0" ) == Coin( 0.0 ) );
    assert( Coin( "0.0" ) == Coin( 0 ) );
    assert( Coin( 0 ) < CoinAmount::SATOSHI );
    assert( Coin( 0.0 ) < CoinAmount::SATOSHI );
    assert( Coin( "0" ) < CoinAmount::SATOSHI );
    assert( Coin( CoinAmount::SATOSHI_REAL ) ==  "0.00000001" );
    assert( Coin( CoinAmount::SATOSHI_REAL ) == CoinAmount::SATOSHI );
    assert( Coin( CoinAmount::SATOSHI_REAL + CoinAmount::SATOSHI_REAL ) == Coin( 2. * CoinAmount::SATOSHI_REAL ) );
    assert( Coin( CoinAmount::SATOSHI_REAL * 10.0 ) == Coin( 0.00000010 ) );
    assert( Coin( CoinAmount::SATOSHI_REAL * 100000000.0 ) == Coin( 1.0 ) );
    assert( Coin( CoinAmount::SATOSHI_REAL ) == Coin( CoinAmount::SATOSHI_STR.toDouble() ) );
    assert( Coin( CoinAmount::SATOSHI_STR.toDouble() ) == CoinAmount::SATOSHI_STR );
    assert( Coin( CoinAmount::SATOSHI_REAL ) == CoinAmount::SATOSHI_STR );
    assert( Coin( CoinAmount::SATOSHI_REAL ).toAmountString().toDouble() == CoinAmount::SATOSHI_STR.toDouble() );
    // note: SATOSHI_REAL is no longer used in engine code, but we'll keep it in testing for consistency

    // Coin::operator /( const Coin &c )
    Coin p( "0.00001" );
    Coin amt( "1.0" );
    Coin qty = amt / p;
    assert( qty == "100000.00000000" );

    // Coin::operator =( const QString &s )
    Coin c13;
    c13 = "1.0";
    assert( c13 == "1.00000000" );

    //exit( 0 );

    // Coin::operator *( const Coin &c ) const
    assert( CoinAmount::COIN == CoinAmount::COIN_PARTS * CoinAmount::SUBSATOSHI );

    // Coin::operator *=( const Coin &c )
    Coin parts = Coin( CoinAmount::COIN_PARTS );
    parts *= CoinAmount::SUBSATOSHI;
    assert( parts == CoinAmount::COIN );

    // Coin::operator *=( const QString &s )
    parts *= QString( "2.0000" );
    parts *= QString( "2" );
    assert( parts == Coin( "4" ) );

    // Coin::operator *(const uint64_t &i )
    assert( parts == CoinAmount::COIN * 4 );

    // Coin::operator /(const uint64_t &i )
    assert( CoinAmount::COIN / 4 == "0.25000000" );
    assert( CoinAmount::COIN / 8 == "0.12500000" );

    // Coin::operator <=( const Coin &c )
    // Coin::operator >=( const Coin &c )
    // Coin::operator <( const Coin &c )
    // Coin::operator >( const Coin &c )
    // Coin::operator !=( const Coin &c )
    // Coin::operator ==( const Coin &c )
    assert(    Coin( 0.00000000 ) <= Coin( 0.00000001 ) );
    assert(    Coin( 0.00000001 ) <= Coin( 0.00000001 ) );
    assert(    Coin( 0.00000001 ) <= Coin( 0.00000002 ) );
    assert(    Coin( 0.99999999 ) <= Coin( 1.00000000 ) );
    assert(    Coin( 0.00000000 ) <  Coin( 0.00000001 ) );
    assert(    Coin( 0.00000001 ) <  Coin( 0.00000002 ) );
    assert(    Coin( 0.99999999 ) <  Coin( 1.00000000 ) );
    assert( !( Coin( 1.00000000 ) <  Coin( 1.00000000 ) ) );
    assert(    Coin( 0.00000001 ) >= Coin( 0.00000000 ) );
    assert(    Coin( 0.00000001 ) >= Coin( 0.00000001 ) );
    assert(    Coin( 0.00000002 ) >= Coin( 0.00000001 ) );
    assert(    Coin( 1.00000000 ) >= Coin( 0.99999999 ) );
    assert(    Coin( 0.00000001 ) >  Coin( 0.00000000 ) );
    assert(    Coin( 0.00000002 ) >  Coin( 0.00000001 ) );
    assert(    Coin( 1.00000000 ) >  Coin( 0.99999999 ) );
    assert( !( Coin( 1.00000000 ) >  Coin( 1.00000000 ) ) );
    assert(    Coin( 0.00000002 ) != Coin( 0.00000001 ) );
    assert(    Coin( 0.00000001 ) != Coin( 0.00000000 ) );
    assert(    Coin( 0.00000010 ) != Coin( 0.00000002 ) );
    assert(    Coin( 1.00000010 ) != Coin( 1.00000011 ) );
    assert( !( Coin( 0.00000000 ) == Coin( 0.00000001 ) ) );
    assert( !( Coin( 0.00000001 ) == Coin( 0.00000002 ) ) );
    assert( !( Coin( 1.00000000 ) == Coin( 0.99999999 ) ) );
    assert(    Coin( 0.00000001 ) == Coin( 0.00000001 ) );
    assert(    Coin( 1.00000001 ) == Coin( 1.00000001 ) );

    // satoshi string function which is used on float function inputs
    assert( CoinAmount::toSubsatoshiFormatExpr( 0.1 ) == "0.1000000000000000" );
    assert( CoinAmount::toSatoshiFormatExpr( 0.1 ) == "0.10000000" );

    Coin c( 0.07777777 );
    c.applyRatio( 0.5 );
    assert( c == "0.03888888" );
    assert( c.toSubSatoshiString() == "0.0388888850000000" );

    // Coin& ratio( qreal r )
    c = 1;
    assert( c.ratio( 3.0 ) == Coin( 3.0 ) );

    // Coin( qreal amount )
    Coin c1( 1000000.0 ); // 1m
    assert( c1 == "1000000.00000000" );
    assert( c1.toSubSatoshiString() == "1000000.0000000000000000" );

    Coin c2( 100000000000000 ); // 100trillion
    assert( c2 == "100000000000000.00000000" );
    assert( c2.toSubSatoshiString() == "100000000000000.0000000000000000" );

    // test the maximum length double accepted, because of the way double is parsed into string.
    // note: if you need more precision, just use division, as seen below
    Coin c3( 100000000000000 ); // 100trillion
    c3.applyRatio( 0.3333333333333333 );
    assert( c3 == "33333333333333.33000000" );
    assert( c3.toSubSatoshiString() == "33333333333333.3300000000000000" );

    // test that we can achieve higher precision than above, with division
    assert( Coin( 100000000000000 ).operator /=( 3 ).toSubSatoshiString() == "33333333333333.3333333333333333" );

    Coin c4( 0.00000001 ); // 1 sat
    assert( c4 == "0.00000001" );
    assert( c4.toSubSatoshiString() == "0.0000000100000000" );

    // Coin::operator /=( const QString &s )
    Coin c12( 10.0 );
    c12 /= "2.0";
    assert( c12 == Coin( 5.0 ) );

    // Coin::operator /=( const uint64_t &i )
    c4 /= 100000000;
    assert( c4 == "0.00000000" );
    assert( c4.toSubSatoshiString() == "0.0000000000000001" );

    // Coin::operator *=( const uint64_t &i )
    c4 *= 100;
    assert( c4 == "0.00000000" );
    assert( c4.toSubSatoshiString() == "0.0000000000000100" );

    Coin c5( "1.0" );
    assert( c5 == "1.00000000" );
    assert( c5.toSubSatoshiString() == "1.0000000000000000" );

    Coin c6( "10000000000000000000000000000000000000.0" );
    assert( c6 == "10000000000000000000000000000000000000.00000000" );
    assert( c6.toSubSatoshiString() == "10000000000000000000000000000000000000.0000000000000000" );

    Coin c7( "77777777777777777777777777777777777777.7777777777777777" );
    assert( c7 == "77777777777777777777777777777777777777.77777777" );
    assert( c7.toSubSatoshiString() == "77777777777777777777777777777777777777.7777777777777777" );

    // instead of Coin( QString ) aborting with std::invalid_argument, filter non-number as 0
    assert( Coin( "1.0abc" ) == Coin() );
    assert( Coin( "1,2" ) == Coin() );
    assert( Coin( "+10" ) == Coin() );
    assert( Coin( "-3" ) == "-3.00000000" ); // tolerate negative sign
    assert( Coin( "0.99999999.a" ) == Coin() );

    // other bad formatting
    assert( Coin( "0.1.00000010" ) == Coin() );
    assert( Coin( "abc0" ) == Coin() );
    assert( Coin( "0_0" ) == Coin() );
    assert( Coin( "0-0" ) == Coin() );
    assert( Coin( "    0.01" ) == Coin( "0.01" ) ); // tolerate spaces outside
    assert( Coin( "0.01  " ) == Coin( "0.01" ) );
    assert( Coin( "0. 01" ) == Coin() ); // don't tolerate spaces inside

    // Coin::operator *( const QString &s )
    Coin test = QString( "0.1" );
    assert( test == "0.10000000" );
    test = test * "2";
    assert( test == "0.20000000" );
    test = test * "2.0";
    assert( test == "0.40000000" );
    test.applyRatio( 0.999 );
    assert( test == "0.39960000" );

    // void applyRatio( qreal r );
    Coin lo_ratio = Coin( 1.0 );
    lo_ratio.applyRatio( 0.999 );
    assert( lo_ratio == Coin( "0.999" ) );
    assert( lo_ratio == Coin( "0.99900000" ) );
    Coin hi_ratio = Coin( 1.0 );
    hi_ratio *= Coin( 1.001 );
    assert( hi_ratio == "1.00099999" );
    assert( hi_ratio == Coin( "1.0009999999999999" ) );

    // fee string
    assert( Coin( 0.2 * 2 ) == "0.40000000" );

    Coin c8 = QString( "0.00080000" );
    assert( c8 == "0.00080000" );
    assert( Coin( c8 * 2 ) == "0.00160000" );
    assert( Coin( c8 * "2" ) == "0.00160000" );

    // fee real
    assert( Coin( 0.0008 ) == "0.00080000" );

    // Coin::operator <( const QString &s ) const
    assert( Coin() < "0.00000001" );
    assert( !( Coin( "0.00000001" ) < "0.00000001" ) );
    // Coin::operator <=( const QString &s ) const
    assert( Coin() <= "0.00000001" );
    assert( Coin() <= "0.00000000" );
    assert( !( Coin( "0.00000005" ) <= "0.00000000" ) );

    // Coin::operator >( const QString &s ) const
    assert( Coin( "0.00000002" ) > "0.00000001" );
    assert( !( Coin( "0.00000002" ) > "0.00000002" ) );
    // Coin::operator >=( const QString &s ) const
    assert( Coin( "0.00000002" ) >= "0.00000001" );
    assert( Coin( "0.00000002" ) >= "0.00000002" );
    assert( !( Coin( "0.00000001" ) >= "0.00000002" ) );

    // Coin::operator +=( const Coin &c )
    Coin c14;
    c14 += Coin( "1.0" );
    assert( c14 == "1.00000000" );

    // Coin::operator -=( const Coin &c )
    Coin c15( "2.0" );
    c15 -= Coin( "1.0" );
    assert( c15 == "1.00000000" );

    // Coin::operator -( const Coin &c
    Coin c16( "2.0" );
    c16 = c16 - Coin( "1.0" );
    assert( c16 == "1.00000000" );

    // Coin::operator +( const Coin &c
    Coin c17( "2.0" );
    c17 = c17 + Coin( "1.0" );
    assert( c17 == "3.00000000" );

    // Engine::findBetterPrice() binance price ticksize
    Coin ticksize = CoinAmount::SATOSHI_STR;
    ticksize += ticksize * qFloor( ( qPow( 10, 1.110 ) ) ); // 10 ^ 1.11 = 12.88 = 12 + 1 = 13
    assert( ticksize == "0.00000013" );

    // Engine::findBetterPrice() polo price ticksize
    assert( Coin( "0.00001000" ).ratio( 0.005 ) + CoinAmount::SATOSHI == "0.00000006" );

    // Engine::findBetterPrice() arithmetic
    assert( Coin( "0.00001000" ) - Coin( "0.00000006" ) == "0.00000994" );
    assert( Coin( "0.00001000" ) + Coin( "0.00000006" ) == "0.00001006" );

    // getHighestActiveBuyPosByPrice(), bad hi_buy price is negative 1
    Coin hi_buy = -1;
    assert( hi_buy == "-1.00000000" );
    assert( hi_buy.isZeroOrLess() );

    // Position::calculateQuantity() q = btc / price
    // Coin::operator /=( const Coin &c )
    Coin q = QString( "0.09988888" );
    q /= Coin( "0.00000003" );
    assert( q == "3329629.33333333" );
    assert( q.toSubSatoshiString() == "3329629.3333333333333333" );

    // Engine::tryMoveSlippageOrder()
    Coin price_lo_original_d = 0.00000015;
    Coin new_buy_price = 0.00000001;
    Coin lo_sell = 0.00000015;
    assert( new_buy_price == CoinAmount::SATOSHI );
    assert( new_buy_price >= CoinAmount::SATOSHI );
    assert( new_buy_price < price_lo_original_d );
    assert( new_buy_price < lo_sell );
    assert( Coin().operator +=( CoinAmount::SATOSHI ) == "0.00000001" ); // avoid infinite loop if logic is broken

    while ( new_buy_price >= CoinAmount::SATOSHI && // new_buy_price >= SATOSHI
            new_buy_price < lo_sell - CoinAmount::SATOSHI && //  new_buy_price < lo_sell - SATOSHI
            new_buy_price < price_lo_original_d ) // new_buy_price < pos->price_lo_original_d
        new_buy_price += CoinAmount::SATOSHI;

    assert( new_buy_price == "0.00000014" );

    // stray order detection logic
    assert( Coin( "1.0" ) > Coin( "1.0" ).ratio( 0.999 ) &&
            Coin( "1.0" ) < Coin( "1.0" ).ratio( 1.001 ) );

    // Coin::isZero()
    assert( Coin().isZero() );
    assert( !Coin( "-1.0" ).isZero() );
    assert( !Coin( "1.0" ).isZero() );

    // Coin::isZeroOrLess()
    assert( Coin( "-1.0" ).isZeroOrLess() );
    assert( Coin( "0.0" ).isZeroOrLess() );
    assert( !Coin( "100.0" ).isZeroOrLess() );

    // Coin::isGreaterThanZero()
    assert( !Coin().isGreaterThanZero() );
    assert( Coin( "0.000000001" ).isGreaterThanZero() );
    assert( Coin( "1.0" ).isGreaterThanZero() );
    assert( Coin( "999999999999999999999999999999999999999999999999999999999.0" ).isGreaterThanZero() );

    // Coin::truncateValueByTicksize
    Coin trunc( "0.00001777" );
    trunc.truncateValueByTicksize( "0.00001000" );
    assert( trunc == "0.00001000" );
    trunc = "0.00000017";
    trunc.truncateValueByTicksize( "0.00000010" );
    assert( trunc == "0.00000010" );
    trunc = "0.09999999";
    trunc.truncateValueByTicksize( "0.01" ); // tolerate unpadded zeroes
    assert( trunc == "0.09000000" );

    ///
    /// stuff we support but aren't even using
    ///
    // test scientific exponent style strings ie. "1e+07" passed via toSatoshiFormat( QVariant(d).toString() )
    assert( Coin( QVariant( 10000000.0 /* 1e7 */ ).toString() ) ==  "10000000.00000000" );
    assert( Coin( QVariant( 1000000000000.0 /* 1e12 */ ).toString() ) ==  "1000000000000.00000000" );
    assert( Coin( QVariant( 10000000000000000000000.0 /* 1e22 */ ).toString() ) ==  "10000000000000000000000.00000000" );
    assert( Coin( QVariant( -10000000.0 /* -1e7 */ ).toString() ) ==  "-10000000.00000000" );

    // strings with negative exponents
    assert( Coin( QVariant( -0.00000001 /* 1e-7 */ ).toString() ).toSubSatoshiString() == "-0.0000000010000000" );
    assert( Coin( QVariant( -0.0000001 /* 1e-6 */ ).toString() ) == "-0.00000001" );
    ///

}

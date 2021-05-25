#include "coinamount_test.h"
#include "coinamount.h"
#include "build-config.h"
#include "global.h"

#include <assert.h>

#include <QVariant>
#include <QtMath>

void CoinAmountTest::test()
{
    testUnit();
    testDoubleFailure();
    testPractical();
    testRandom();
}

void CoinAmountTest::testUnit()
{
    Coin c( 0.07777777 );
    c.applyRatio( 0.5 );
    assert( c == "0.03888888" );
    assert( c.toSubSatoshiString() == "0.0388888850000000" );

    // Coin::clear()
    c.clear();
    assert( c == Coin() );

    // Coin( QString amount )
    // Coin( qreal amount )
    assert( Coin() == "0.00000000" );
    assert( Coin( "0" ) == "0.00000000" );
    assert( Coin( "0.0" ) == "0.00000000" );
    assert( Coin( "0.0" ) == Coin( qreal(0.0) ) );
    assert( Coin( "0.0" ) == Coin( int(0) ) );

    // Coin::operator /( const Coin &c )
    c = Coin( "0.00001" );
    assert( Coin( "1.0" ) / c == "100000.00000000" );

    // Coin::operator =( const QString &s )
    c = Coin();
    c = "1.0";
    assert( c == "1.00000000" );

    // Coin::operator *( const Coin &c ) const
    assert( CoinAmount::COIN == CoinAmount::COIN_PARTS * CoinAmount::SUBSATOSHI );

    // Coin::operator *=( const Coin &c )
    c = Coin( CoinAmount::COIN_PARTS );
    c *= CoinAmount::SUBSATOSHI;
    assert( c == CoinAmount::COIN );

    // Coin::operator *=( const QString &s )
    c *= QString( "2.0000" );
    c *= QString( "2" );
    assert( c == Coin( "4" ) );

    // Coin::operator *(const uint64_t &i )
    assert( c == CoinAmount::COIN * 4 );

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

    // Coin& ratio( qreal r )
    c = 1;
    assert( c.ratio( 3.0 ) == Coin( 3.0 ) );

    // Coin( qreal amount )
    c = Coin( 1000000.0 ); // 1m
    assert( c == "1000000.00000000" );
    assert( c.toSubSatoshiString() == "1000000.0000000000000000" );

    c = Coin( 100000000000000 ); // 100trillion
    assert( c == "100000000000000.00000000" );
    assert( c.toSubSatoshiString() == "100000000000000.0000000000000000" );

    // Coin::operator /=( const QString &s )
    c = Coin( 10.0 );
    c /= "2.0";
    assert( c == Coin( 5.0 ) );

    // Coin::operator /=( const uint64_t &i )
    c = Coin( 0.00000001 ); // 1 sat
    c /= 100000000;
    assert( c == "0.00000000" );
    assert( c.toSubSatoshiString() == "0.0000000000000001" );

    // Coin::operator *=( const uint64_t &i )
    c *= 100;
    assert( c == "0.00000000" );
    assert( c.toSubSatoshiString() == "0.0000000000000100" );

    c = Coin( "1.0" );
    assert( c == "1.00000000" );
    assert( c.toSubSatoshiString() == "1.0000000000000000" );

    c = Coin( "10000000000000000000000000000000000000.0" );
    assert( c == "10000000000000000000000000000000000000.00000000" );
    assert( c.toSubSatoshiString() == "10000000000000000000000000000000000000.0000000000000000" );

    c = Coin( "77777777777777777777777777777777777777.7777777777777777" );
    assert( c == "77777777777777777777777777777777777777.77777777" );
    assert( c.toSubSatoshiString() == "77777777777777777777777777777777777777.7777777777777777" );

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
    assert( Coin( "   0.01   " ) == Coin( "0.01" ) );
    assert( Coin( "0. 01" ) == Coin() ); // don't tolerate spaces inside

    // Coin::operator *( const QString &s )
    c = Coin( "0.1" );
    assert( c == "0.10000000" );
    c = c * "2";
    assert( c == "0.20000000" );
    c = c * "2.0";
    assert( c == "0.40000000" );
    c.applyRatio( 0.999 );
    assert( c == "0.39960000" );

    // void applyRatio( qreal r );
    c = Coin( 1.0 );
    c.applyRatio( 0.999 );
    assert( c == Coin( "0.999" ) );
    assert( c == Coin( "0.99900000" ) );
    c = Coin( 1.0 );
    c *= Coin( 1.001 );
    assert( c == "1.00099999" );
    assert( c == Coin( "1.0009999999999999" ) );

    // fee string
    assert( Coin( 0.2 * 2 ) == "0.40000000" );

    c = Coin( "0.0008" );
    assert( c == "0.00080000" );
    assert( Coin( c * 2 ) == "0.00160000" );
    assert( Coin( c * "2" ) == "0.00160000" );

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
    c = Coin();
    c += Coin( "1.0" );
    assert( c == "1.00000000" );

    // Coin::operator -=( const Coin &c )
    c = Coin( "2.0" );
    c -= Coin( "1.0" );
    assert( c == "1.00000000" );

    // Coin::operator -( const Coin &c
    c = Coin( "2.0" );
    c = c - Coin( "1.0" );
    assert( c == "1.00000000" );

    // Coin::operator +( const Coin &c
    c = Coin( "2.0" );
    c = c + Coin( "1.0" );
    assert( c == "3.00000000" );

    // Coin::operator *=( const Coin &c );
    c = Coin( "1.1" );
    c *= c;
    assert( c == "1.21000000" );

    // Coin::operator /=( const Coin &c )
    c = QString( "0.09988888" );
    c /= Coin( "0.00000003" );
    assert( c == "3329629.33333333" );
    assert( c.toSubSatoshiString() == "3329629.3333333333333333" );

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
    assert( !Coin("-1").isGreaterThanZero() );
    assert( Coin( "0.000000001" ).isGreaterThanZero() );
    assert( Coin( "1.0" ).isGreaterThanZero() );
    assert( Coin( "999999999999999999999999999999999999999999999999999999999.0" ).isGreaterThanZero() );

    // Coin::isLessThanZero()
    assert( !Coin().isLessThanZero() );
    assert( !CoinAmount::COIN.isLessThanZero() );
    assert( Coin( "-0.000000001" ).isLessThanZero() );
    assert( Coin( "-1.0" ).isLessThanZero() );
    assert( Coin( "-999999999999999999999999999999999999999999999999999999999.0" ).isLessThanZero() );

    // Coin::abs()
    assert( Coin( "-7.7777777" ).abs() == Coin( "7.7777777" ) );
    assert( Coin( "-1.0" ).abs() == Coin( "1.0" ) );
    assert( Coin(  "1.0" ).abs() == Coin( "1.0" ) );
    assert( Coin().abs() == Coin() );

    // Coin::operator -()
    assert( -Coin( "7" ) != Coin( "7" ) );
    assert( -Coin( "7" ) == Coin( "-7" ) );
    assert( -Coin( "-7" ) == Coin( "7" ) );
    assert( -Coin( "0" ) == Coin() );
    assert( -Coin() == Coin( "-0" ) );

    // Coin::truncateByTicksize
    c = Coin( "0.00001777" );
    c.truncateByTicksize( "0.00001000" );
    assert( c == "0.00001000" );
    c = "0.00000017";
    c.truncateByTicksize( "0.00000010" );
    assert( c == "0.00000010" );
    c = "0.09999999";
    c.truncateByTicksize( "0.01" ); // tolerate unpadded zeroes
    assert( c == "0.09000000" );

    // Coin::truncatedByTicksize
    assert( Coin( "0.007777777" ).truncatedByTicksize( "0.0001" ) == "0.00770000" );
    assert( Coin( "0.007777777" ).truncatedByTicksize( "0.000001" ) == "0.00777700" );

    // Coin::toInt()
    assert( Coin("-65535").toInt() == int(-65535) );
    assert( Coin("-17").toInt() == int(-17) );
    assert( Coin("-1").toInt() == int(-1) );
    assert( Coin("1").toInt() == int(1) );
    assert( Coin("17").toInt() == int(17) );
    assert( Coin("65535").toInt() == int(65535) );

    // Coin::toUInt32()
    assert( Coin("1").toUInt32() == quint32(1) );
    assert( Coin("17").toUInt32() == quint32(17) );
    assert( Coin("4294967296").toUInt32() == quint32(4294967296) );

    // Coin::toIntSatoshis()
    assert( Coin("0").toIntSatoshis() == qint64(0) );
    assert( Coin("1").toIntSatoshis() == qint64(100000000) );
    assert( Coin("-1").toIntSatoshis() == qint64(-100000000) );
    assert( Coin("10000").toIntSatoshis() == qint64(1000000000000) );
    assert( Coin("0.00000001").toIntSatoshis() == qint64(1) );
    assert( Coin("0.00000100").toIntSatoshis() == qint64(100) );

    // test Coin::ticksizeFromDecimals()
    assert( Coin::ticksizeFromDecimals( 8 ) == CoinAmount::SATOSHI );
    assert( Coin::ticksizeFromDecimals( 7 ) == CoinAmount::SATOSHI *10 );
    assert( Coin::ticksizeFromDecimals( 6 ) == CoinAmount::SATOSHI *100 );
    assert( Coin::ticksizeFromDecimals( 5 ) == CoinAmount::SATOSHI *1000 );
    assert( Coin::ticksizeFromDecimals( 4 ) == CoinAmount::SATOSHI *10000 );
    assert( Coin::ticksizeFromDecimals( 3 ) == CoinAmount::SATOSHI *100000 );
    assert( Coin::ticksizeFromDecimals( 2 ) == CoinAmount::SATOSHI *1000000 );
    assert( Coin::ticksizeFromDecimals( 1 ) == CoinAmount::SATOSHI *10000000 );
    assert( Coin::ticksizeFromDecimals( 0 ) == CoinAmount::COIN );

    // test scientific exponent style strings ie. "1e+07" passed via toSatoshiFormat( QVariant(d).toString() )
    assert( Coin( QVariant( 10000000.0 /* 1e7 */ ).toString() ) ==  "10000000.00000000" );
    assert( Coin( QVariant( 1000000000000.0 /* 1e12 */ ).toString() ) ==  "1000000000000.00000000" );
    assert( Coin( QVariant( 10000000000000000000000.0 /* 1e22 */ ).toString() ) ==  "10000000000000000000000.00000000" );
    assert( Coin( QVariant( -10000000.0 /* -1e7 */ ).toString() ) ==  "-10000000.00000000" );

    // strings with negative exponents
    assert( Coin( QVariant( -0.000000011 /* -11e-7 */ ).toString() ).toSubSatoshiString() == "-0.0000000110000000" );
    assert( Coin( QVariant( -0.00000001 /* -1e-7 */ ).toString() ).toSubSatoshiString() == "-0.0000000100000000" );
    assert( Coin( QVariant( -0.00000010 /* -1e-6 */ ).toString() ) == "-0.00000010" );

    // toCompact()
    assert( Coin( "0" ).toCompact() == "0." );
    assert( Coin( "2" ).toCompact() == "2." );
    assert( Coin( "22222" ).toCompact() == "22222." );
    assert( Coin( "2.222" ).toCompact() == "2.222" ); // also test with trailing decimal value

    // pow(p)
    assert( Coin( CoinAmount::COIN * 4 ).pow( 1 ) == CoinAmount::COIN *4 );
    assert( Coin( CoinAmount::COIN * 4 ).pow( 3 ) == CoinAmount::COIN *64 );
    //assert( Coin( CoinAmount::COIN * 999 ).pow( 65530 ) == CoinAmount::COIN *16 );
}

void CoinAmountTest::testDoubleFailure()
{
    // floats suck #1
    // synopsis: double is okay to store bitcoin quantities, but not other altcoins with larger supplies, or
    //           coins with a different number of coin parts, or prices with a certain amount of decimals, or
    //           tick sizes.
    assert( (double) 2100000000000000.0 != (double) 2099999999999999.0 ); // bitcoin max digits are safe, test that max satoshis != max satoshis -1
    assert( (double) 2251799813685248.0 == (double) 2251799813685247.9 ); // however, if we went over the current supply limit (as some altcoins do), a mantissa of 2^51 == 2^51 - 0.1, among other bad things
    assert( (double) 100000000.00000010 == (double) 100000000.00000011 ); // in addition, an altcoin would lose precision with (1) something like 100m coins or (2) precision higher than satoshis

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
}

void CoinAmountTest::testPractical()
{
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
#if defined(COIN_CATCH_INF)
    assert( CoinAmount::toSubsatoshiFormatExpr( std::numeric_limits<qreal>::infinity() ) == "0.0000000000000000" );
    assert( CoinAmount::toSubsatoshiFormatExpr( -std::numeric_limits<qreal>::infinity() ) == "0.0000000000000000" );
    assert( CoinAmount::toSatoshiFormatExpr( std::numeric_limits<qreal>::infinity() ) == "0.00000000" );
    assert( CoinAmount::toSatoshiFormatExpr( -std::numeric_limits<qreal>::infinity() ) == "0.00000000" );
#endif

    // sanity testing
    assert( CoinAmount::COIN.isGreaterThanZero() ); // also avoids div0 in tests
    assert( CoinAmount::COIN_PARTS.isGreaterThanZero() );
    assert( CoinAmount::toSatoshiFormatExpr( CoinAmount::SATOSHI_STR.toDouble() ) == CoinAmount::SATOSHI_STR );
    assert( CoinAmount::toSatoshiFormatExpr( CoinAmount::SATOSHI_REAL ) == CoinAmount::SATOSHI_STR );
    assert( CoinAmount::toSatoshiFormatExpr( CoinAmount::SATOSHI_REAL ).toDouble() == CoinAmount::SATOSHI_STR.toDouble() );

    // SATOSHI_REAL is no longer used in engine code, but we'll keep these tests for consistency with doubles
    assert( Coin( 0 ) == Coin() );
    assert( Coin() == CoinAmount::ZERO );
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

    // satoshi string function which is used on float function inputs
    assert( CoinAmount::toSubsatoshiFormatExpr( 0.1 ) == "0.1000000000000000" );
    assert( CoinAmount::toSatoshiFormatExpr( 0.1 ) == "0.10000000" );

    // Engine::tryMoveOrder() - confirm that we don't reproduce the "floats suck #2" bug
    Coin buy_price_original_d = 0.00000015;
    Coin new_buy_price = 0.00000001;
    Coin lo_sell = 0.00000015;
    assert( new_buy_price == CoinAmount::SATOSHI );
    assert( new_buy_price >= CoinAmount::SATOSHI );
    assert( new_buy_price < buy_price_original_d );
    assert( new_buy_price < lo_sell );
    assert( Coin().operator +=( CoinAmount::SATOSHI ) == "0.00000001" ); // avoid infinite loop if logic is broken

    while ( new_buy_price >= CoinAmount::SATOSHI && // new_buy_price >= SATOSHI
            new_buy_price < lo_sell - CoinAmount::SATOSHI && //  new_buy_price < lo_sell - SATOSHI
            new_buy_price < buy_price_original_d ) // new_buy_price < pos->buy_price_original_d
        new_buy_price += CoinAmount::SATOSHI;

    assert( new_buy_price == "0.00000014" );

    // Engine::findBetterPrice() binance price ticksize
    Coin ticksize = CoinAmount::SATOSHI_STR;
    ticksize += ticksize * qFloor( ( qPow( 10, 1.110 ) ) ); // 10 ^ 1.11 = 12.88 = 12 + 1 = 13
    assert( ticksize == "0.00000013" );

    // Engine::findBetterPrice() polo price ticksize
    assert( Coin( "0.00001000" ).ratio( 0.005 ) + CoinAmount::SATOSHI == "0.00000006" );

    // Engine::findBetterPrice() arithmetic
    assert( Coin( "0.00001000" ) - Coin( "0.00000006" ) == "0.00000994" );
    assert( Coin( "0.00001000" ) + Coin( "0.00000006" ) == "0.00001006" );

    // getHighestBuyByPrice(), bad hi_buy price is negative 1
    Coin hi_buy = -1;
    assert( hi_buy == "-1.00000000" );
    assert( hi_buy.isZeroOrLess() );

    // run a test that confirms different magnitudes of strings and coin are the same value
    QString test_str = "1";
    Coin test_coin = CoinAmount::COIN;
    for ( int i = 0; i < 100; i++ )
    {
        // compare by-value
        assert( Coin( test_str ) == test_coin );

        // compare by-string
        assert( Coin( test_str ).toAmountString() == test_coin );

        // multiply times 10
        test_coin *= 10;
        test_str += "0";
    }
}

void CoinAmountTest::testRandom()
{
    // test random( uint64::max, uint64::max ) == uint64::max
    Coin test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange64( std::numeric_limits<quint64>::max(), std::numeric_limits<quint64>::max() );
    assert( test_coin == CoinAmount::SATOSHI * std::numeric_limits<quint64>::max() );

    // test toUIntSatoshis() extraction to uint64
    assert( test_coin.toUIntSatoshis() == std::numeric_limits<quint64>::max() );

    // test random( uint32::max, uint32::max ) == uint32::max
    test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange32( std::numeric_limits<quint32>::max(), std::numeric_limits<quint32>::max() );
    assert( test_coin == CoinAmount::SATOSHI * std::numeric_limits<quint32>::max() );

    // test toUIntSatoshis() extraction to int64
    assert( test_coin.toIntSatoshis() == std::numeric_limits<quint32>::max() );

    // test random( uint16::max +1, uint32::max -1 )
    test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange32( std::numeric_limits<quint16>::max() +1, std::numeric_limits<quint32>::max() -1 );
    assert( test_coin >= CoinAmount::SATOSHI * std::numeric_limits<quint16>::max() + CoinAmount::SATOSHI &&
            test_coin <= CoinAmount::SATOSHI * std::numeric_limits<quint32>::max() - CoinAmount::SATOSHI );

    // test random( uint32::max +1, uint64::max -1 )
    test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange64( (quint64) std::numeric_limits<quint32>::max() +1, std::numeric_limits<quint64>::max() -1 );
    assert( test_coin >= CoinAmount::SATOSHI * std::numeric_limits<quint32>::max() + CoinAmount::SATOSHI &&
            test_coin <= CoinAmount::SATOSHI * std::numeric_limits<quint64>::max() - CoinAmount::SATOSHI );

    // test random( 0, uint32::max )
    test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange32( 0, std::numeric_limits<quint32>::max() );
    assert( test_coin >= Coin() && test_coin <= CoinAmount::SATOSHI * std::numeric_limits<quint32>::max() );

    // test random( 0, uint32::max -1 )
    test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange32( 0, std::numeric_limits<quint32>::max() -1 );
    assert( test_coin >= Coin() && test_coin <= CoinAmount::SATOSHI * std::numeric_limits<quint32>::max() -1 );

    // test random( 0, uint64::max )
    test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange64( 0, std::numeric_limits<quint64>::max() );
    assert( test_coin >= Coin() && test_coin <= CoinAmount::SATOSHI * std::numeric_limits<quint64>::max() );

    // test random( 0, uint64::max -1 )
    test_coin = CoinAmount::SATOSHI * Global::getSecureRandomRange64( 0, std::numeric_limits<quint64>::max() -1 );
    assert( test_coin >= Coin() && test_coin <= CoinAmount::SATOSHI * std::numeric_limits<quint64>::max() -1 );
}

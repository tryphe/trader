#include "coinamount.h"
#include "build-config.h"

#include <vector>
#include <gmp.h>

#include <QString>
#include <QDebug>
#include <QThread>

static inline QString qrealToSubsatoshis( qreal r )
{
    return CoinAmount::toSubsatoshiFormatExpr( r ).remove( CoinAmount::decimal_exp );
}

static inline QString qstringToSubsatoshis( QString s )
{
    return CoinAmount::toSatoshiFormatStr( s, Coin::subsatoshi_decimals ).remove( CoinAmount::decimal_exp );
}

Coin::Coin()
{
    mpz_init( b );
}

Coin::~Coin()
{
    mpz_clear( b );
}

Coin::Coin( const Coin &big )
{
    mpz_init_set( b, big.b );
}

Coin::Coin( QString amount )
{
    mpz_init_set_str( b, qstringToSubsatoshis( amount ).toLocal8Bit().data(), Coin::str_base );
}

Coin::Coin( qreal amount )
{
    mpz_init_set_str( b, qrealToSubsatoshis( amount ).toLocal8Bit().data(), Coin::str_base );
}

void Coin::clear()
{
    mpz_set_ui( b, 0 );
}

Coin &Coin::operator =( const QString &in )
{
    mpz_set_str( b, qstringToSubsatoshis( in ).toLocal8Bit().data(), Coin::str_base );
    return *this;
}

Coin &Coin::operator =( const Coin &c )
{
    mpz_set( b, c.b );
    return *this;
}

Coin &Coin::operator /=( const QString &s )
{
    return operator /=( Coin( s ) );
}

Coin &Coin::operator +=( const Coin &c )
{
    mpz_add( b, b, c.b ); // b += c.b;
    return *this;
}

Coin &Coin::operator -=( const Coin &c )
{
    mpz_sub( b, b, c.b ); // b -= c.b;
    return *this;
}

Coin &Coin::operator /=( const Coin &c )
{
#if defined(COIN_CATCH_DIV0)
    if ( mpz_cmp_ui( c.b, 0 ) == 0 )
    {
        qDebug() << "[Coin] trapped div0 in" << __FUNCTION__ << toSubSatoshiString() << "/" << c.toSubSatoshiString();
        mpz_set_ui( b, 0 );
        return *this;
    }
#endif

    mpz_mul( b, b, CoinAmount::COIN.b );
    mpz_div( b, b, c.b );
    return *this;
}

Coin &Coin::operator *=( const Coin &c )
{
    mpz_mul( b, b, c.b );
    mpz_div( b, b, CoinAmount::COIN.b );
    return *this;
}

Coin &Coin::operator /=( const uint64_t &i )
{
#if defined(COIN_CATCH_DIV0)
    if ( i == 0 )
    {
        qDebug() << "[Coin] trapped div0 in" << __FUNCTION__ << toSubSatoshiString() << "/" << i;
        mpz_set_ui( b, 0 );
        return *this;
    }
#endif

    mpz_div_ui( b, b, i ); // b /= i;
    return *this;
}

Coin &Coin::operator *=( const uint64_t &i )
{
    mpz_mul_ui( b, b, i ); // b *= i;
    return *this;
}

Coin Coin::operator *( const QString &s ) const
{
    return operator *( Coin( s ) );
}

Coin Coin::operator -( const Coin &c ) const
{
    Coin r = *this;
    r -= c;
    return r;
}

Coin Coin::operator +( const Coin &c ) const
{
    Coin r = *this;
    r += c;
    return r;
}

Coin Coin::operator *( const Coin &c ) const
{
    Coin in = c;
    mpz_mul( in.b, in.b, b );
    mpz_div( in.b, in.b, CoinAmount::COIN.b );
    return in;
}

Coin Coin::operator /( const Coin &c ) const
{
    Coin in = *this;
    in /= c;
    return in;
}

Coin Coin::operator *( const uint64_t &i ) const
{
    Coin r = *this;
    r *= i;
    return r;
}

Coin Coin::operator /( const uint64_t &i ) const
{
    Coin r = *this;
    r /= i;
    return r;
}

bool Coin::operator ==( const QString &s ) const
{
    return s == toAmountString();
}

bool Coin::operator !=( const QString &s ) const
{
    return s != toAmountString();
}

bool Coin::operator ==( const Coin &c ) const
{
    return mpz_cmp( b, c.b ) == 0;
}

bool Coin::operator !=( const Coin &c ) const
{
    return mpz_cmp( b, c.b ) != 0;
}

bool Coin::operator <( const QString &s ) const
{
    return operator <( Coin( s ) );
}

bool Coin::operator >( const QString &s ) const
{
    return operator >( Coin( s ) );
}

bool Coin::operator <( const Coin &c ) const
{
    return mpz_cmp( b, c.b ) < 0;
}

bool Coin::operator >( const Coin &c ) const
{
    return mpz_cmp( b, c.b ) > 0;
}

bool Coin::operator <=( const QString &s ) const
{
    return operator <=( Coin( s ) );
}

bool Coin::operator >=( const QString &s ) const
{
    return operator >=( Coin( s ) );
}

bool Coin::operator <=( const Coin &c ) const
{
    return mpz_cmp( b, c.b ) <= 0;
}

bool Coin::operator >=( const Coin &c ) const
{
    return mpz_cmp( b, c.b ) >= 0;
}

Coin Coin::operator -() const
{
    return isZeroOrLess() ? CoinAmount::ZERO - *this :
                            *this * ( CoinAmount::ZERO - CoinAmount::COIN );
}

Coin Coin::abs() const
{
    return isGreaterThanZero() ? *this :
                                 CoinAmount::ZERO - *this;
}

Coin Coin::pow( int p ) const
{
    Coin ret = *this;
    while ( p-- > 1 )
        ret = *this * ret;

    return ret;
}

//Coin Coin::root_slow( const int n, const Coin &precision )
//{
//    Coin lo;
//    Coin hi = *this;
//    Coin guess;

//    while ( true )
//    {
//        guess = ( lo + hi ) /2;
//        if ( ( guess.pow( n ) - *this ).abs() < precision )
//            return guess;

//        if ( guess.pow( n ) > *this )
//            hi = guess;
//        else
//            lo = guess;
//    }

//    return Coin();
//}

//Coin Coin::root_newton( const int n, const Coin &precision )
//{
//    Coin guess = *this /2;

//    while ( ( guess.pow( n ) - *this ).abs() > precision )
//        guess -= ( guess.pow( n ) - *this ) / ( ( guess * n ).pow( n -1 ) );

//    return guess;
//}

bool Coin::isZero() const
{
    return mpz_cmp_ui( b, 0 ) == 0;
}

bool Coin::isZeroOrLess() const
{
    return mpz_cmp_ui( b, 0 ) <= 0;
}

bool Coin::isLessThanZero() const
{
    return mpz_cmp_ui( b, 0 ) < 0;
}

bool Coin::isGreaterThanZero() const
{
    return mpz_cmp_ui( b, 0 ) > 0;
}

Coin::operator QString() const
{
    return toAmountString();
}

QString Coin::toString( const int decimals = Coin::subsatoshi_decimals ) const
{
    // thread-safe static opt
    static QMap<Qt::HANDLE, std::vector<char>> buffer_map;
    std::vector<char> &buffer = buffer_map[ QThread::currentThreadId() ];

    // resize the buffer to how many base10 bytes we'll need
    size_t buffer_size = mpz_sizeinbase( b, Coin::str_base ) +2; // "two extra bytes for a possible minus sign, and null-terminator."
    if ( buffer_size != buffer.size() )
        buffer.resize( buffer_size );

    // fill buffer and make a QString out of it
    mpz_get_str( buffer.data(), Coin::str_base, b );
    QString ret( buffer.data() );

    // alternative method which uses malloc/free, slower than std::vector
//    char *c = mpz_get_str( nullptr, 10, b );
//    ret.insert( 0, c );
//    free( c );

    // temporarily remove the negative sign so we can properly prepend zeroes
    bool is_negative = ret.at( 0 ) == CoinAmount::minus_exp;
    if ( is_negative ) ret.remove( 0, 1 );

    int sz = ret.size();
    // truncate at proper digit if we have less than 16 digits
    if ( decimals < Coin::subsatoshi_decimals )
    {
        int diff = Coin::subsatoshi_decimals - decimals;
        ret.chop( diff );

        // update sz
        if ( sz <= Coin::satoshi_decimals )
            sz = 0;
        else
            sz -= diff;
    }

    while ( sz < decimals )
    {
        sz++; // add only if statement passes
        ret.prepend( CoinAmount::zero_exp );
    }

    // reuse sz to calculate a new decimal index and insert decimal
    sz -= decimals;
    ret.insert( sz, CoinAmount::decimal_exp );

    // decimal is the first character, return with prepended zero
    if ( sz == 0 )
        ret.prepend( CoinAmount::zero_exp );

    if ( is_negative )
        ret.prepend( CoinAmount::minus_exp );

    return ret;
}

QString Coin::toSubSatoshiString() const
{
    return toString( Coin::subsatoshi_decimals );
}

QString Coin::toAmountString() const
{
    return toString( Coin::satoshi_decimals );
}

QString Coin::toCompact() const
{
    QString ret = toString( Coin::satoshi_decimals );

    while ( ret.endsWith( CoinAmount::zero_exp ) )
        ret.chop( 1 );

    return ret;
}

int Coin::toInt() const
{
    QString str = toString( 0 );

    // truncate decimal
    str.chop( 1 );

    // convert to int
    bool ok = false;
    const int ret = str.toInt( &ok );

    // check for valid int
    if ( !ok )
    {
        qDebug() << "[Coin] local error: couldn't read int32 out of" << str;
        return 0;
    }

    return ret;
}

quint32 Coin::toUInt32() const
{
    QString str = toString( 0 );

    // truncate decimal
    str.chop( 1 );

    // convert to int
    bool ok = false;
    const quint32 ret = str.toULong( &ok );

    // check for valid int
    if ( !ok )
    {
        qDebug() << "[Coin] local error: couldn't read uint32 out of" << str;
        return 0;
    }

    return ret;
}

qint64 Coin::toIntSatoshis() const
{
    QString str = toString( Coin::satoshi_decimals );

    // remove decimal
    str.remove( str.size() - ( Coin::satoshi_decimals +1 ), 1 );

    // convert to int
    bool ok = false;
    const qint64 ret = str.toLongLong( &ok );

    // check for valid int
    if ( !ok )
    {
        qDebug() << "[Coin] local error: couldn't read int64 out of" << str;
        return 0;
    }

    return ret;
}

quint64 Coin::toUIntSatoshis() const
{
    QString str = toString( Coin::satoshi_decimals );

    // remove decimal
    str.remove( str.size() - ( Coin::satoshi_decimals +1 ), 1 );

    // convert to int
    bool ok = false;
    const quint64 ret = str.toULongLong( &ok );

    // check for valid int
    if ( !ok )
    {
        qDebug() << "[Coin] local error: couldn't read uint64 out of" << str;
        return 0;
    }

    return ret;
}

void Coin::applyRatio( qreal r )
{
#if defined(COIN_CATCH_INF)
    // trap infinity as 0
    if ( r == std::numeric_limits<qreal>::infinity() ||
         r == -std::numeric_limits<qreal>::infinity() )
        r = 0.;
#endif

    // doesn't apply the ratio beyond Coin digits
    operator *= ( Coin( r ) );
    operator /= ( CoinAmount::COIN );
}

Coin Coin::ratio( qreal r ) const
{
    Coin ret = *this;
    ret.applyRatio( r );
    return ret;
}

void Coin::truncateByTicksize( QString ticksize )
{
    // set default value when argument is blank
    if ( ticksize.isEmpty() )
        ticksize = CoinAmount::SATOSHI_STR;

    QString s = toAmountString();
    int dec_idx = s.indexOf( CoinAmount::decimal_exp );
    int ticksize_idx_one = ticksize.indexOf( CoinAmount::one_exp );

    // sanity check: make sure there's a '1' and a decimal
    if ( dec_idx < 0 || ticksize_idx_one < 0 )
        return;

    // apply ticksize
    s.truncate( dec_idx + ticksize_idx_one );

    operator =( s );
}

Coin Coin::truncatedByTicksize( QString ticksize )
{
    Coin ret = *this;
    ret.truncateByTicksize( ticksize );
    return ret;
}

Coin Coin::ticksizeFromDecimals( int dec )
{
    Coin ret = CoinAmount::SATOSHI;

    if ( dec > Coin::satoshi_decimals || dec < 0 )
        return ret;

    // increase magnitude (decimals-8) times
    for ( int i = 0; i < Coin::satoshi_decimals - dec; i++ )
        ret *= Coin::str_base;

    return ret;
}

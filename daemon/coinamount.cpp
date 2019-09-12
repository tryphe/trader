#include "coinamount.h"
#include "global.h"

#include <vector>
#include <gmp.h>

#include <QString>
#include <QDebug>

static inline QString qrealToSubsatoshis( qreal r )
{
    return CoinAmount::toSubsatoshiFormatExpr( r ).remove( CoinAmount::decimal_exp );
}

static inline QString qstringToSubsatoshis( QString s )
{
    return CoinAmount::toSatoshiFormatStr( s, CoinAmount::subsatoshi_decimals ).remove( CoinAmount::decimal_exp );
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
    mpz_init_set_str( b, qstringToSubsatoshis( amount ).toLocal8Bit().data(), CoinAmount::str_base );
}

Coin::Coin( qreal amount )
{
    mpz_init_set_str( b, qrealToSubsatoshis( amount ).toLocal8Bit().data(), CoinAmount::str_base );
}

Coin &Coin::operator =( const QString &in )
{
    mpz_set_str( b, qstringToSubsatoshis( in ).toLocal8Bit().data(), CoinAmount::str_base );
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
    mpz_mul( b, b, CoinAmount::COIN.b );
    if ( mpz_cmp_ui( c.b, 0 ) != 0 )
        mpz_div( b, b, c.b );
    else
    {
        kDebug() << "[Coin] trapped div0 in" << __FUNCTION__ << toSubSatoshiString() << "/" << c.toSubSatoshiString();
        mpz_set_ui( b, 0 );
    }
    return *this;
}

Coin &Coin::operator *=( const Coin &c )
{
    mpz_mul( b, b, CoinAmount::COIN_PARTS.b );
    mpz_mul( b, b, c.b );
    mpz_div( b, b, CoinAmount::COIN_PARTS_DIV.b );
    return *this;
}

Coin &Coin::operator /=( const uint64_t &i )
{
    if ( i != 0 )
        mpz_div_ui( b, b, i ); // b /= i;
    else
    {
        kDebug() << "[Coin] trapped div0 in" << __FUNCTION__ << toSubSatoshiString() << "/" << i;
        mpz_set_ui( b, 0 );
    }
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
    mpz_mul( in.b, in.b, CoinAmount::COIN_PARTS.b );
    mpz_mul( in.b, in.b, b );
    mpz_div( in.b, in.b, CoinAmount::COIN_PARTS_DIV.b );
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
    return isZeroOrLess() ? Coin() - *this :
                            *this * ( Coin()-CoinAmount::COIN );
}

Coin Coin::abs() const
{
    return isGreaterThanZero() ? *this :
                                 Coin() - *this;
}

bool Coin::isZero() const
{
    return mpz_cmp_ui( b, 0 ) == 0;
}

bool Coin::isZeroOrLess() const
{
    return mpz_cmp_ui( b, 0 ) <= 0;
}

bool Coin::isGreaterThanZero() const
{
    return mpz_cmp_ui( b, 0 ) > 0;
}

Coin::operator QString() const
{
    return toAmountString();
}

// note: these next few functions are optimized with 'static' but it'll only work with 1 thread using Coin
QString Coin::toSubSatoshiString() const
{
    // note: if multithread, then remove these static declarations
    static size_t sz;
    static std::vector<char> buffer = std::vector<char>( 10 );

    // resize the buffer to how many base10 bytes we'll need
    sz = mpz_sizeinbase( b, CoinAmount::str_base ) +2; // "two extra bytes for a possible minus sign, and null-terminator."
    if ( sz != buffer.size() )
        buffer.resize( sz );

    // fill buffer and make a QString out of it
    mpz_get_str( buffer.data(), CoinAmount::str_base, b );
    QString ret( buffer.data() );

    // alternative method which uses malloc/free, about 2.5% slower than std::vector
//    char *c = mpz_get_str( nullptr, 10, b );
//    ret.insert( 0, c );
//    free( c );

    //qDebug() << "str:" << ret;

    // temporarily remove the negative sign so we can properly prepend zeroes
    bool is_negative = ret.at( 0 ) == CoinAmount::minus_exp;
    if ( is_negative ) ret.remove( 0, 1 );

    int sz1 = ret.size();
    while ( sz1 < CoinAmount::subsatoshi_decimals )
    {
        sz1++; // add only if statement passes
        ret.prepend( CoinAmount::zero_exp );
    }

    // reuse sz1 to calculate a new decimal index and insert decimal
    sz1 -= CoinAmount::subsatoshi_decimals;
    ret.insert( sz1, CoinAmount::decimal_exp );

    // decimal is the first character, return with prepended zero
    if ( sz1 == 0 )
        ret.prepend( CoinAmount::zero_exp );

    if ( is_negative ) ret.prepend( CoinAmount::minus_exp );

    return ret;
}

QString Coin::toAmountString() const
{
    QString ret = toSubSatoshiString();
    int trunc_idx = ret.indexOf( CoinAmount::decimal_exp ) +9;

    if ( ret.size() > trunc_idx )
        ret.truncate( trunc_idx );

    return ret;
}

int Coin::toInt() const
{
    // index of y = x / m_tick_size;
    QString str = toAmountString();
    int dec_idx = str.indexOf( QChar('.') );

    // check for valid decimal, we should never get here
    if ( dec_idx < 0 )
    {
        kDebug() << "[Coin] local error: couldn't read decimal out of" << str;
        return 0;
    }

    // truncate decimal
    str.truncate( dec_idx );

    // convert to int
    bool ok = false;
    int ret = str.toInt( &ok );

    // check for valid int
    if ( !ok )
    {
        kDebug() << "[Spruce] local error: couldn't read decimal out of" << str;
        return 0;
    }

    return ret;
}

quint32 Coin::toUInt32() const
{
    // index of y = x / m_tick_size;
    QString str = toAmountString();
    int dec_idx = str.indexOf( QChar('.') );

    // check for valid decimal, we should never get here
    if ( dec_idx < 0 )
    {
        kDebug() << "[Coin] local error: couldn't read decimal out of" << str;
        return 0;
    }

    // truncate decimal
    str.truncate( dec_idx );

    // convert to int
    bool ok = false;
    quint32 ret = str.toInt( &ok );

    // check for valid int
    if ( !ok )
    {
        kDebug() << "[Spruce] local error: couldn't read decimal out of" << str;
        return 0;
    }

    return ret;
}

void Coin::applyRatio( qreal r )
{
    // trap infinity as 0
    if ( r == std::numeric_limits<qreal>::infinity() ||
         r == -std::numeric_limits<qreal>::infinity() )
        r = 0.;

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

#ifndef COINAMOUNT_H
#define COINAMOUNT_H

#include <gmp.h>

#include <QString>
#include <QDebug>

class Coin
{
public:
    Coin();
    ~Coin();
    Coin( const Coin &big );
    Coin( QString amount );
    Coin( qreal amount );

    Coin& operator =( const QString &s );
    Coin& operator =( const Coin &c );
    Coin& operator /=( const QString &s );
    Coin& operator +=( const Coin &c );
    Coin& operator -=( const Coin &c );
    Coin& operator /=( const Coin &c );
    Coin& operator *=( const Coin &c );
    Coin& operator /=( const uint64_t &i );
    Coin& operator *=( const uint64_t &i );

    Coin operator *( const QString &s ) const;
    Coin operator -( const Coin &c ) const;
    Coin operator +( const Coin &c ) const;
    Coin operator *( const Coin &c ) const;
    Coin operator /( const Coin &c ) const;
    Coin operator *( const uint64_t &i ) const;
    Coin operator /( const uint64_t &i ) const;

    bool operator ==( const QString &s ) const;
    bool operator !=( const QString &s ) const;
    bool operator ==( const Coin &c ) const;
    bool operator !=( const Coin &c ) const;
    bool operator <( const QString &s ) const;
    bool operator >( const QString &s ) const;
    bool operator <( const Coin &c ) const;
    bool operator >( const Coin &c ) const;
    bool operator <=( const QString &s ) const;
    bool operator >=( const QString &s ) const;
    bool operator <=( const Coin &c ) const;
    bool operator >=( const Coin &c ) const;

    Coin operator -() const;
    Coin abs() const;

    bool isZero() const;
    bool isZeroOrLess() const;
    bool isGreaterThanZero() const;

    operator QString() const;

    QString toSubSatoshiString() const;
    QString toAmountString() const;

    void applyRatio( qreal r );
    Coin ratio( qreal r ) const;

    void truncateByTicksize( QString ticksize );
    Coin truncatedByTicksize( QString ticksize );

private:
    mpz_t b;
};

namespace CoinAmount
{

// note: there MUST NOT be any Coin::operators used here as we might recursively reference another
// uninitialized static variable, which is undefined and will break things
static const Coin COIN = QString( "1.0" );
static const Coin ZERO = QString( "0" );
static const Coin COIN_PARTS = QString( "10000000000000000" );
static const Coin COIN_PARTS_DIV = QString( "100000000000000000000000000000000" ); // =COIN_PARTS^2
static const Coin SATOSHI_PARTS = QString( "100000000" );
static const Coin SATOSHI = QString( "0.00000001" );
static const Coin SUBSATOSHI = QString( "0.0000000000000001" );
static const Coin A_LOT = QString( "10000000000000000000000000000000000000000000000000000000000000000000000" );
static const QString SATOSHI_STR = QString( "0.00000001" );
static const qreal SATOSHI_REAL = 0.00000001;
static const Coin ORDER_SHIM = QString( "0.000000005" );

static const QChar decimal_exp = QChar( '.' );
static const QChar zero_exp = QChar( '0' );
static const QChar one_exp = QChar( '1' );
static const QChar minus_exp = QChar( '-' );
static const QChar plus_exp = QChar( '+' );
static const int str_base = 10;
static const int subsatoshi_decimals = 16;

static inline void toSatoshiFormat( QString &s, int decimals = 8 )
{
    // trimmed() alternative
    int sz0 = s.size();
    while ( sz0 > 0 && s.at( 0 ).isSpace() )
    {
        sz0--;
        s.remove( 0, 1 );
    }
    while ( sz0 > 0 && s.at( sz0 -1 ).isSpace() )
    {
        s.remove( sz0-- -1, 1 );
    }

    // filter not 0-9 '.' or '-'
    int dec_count = 0, ct = 0;
    for ( QString::const_iterator i = s.begin(); i != s.end(); i++ )
    {
        // trap multiple decimals
        if ( *i == CoinAmount::decimal_exp )
        {
            if ( dec_count > 0 )
            {
                dec_count = 0;
                s.clear();
                break;
            }

            dec_count++;
        }

        // try to read scientific notation string
        if ( *i == QChar('e') )
        {
            // if we have a decimal, remove it and iterate backwards
            if ( dec_count > 0 )
            {
                int dec_idx = s.indexOf( CoinAmount::decimal_exp );
                s.remove( dec_idx, 1 );
                dec_count--;
                ct--;
            }

            int sz = s.size();
            int e = s.mid( ct +2, sz - ct +2 ).toInt(); // read exponent
            QChar sign = s.at( ct +1 ); // read sign
            int sign_int = sign == CoinAmount::plus_exp ? 1 :
                           sign == CoinAmount::minus_exp ? -1 : 0;

            if ( sign_int == -1 ) e--; // one less if we're negative

            // make sure there's something to read and the conversion passed
            if ( sign_int != 0 && sz > ct +2 && e > 0 )
            {
                s.remove( ct, sz - ct );

                // append zeroes
                while ( e-- > 0 )
                {
                    if ( sign_int == -1 )
                        s.insert( 1, CoinAmount::zero_exp );
                    else
                        s.append( CoinAmount::zero_exp );
                }

                // insert decimal
                if ( sign_int == -1 )
                {
                    s.insert( 1, CoinAmount::decimal_exp );
                    s.insert( 1, CoinAmount::zero_exp );
                }
                else
                    s.append( CoinAmount::decimal_exp );

                // restart loop and check again
                dec_count = ct = 0;
                i = s.begin();
                continue;
            }
            else
            {
                dec_count = 0;
                s.clear();
                break;
            }
        }
        // trap other junk
        else if ( !i->isNumber() && *i != CoinAmount::decimal_exp &&
             !( *i == CoinAmount::minus_exp && ct == 0 ) ) // allow '-' if first character
        {
            //qDebug() << "local warning: toSatoshiFormatStr caught bad character" << *i << "in" << s;
            dec_count = 0;
            s.clear();
            break;
        }
        ct++;
    }

    // if there is no decimal, append ".0"
    if ( dec_count == 0 )
    {
        s.append( CoinAmount::decimal_exp );
        s.append( CoinAmount::zero_exp );
    }

    // prepend zero if decimal is the first character
    if ( s.at( 0 ) == CoinAmount::decimal_exp )
        s.prepend( CoinAmount::zero_exp );
    else if ( s.at( 0 ) == CoinAmount::minus_exp &&
              s.at( 1 ) == CoinAmount::decimal_exp )
        s.insert( 1, CoinAmount::zero_exp );

    // remove front padded zeroes
    while ( s.at( 0 ) == CoinAmount::zero_exp &&
            s.at( 1 ) == CoinAmount::zero_exp )
        s.remove( 0, 1 );

    int dec_idx = s.indexOf( CoinAmount::decimal_exp );

    // add zeroes on end
    decimals++;
    int sz1 = s.size();
    while ( sz1++ - dec_idx < decimals )
        s.append( CoinAmount::zero_exp );

    s.truncate( dec_idx + decimals );
}

static inline QString toSatoshiFormatStr( QString &s, int decimals = 8 )
{
    QString ret = s;
    toSatoshiFormat( ret, decimals );
    return ret;
}

static inline QString toSatoshiFormatStrExpr( QString s )
{
    return toSatoshiFormatStr( s );
}

static inline QString toSatoshiFormat( qreal &r )
{
    if ( r == std::numeric_limits<qreal>::infinity() ||
         r == -std::numeric_limits<qreal>::infinity() )
        r = 0.;

    static QString ret;
    ret = QString::number( r, 'f', CoinAmount::str_base );

    int dec_idx = ret.indexOf( CoinAmount::decimal_exp );
    ret.truncate( dec_idx +9 ); // truncate to 8 decimals
    return ret;
}
static inline QString toSatoshiFormatExpr( qreal r )
{
    return toSatoshiFormat( r );
}

static inline QString toSubsatoshiFormat( qreal &r )
{
    if ( r == std::numeric_limits<qreal>::infinity() ||
         r == -std::numeric_limits<qreal>::infinity() )
        r = 0.;

    static QString ret;
    ret = QString::number( r, 'f', CoinAmount::subsatoshi_decimals );

    int dec_idx;
    dec_idx = ret.indexOf( CoinAmount::decimal_exp );
    ret.truncate( dec_idx + CoinAmount::subsatoshi_decimals + 1 ); // truncate to 8 decimals
    return ret;
}
static inline QString toSubsatoshiFormatExpr( qreal r )
{
    return toSubsatoshiFormat( r );
}
static inline QString toSubsatoshiFormat( const qreal &r )
{
    return toSubsatoshiFormat( r );
}

} // CoinAmount


#endif // COINAMOUNT_H

#include "market.h"

Market::Market()
{
}

Market::Market( const QString &market )
{
    // find separator index
    int sep_idx = market.indexOf( QChar( '_' ) );
    if ( sep_idx < 0 )
        sep_idx = market.indexOf( QChar( '-' ) );

    // check for separator
    if ( sep_idx < 0 )
        return;

    base = market.left( sep_idx );
    quote = market.mid( sep_idx +1, market.size() - sep_idx -1 );
}

Market::Market( const QString &_base, const QString &_quote )
  : base( _base ),
    quote( _quote )
{
}

bool Market::isValid() const
{
    return !( base.isEmpty() || quote.isEmpty() );
}

bool Market::operator ==(const QString &other ) const
{
    return operator QString() == other;
}

Market::operator QString() const
{
    return !isValid() ? QString() :
            QString( DEFAULT_MARKET_STRING_TEMPLATE )
            .arg( base )
            .arg( quote );
}

QString Market::toExchangeString( const quint8 engine_type ) const
{
    return QString( engine_type == ENGINE_BITTREX  ? BITTREX_MARKET_STRING_TEMPLATE :
                    engine_type == ENGINE_BINANCE  ? BINANCE_MARKET_STRING_TEMPLATE :
                    engine_type == ENGINE_POLONIEX ? POLONIEX_MARKET_STRING_TEMPLATE :
                                                     QString() )
            .arg( base )
            .arg( quote );
}

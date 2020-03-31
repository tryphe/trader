#ifndef MARKET_H
#define MARKET_H

#include "global.h"
#include "positiondata.h"
#include "coinamount.h"

#include <QVector>
#include <QString>
#include <QJsonArray>

class Market
{
public:
    Market();
    Market( const QString &market );
    Market( const QString &_base, const QString &_quote );
    bool isValid() const;

    bool operator ==( const QString &other ) const;
    bool operator !=( const QString &other ) const;
    operator QString() const; // obtain universal string
    QString toExchangeString( const quint8 engine_type ) const; // obtain exchange-specific string

    const QString &getBase() const { return base; }
    const QString &getQuote() const { return quote; }

    Market getInverse() const { return Market( getQuote(), getBase() ); }

private:
    QString base, quote;
};

struct MarketInfo
{
    explicit MarketInfo()
    {
    }

    operator QString() const { return QString( "bid %1 ask %2 tradeable %3 price_tick %4 matcher_tick %5 qty_tick %6 order_min %7 order_max %8 order_dc %9 order_dc_nice %10 order_landmark_thresh %11 order_landmark_start %12 slippage_timeout %13 market_offset %14 market_sentiment %15" )
                                   .arg( highest_buy, -16 )
                                   .arg( lowest_sell, -16 )
                                   .arg( is_tradeable )
                                   .arg( price_ticksize )
                                   .arg( quantity_ticksize )
                                   .arg( matcher_ticksize )
                                   .arg( order_min, -2 )
                                   .arg( order_max, -2 )
                                   .arg( order_dc, -2 )
                                   .arg( order_dc_nice, -2 )
                                   .arg( order_landmark_thresh, -2 )
                                   .arg( order_landmark_start, -2 )
                                   .arg( slippage_timeout, -6 )
                                   .arg( market_offset, -6 )
                                   .arg( market_sentiment ); } // TODO: fill in this string for binance stuff

    void jsonifyTicker( QJsonArray &arr, const QString &market ) const
    {
        arr += "t";
        arr += market;
        arr += highest_buy.toAmountString();
        arr += lowest_sell.toAmountString();
    }
    void jsonifyBid( QJsonArray &arr, const QString &market ) const
    {
        arr += "b";
        arr += market;
        arr += highest_buy.toAmountString();
    }
    void jsonifyAsk( QJsonArray &arr, const QString &market ) const
    {
        arr += "a";
        arr += market;
        arr += lowest_sell.toAmountString();
    }

    // prices for this market
    QVector<QString> order_prices;

    // internal ticker
    Coin highest_buy;
    Coin lowest_sell;

    // ping-pong settings
    QVector<PositionData> /*position_index*/ position_index;
    qint32 /*order count limit*/ order_min{ 5 };
    qint32 /*order count limit*/ order_max{ 10 };
    qint32 /*order count combine*/ order_dc{ 1 };
    qint32 /*nice value*/ order_dc_nice{ 0 };
    qint32 /*n*/ order_landmark_thresh{ 0 };
    qint32 /*n*/ order_landmark_start{ 0 };
    qint32 /*timeout secs*/ slippage_timeout{ 2 * 60000 };
    qreal /*offset scalar*/ market_offset{ 0. };
    bool /*bullish*/ market_sentiment{ false };

    // per-market settings
    Coin price_ticksize{ CoinAmount::SATOSHI }; // used to find new prices and pass binance filter PRICE_FILTER "tickSize"

    // Binance only - used to pass filter PERCENT_PRICE
    Coin price_min_mul{ 0.2 };
    Coin price_max_mul{ 5.0 };
    Coin quantity_ticksize{ CoinAmount::SATOSHI }; // used to pass filter LOT_SIZE "stepSize"

    // waves exchange
    Coin matcher_ticksize{ CoinAmount::SATOSHI };

    // inverse market tags
    bool is_tradeable{ false };
};

#endif // MARKET_H

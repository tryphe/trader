#ifndef SPRUCE_H
#define SPRUCE_H

#include "coinamount.h"
#include "costfunctioncache.h"

#include <QString>
#include <QMap>

struct Node
{
    QString currency;
    Coin price;
    Coin quantity;
    Coin amount;

    void recalculateAmountByQuantity() { amount = quantity * price; }
    void recalculateQuantityByPrice() { quantity = amount / price; }
};

struct RelativeCoeffs // tracks hi/lo coeffs with their corresponding markets
{
    explicit RelativeCoeffs()
    {
        lo_coeff = CoinAmount::A_LOT;
        hi_coeff = -CoinAmount::A_LOT;
    }

    QString hi_currency;
    QString lo_currency;
    Coin hi_coeff;
    Coin lo_coeff;
};

class Spruce
{
public:
    explicit Spruce();
    ~Spruce();

    void setBaseCurrency( QString currency ) { base_currency = currency; }
    QString getBaseCurrency() const { return base_currency; }
    void setCurrencyWeight( QString currency, Coin weight );
    Coin getMarketWeight( QString market ) const;

    void setTarget( Coin ratio ) { m_target = ratio; }

    void setOrderGreed( Coin ratio ) { m_order_greed = ratio; }
    void setOrderRandom( Coin r ) { m_order_greed_randomness = r; }
    Coin getOrderGreed();

    void setOrderNice( Coin nice ) { m_order_nice = nice; }
    Coin getOrderNice() const { return m_order_nice; }

    void setTrailingPriceLimit( Coin limit ) { m_trailing_price_limit = limit; }
    Coin getTrailingPriceLimit() const { return m_trailing_price_limit; }

    void addStartNode( QString _currency, QString _quantity, QString _price );
    void addLiveNode( QString _currency, QString _price );
    void clearLiveNodes();

    void calculateAmountToShortLong();
    Coin getAmountToShortLongNow( QString market );
    void addToShortLonged( QString market, Coin amount );

    QList<QString> getCurrencies() const;
    QList<QString> getMarkets() const;
    bool isActive();
    QString getSaveState();

    void setLongMax( Coin longmax ) { m_long_max = longmax; }
    Coin getLongMax() const { return m_long_max; }
    void setShortMax( Coin shortmax ) { m_short_max = shortmax; }
    Coin getShortMax() const { return m_short_max; }
    void setMarketMax( Coin marketmax ) { m_market_max = marketmax; }
    Coin getMarketMax( QString market = "" ) const;
    void setOrderSize( Coin ordersize ) { m_order_size = ordersize; }
    Coin getOrderSize( QString market = "" ) const;

    const RelativeCoeffs &startCoeffs() { return m_start_coeffs; }
    const RelativeCoeffs &relativeCoeffs() { return m_relative_coeffs; }
    const QMap<QString,Coin> &getAmountToShortLongMap() { return m_amount_to_shortlong_map; }
    const Coin &getAmountToShortLongTotal() { return m_amount_to_shortlong_total; }

    void setLeverage( Coin l ) { m_leverage = l; }
    Coin getLeverage() const { return m_leverage; }

    void setProfileU( QString currency, Coin u );
    Coin getProfileU( QString currency ) const { return m_currency_profile_u.value( currency, DEFAULT_PROFILE_U ); }

    void setReserve( QString currency, Coin r );
    Coin getReserve( QString currency ) const { return m_currency_reserve.value( currency, DEFAULT_RESERVE ); }

    Coin getEquityNow( QString currency );
    Coin getLastCoeffForMarket( const QString &market ) const;

private:
    void equalizeDates();
    void normalizeEquity();

    CostFunctionCache m_cost_cache;
    QMap<QString,Coin> m_currency_profile_u, m_currency_reserve;

    QMap<QString/*currency*/,Coin> getMarketCoeffs();
    RelativeCoeffs getRelativeCoeffs();

    RelativeCoeffs m_relative_coeffs, m_start_coeffs;
    QMap<QString,Coin> m_amount_to_shortlong_map;
    Coin m_amount_to_shortlong_total;

    QString base_currency;
    QMap<QString,Coin> currency_weight; // note: weights are >0 and <=1
    QMultiMap<Coin,QString> currency_weight_by_coin; // note: weights are >0 and <=1
    QMap<QString,Coin> shortlonged_total; // running total of shorted/longed coins
    QMap<QString,Coin> amount_to_shortlong; // amount to shortlong now based on total above
    QMap<QString,Coin> original_quantity; // track original start quantity, since it changes
    Coin m_target, m_order_greed, m_order_greed_randomness, m_long_max, m_short_max, m_market_max,
    m_order_size, m_order_size_min, m_order_nice, m_trailing_price_limit;

    QList<Node*> nodes_start, nodes_now;
    QMap<QString/*currency*/,Coin> m_last_coeffs;

    Coin m_leverage;
};

#endif // SPRUCE_H

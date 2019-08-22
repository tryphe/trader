#ifndef SPRUCE_H
#define SPRUCE_H

#include "coinamount.h"

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

    void setLeverage( Coin leverage ) { m_leverage = m_default_leverage = leverage; }
    void setLeverageCutoff( Coin cutoff, Coin leverage ) { m_leverage_cutoff.insert( cutoff, leverage ); }
    const Coin &getActiveLeverage() const { return m_leverage; }
    void setHedgeTarget( Coin ratio ) { m_hedge_target = ratio; }

    void setOrderGreed( Coin ratio ) { m_order_greed = ratio; }
    Coin getOrderGreed() { return m_order_greed; }

    void addStartNode( QString _currency, QString _quantity, QString _price );
    void addLiveNode( QString _currency, QString _price );
    void clearLiveNodes();

    void calculateAmountToShortLong();
    Coin getAmountToShortLongNow( QString market );
    void addToShortLonged( QString market, Coin amount );

    QList<QString> getCurrencies() const;
    QList<QString> getMarkets() const;
    bool isActive() { return !( base_currency.isEmpty() || nodes_start.isEmpty() || currency_weight.isEmpty() ); }
    QString getSaveState();

    void setLongMax( Coin longmax ) { m_long_max = longmax; }
    Coin getLongMax() const { return m_long_max; }
    void setShortMax( Coin shortmax ) { m_short_max = shortmax; }
    Coin getShortMax() const { return m_short_max; }
    void setMarketMax( Coin marketmax ) { m_market_max = marketmax; }
    Coin getMarketMax( QString market = "" ) const { return market.isEmpty() ? m_market_max : std::max( m_market_max * getMarketWeight( market ), m_market_max * Coin( "0.1" ) ); }
    void setOrderSize( Coin ordersize ) { m_order_size = ordersize; }
    Coin getOrderSize( QString market = "" ) const { return market.isEmpty() ? m_order_size : std::max( m_order_size * getMarketWeight( market ), m_order_size_min ); }

    const RelativeCoeffs &startCoeffs() { return m_start_coeffs; }
    const RelativeCoeffs &relativeCoeffs() { return m_relative_coeffs; }
    const QMap<QString,Coin> &getAmountToShortLongMap() { return m_amount_to_shortlong_map; }
    const Coin &getAmountToShortLongTotal() { return m_amount_to_shortlong_total; }

private:
    void equalizeDates();
    void normalizeEquity();

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
    QMap<Coin,Coin> m_leverage_cutoff;
    Coin m_default_leverage, m_leverage, m_hedge_target, m_order_greed, m_long_max, m_short_max, m_market_max, m_order_size, m_order_size_min;

    QList<Node*> nodes_start, nodes_now;
};

#endif // SPRUCE_H

#ifndef SPRUCE_H
#define SPRUCE_H

#include "coinamount.h"
#include "costfunctioncache.h"
#include "market.h"

#include <QString>
#include <QMap>
#include <QVector>

static const Coin DEFAULT_PROFILE_U = Coin("10");
static const Coin DEFAULT_RESERVE = Coin("0.05");

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

    void clear();

    void setIntervalSecs( const qint64 secs ) { m_interval_secs = secs; }
    qint64 getIntervalSecs() const { return m_interval_secs; }

    void setBaseCurrency( QString currency ) { base_currency = currency; }
    QString getBaseCurrency() const { return base_currency; }
    void setCurrencyWeight( QString currency, Coin weight );
    Coin getMarketWeight( QString market ) const;

    Coin getExchangeAllocation( const QString &exchange_market );
    void setExchangeAllocation( const QString &exchange_market_key, const Coin allocation );

    void setOrderGreed( Coin ratio ) { m_order_greed = ratio; }
    void setOrderRandomBuy( Coin r ) { m_order_greed_buy_randomness = r; }
    void setOrderRandomSell( Coin r ) { m_order_greed_sell_randomness = r; }
    Coin getOrderGreed( quint8 side );
    Coin getOrderGreed() { return m_order_greed; }
    Coin getOrderTrailingLimit( quint8 side ) { return m_order_greed - ( side == SIDE_BUY ? m_order_greed_buy_randomness : m_order_greed_sell_randomness ); }

    void setOrderNice( Coin nice ) { m_order_nice = nice; }
    Coin getOrderNice() const { return m_order_nice; }

    void setOrderNiceSpreadPut( Coin nice ) { m_order_nice_spreadput = nice.isZeroOrLess() ? m_order_nice_spreadput : nice; }
    Coin getOrderNiceSpreadPut() const { return m_order_nice_spreadput; }

    void setOrderNiceZeroBound( Coin nice ) { m_order_nice_zerobound = nice; }
    Coin getOrderNiceZeroBound() const { return m_order_nice_zerobound; }

    void setAgitator( Coin start, Coin stop, Coin increment );
    void runAgitator();

    void addStartNode( QString _currency, QString _quantity, QString _price );
    void addLiveNode( QString _currency, QString _price );
    void clearLiveNodes();
    void clearStartNodes();

    bool calculateAmountToShortLong();
    Coin getQuantityToShortLongNow( const QString &market );
    void addToShortLonged( const QString &market, const Coin &qty );

    QList<QString> getCurrencies() const;
    QList<QString> getMarkets() const;
    bool isActive();
    QString getSaveState();

    void setLongMax( Coin longmax ) { m_long_max = longmax; }
    Coin getLongMax() const { return m_long_max; }
    void setShortMax( Coin shortmax ) { m_short_max = shortmax; }
    Coin getShortMax() const { return m_short_max; }
    void setMarketBuyMax( Coin marketmax ) { m_market_buy_max = marketmax; }
    Coin getMarketBuyMax( QString market = "" ) const;
    void setMarketSellMax( Coin marketmax ) { m_market_sell_max = marketmax; }
    Coin getMarketSellMax( QString market = "" ) const;
    void setOrderSize( Coin ordersize ) { m_order_size = ordersize; }
    Coin getOrderSize( QString market = "" ) const;

    const RelativeCoeffs &startCoeffs() { return m_start_coeffs; }
    const RelativeCoeffs &relativeCoeffs() { return m_relative_coeffs; }
    const QMap<QString,Coin> &getQuantityToShortLongMap() { return m_quantity_to_shortlong_map; }

    Coin getCurrencyPriceByMarket( Market market );

    void setLeverage( Coin l ) { m_leverage = l; }
    Coin getLeverage() const { return m_leverage; }

    void setProfileU( QString currency, Coin u );
    Coin getProfileU( QString currency ) const { return m_currency_profile_u.value( currency, DEFAULT_PROFILE_U ); }

    void setReserve( QString currency, Coin r );
    Coin getReserve( QString currency ) const { return m_currency_reserve.value( currency, DEFAULT_RESERVE ); }

    Coin getEquityNow( QString currency );
    Coin getLastCoeffForMarket( const QString &market ) const;

    void setOrderDuplicity( bool dup, bool taker ) { order_duplicity = dup; taker_mode = taker; }
    bool getOrderDuplicity() const { return order_duplicity; }
    bool getTakerMode() const { return taker_mode; }

    static inline Coin getUniversalMinOrderSize()
    {
        return std::max( Coin( BITTREX_MINIMUM_ORDER_SIZE ),
                         std::max( Coin( BINANCE_MINIMUM_ORDER_SIZE ), Coin( POLONIEX_MINIMUM_ORDER_SIZE ) )
                         );
    }

private:
    bool normalizeEquity();
    bool equalizeDates();

    CostFunctionCache m_cost_cache;
    QMap<QString,Coin> m_currency_profile_u, m_currency_reserve;

    QMap<QString/*currency*/,Coin> getMarketCoeffs();
    RelativeCoeffs getRelativeCoeffs();

    RelativeCoeffs m_relative_coeffs, m_start_coeffs;
    QMap<QString,Coin> m_quantity_to_shortlong_map;

    QMap<QString,Coin> original_quantity; // track original start quantity, since it changes
    QMap<QString,Coin> quantity_already_shortlong; // running total of shorted/longed coins
    QMap<QString,Coin> quantity_to_shortlong; // amount to shortlong now based on total above

    QString base_currency;
    QMap<QString,Coin> currency_weight; // note: weights are >0 and <=1
    QMultiMap<Coin,QString> currency_weight_by_coin; // note: weights are >0 and <=1
    QMap<QString, Coin> per_exchange_market_allocations; // note: market allocations are 0:1
    Coin m_order_greed, m_order_greed_buy_randomness, m_order_greed_sell_randomness, m_long_max, m_short_max, m_market_buy_max,
    m_market_sell_max, m_order_size, m_order_nice, m_order_nice_spreadput, m_order_nice_zerobound;

    QList<Node*> nodes_start, nodes_now;
    QMap<QString,Node*> nodes_now_by_currency;
    QMap<QString/*currency*/,Coin> m_last_coeffs;
    QVector<QMap<QString/*currency*/,Coin>> m_qtys;

    Coin m_leverage;
    Coin m_leverage_start, m_leverage_stop, m_leverage_increment; // agitator variables
    qint64 m_interval_secs{ 60 * 2 }; // 2min default
    qint64 m_agitator_last_tick{ 0 }; // timestamp state for last agitator tick

    // spread settings
    bool order_duplicity{ false }; // todo: make a command to set this to true (good volume strat)
    bool taker_mode{ false }; // if duplicity and taker_mode is set, use bid price for sells, and ask price for buys
};

#endif // SPRUCE_H

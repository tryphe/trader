#ifndef SPRUCE_H
#define SPRUCE_H

#include "coinamount.h"
#include "costfunctioncache.h"
#include "market.h"

#include <QString>
#include <QMap>
#include <QMultiMap>
#include <QVector>
#include <QList>

static const Coin DEFAULT_PROFILE_U = Coin("10");
static const Coin DEFAULT_RESERVE = Coin("0.01");

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
    Coin getOrderRandomBuy() const { return m_order_greed_buy_randomness; }
    void setOrderRandomSell( Coin r ) { m_order_greed_sell_randomness = r; }
    Coin getOrderRandomSell() const { return m_order_greed_sell_randomness; }
    Coin getOrderGreedRandom( quint8 side ) const;
    Coin getOrderGreed() const { return m_order_greed; }
    void setOrderGreedMinimum( Coin ratio ) { m_order_greed_minimum = std::max( ratio, m_order_greed ); }
    Coin getOrderGreedMinimum() const { return m_order_greed_minimum; }
    Coin getOrderTrailingLimit( quint8 side ) const { return side == SIDE_BUY ? ( m_order_greed - m_order_greed_buy_randomness ) : CoinAmount::COIN - m_order_greed_sell_randomness; }
    void setOrderCancelMode( bool cancel_random ) { m_order_cancel_mode = cancel_random; }
    bool getOrderCancelMode() const { return m_order_cancel_mode; }

    void setOrderNice( const quint8 side, Coin nice ) { ( side == SIDE_BUY ) ? m_order_nice_buys = nice : m_order_nice_sells = nice ; }
    Coin getOrderNice( const quint8 side ) const { return ( side == SIDE_BUY ) ? m_order_nice_buys : m_order_nice_sells; }
    void setOrderNiceZeroBound( const quint8 side, Coin nice ) { ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys = nice : m_order_nice_zerobound_sells = nice; }
    Coin getOrderNiceZeroBound( const quint8 side ) const { return ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys : m_order_nice_zerobound_sells; }
    void setOrderNiceSpreadPut( const quint8 side, Coin nice ) { ( side == SIDE_BUY ) ? m_order_nice_spreadput_buys = nice : m_order_nice_spreadput_sells = nice; }
    Coin getOrderNiceSpreadPut( const quint8 side ) const { return ( side == SIDE_BUY ) ? m_order_nice_spreadput_buys : m_order_nice_spreadput_sells; }

    void setOrderNiceCustom( const quint8 side, Coin nice ) { ( side == SIDE_BUY ) ? m_order_nice_custom_buys = nice : m_order_nice_custom_sells = nice ; }
    Coin getOrderNiceCustom( const quint8 side ) const { return ( side == SIDE_BUY ) ? m_order_nice_custom_buys : m_order_nice_custom_sells; }
    void setOrderNiceCustomZeroBound( const quint8 side, Coin nice ) { ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys = nice : m_order_nice_custom_zerobound_sells = nice; }
    Coin getOrderNiceCustomZeroBound( const quint8 side ) const { return ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys : m_order_nice_custom_zerobound_sells; }

    void setSkew( Coin s ) { m_skew = s; }
    Coin getSkew() const { return m_skew; }

    void setAgitator( Coin start, Coin stop, Coin increment );
    void runAgitator();

    void addStartNode( QString _currency, QString _quantity, QString _price );
    void addLiveNode( QString _currency, QString _price );
    void addMarketBeta( Market m );
    void clearLiveNodes();
    void clearStartNodes();

    bool calculateAmountToShortLong();
    Coin getQuantityToShortLongNow( const QString &market );
    void addToShortLonged( const QString &market, const Coin &qty );

    QList<QString> getCurrencies() const;
    QList<QString> getMarketsAlpha() const;
    QList<Market> &getMarketsBeta() { return m_markets_beta; }
    bool isActive();
    QString getSaveState();

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

    void setAmplification( Coin l ) { m_amplification = l; }
    Coin getAmplification() const { return m_amplification; }

    void setProfileU( QString currency, Coin u );
    Coin getProfileU( QString currency ) const { return m_currency_profile_u.value( currency, DEFAULT_PROFILE_U ); }

    void setReserve( QString currency, Coin r );
    Coin getReserve( QString currency ) const { return m_currency_reserve.value( currency, DEFAULT_RESERVE ); }

    Coin getEquityAll();
    Coin getLastCoeffForMarket( const QString &market ) const;

    static inline Coin getUniversalMinOrderSize()
    {
        return std::max( std::max( Coin( WAVES_MINIMUM_ORDER_SIZE ), Coin( BITTREX_MINIMUM_ORDER_SIZE ) ),
                         std::max( Coin( BINANCE_MINIMUM_ORDER_SIZE ), Coin( POLONIEX_MINIMUM_ORDER_SIZE ) ) );
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
    Coin m_order_greed, m_order_greed_minimum, m_order_greed_buy_randomness, m_order_greed_sell_randomness, m_market_buy_max,
    m_market_sell_max, m_order_size, m_order_nice_buys, m_order_nice_sells, m_order_nice_zerobound_buys, m_order_nice_zerobound_sells,
    m_order_nice_spreadput_buys, m_order_nice_spreadput_sells, m_order_nice_custom_buys, m_order_nice_custom_sells,
    m_order_nice_custom_zerobound_buys, m_order_nice_custom_zerobound_sells, m_skew;

    QList<Node*> nodes_start, nodes_now;
    QMap<QString,Node*> nodes_now_by_currency;
    QMap<QString/*currency*/,Coin> m_last_coeffs;
    QVector<QMap<QString/*currency*/,Coin>> m_qtys;
    QList<Market> m_markets_beta;

    Coin m_amplification;
    Coin m_amplification_start, m_amplification_stop, m_amplification_increment; // agitator variables
    qint64 m_interval_secs{ 60 * 2 }; // 2min default
    qint64 m_agitator_last_tick{ 0 }; // timestamp state for last agitator tick
    bool m_order_cancel_mode{ false }; // false = cancel edges, true = cancel random
};

#endif // SPRUCE_H

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
#include <QDebug>

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

    void setOrderNice( const quint8 side, Coin nice, bool midspread_phase );
    Coin getOrderNice( const QString &market, const quint8 side, bool midspread_phase );

    void setOrderNiceZeroBound( const quint8 side, Coin nice, bool midspread_phase );
    Coin getOrderNiceZeroBound( const QString &market, const quint8 side, bool midspread_phase ) const;

    // spread reduction sensitivity
    void setOrderNiceSpreadPut( const quint8 side, Coin nice ) { ( side == SIDE_BUY ) ? m_order_nice_spreadput_buys = nice :
                                                                                        m_order_nice_spreadput_sells = nice; }
    Coin getOrderNiceSpreadPut( const quint8 side ) const { return ( side == SIDE_BUY ) ? m_order_nice_spreadput_buys :
                                                                                          m_order_nice_spreadput_sells; }

    // nice+zero bound offets for each market+side on all phases
    void setOrderNiceMarketOffset( const QString &market, const quint8 side, Coin offset ) { ( side == SIDE_BUY ) ? m_order_nice_market_offset_buys[ market ] = offset :
                                                                                                                    m_order_nice_market_offset_sells[ market ] = offset; }
    Coin getOrderNiceMarketOffset( const QString &market, const quint8 side ) { return ( side == SIDE_BUY ) ? m_order_nice_market_offset_buys.value( market ) :
                                                                                                              m_order_nice_market_offset_sells.value( market ); }
    void setOrderNiceZeroBoundMarketOffset( const QString &market, const quint8 side, Coin offset ) { ( side == SIDE_BUY ) ? m_order_nice_market_offset_zerobound_buys[ market ] = offset :
                                                                                                                             m_order_nice_market_offset_zerobound_sells[ market ] = offset; }
    Coin getOrderNiceZeroBoundMarketOffset( const QString &market, const quint8 side ) { return ( side == SIDE_BUY ) ? m_order_nice_market_offset_zerobound_buys.value( market ) :
                                                                                                                       m_order_nice_market_offset_zerobound_sells.value( market ); }

    // snapback settings
    void setSnapbackState( const QString &market, const quint8 side, const bool state );
    bool getSnapbackState( const QString &market, const quint8 side ) const;
    void setSnapbackRatio( const Coin &r ) { m_snapback_ratio = r; }
    Coin getSnapbackRatio() const { return m_snapback_ratio; }
    void setSnapbackExpiry( const qint64 secs ) { m_snapback_expiry_secs = secs; }
    qint64 getSnapbackExpiry() const { return m_snapback_expiry_secs; }

    void setOrdersPerSideFlux( quint16 orders ) { m_orders_per_side_flux = orders; }
    quint16 getOrdersPerSideFlux() const { return m_orders_per_side_flux; }

    void setOrdersPerSideMidspread( quint16 orders ) { m_orders_per_side_midspread = orders; }
    quint16 getOrdersPerSideMidspread() const { return m_orders_per_side_midspread; }

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
    m_order_nice_custom_zerobound_buys, m_order_nice_custom_zerobound_sells;
    QMap<QString, Coin> m_order_nice_market_offset_buys, m_order_nice_market_offset_sells, m_order_nice_market_offset_zerobound_buys,
    m_order_nice_market_offset_zerobound_sells;

    QList<Node*> nodes_start, nodes_now;
    QMap<QString,Node*> nodes_now_by_currency;
    QMap<QString/*currency*/,Coin> m_last_coeffs;
    QVector<QMap<QString/*currency*/,Coin>> m_qtys;
    QList<Market> m_markets_beta;

    // snapback settings
    QMap<QString, bool> m_snapback_state_buys, m_snapback_state_sells;
    QMap<QString, qint64> m_snapback_state_buys_expiry_secs, m_snapback_state_sells_expiry_secs;
    Coin m_snapback_ratio{ "0.1" }; // 0.1 default
    qint64 m_snapback_expiry_secs{ 60 * 60 * 24 }; // 1 day default

    // order scaling settings
    quint16 m_orders_per_side_flux{ 10 },
            m_orders_per_side_midspread{ 3 };

    Coin m_amplification;
    qint64 m_interval_secs{ 60 * 2 }; // 2min default
    bool m_order_cancel_mode{ false }; // false = cancel edges, true = cancel random
};

#endif // SPRUCE_H

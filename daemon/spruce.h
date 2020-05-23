#ifndef SPRUCE_H
#define SPRUCE_H

#include "coinamount.h"
#include "costfunctioncache.h"
#include "market.h"
#include "misctypes.h"

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
    friend class SpruceOverseer;

public:
    explicit Spruce();
    ~Spruce();

    void clear();

    void setIntervalSecs( const qint64 secs ) { m_interval_secs = secs; }
    qint64 getIntervalSecs() const { return m_interval_secs; }

    void setBaseCurrency( QString currency ) { base_currency = currency; }
    QString getBaseCurrency() const { return base_currency.isEmpty() ? "disabled" : base_currency; }
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

    // snapback state settings
    void setSnapbackState( const QString &market, const quint8 side, const bool state, const Coin price = Coin(), const Coin amount_to_shortlong_abs = Coin() );
    bool getSnapbackState( const QString &market, const quint8 side ) const;
    void setSnapbackRatio( const Coin &r ) { m_snapback_ratio = r; }
    Coin getSnapbackRatio() const { return m_snapback_ratio; }
    bool getSnapbackStateTrigger1( const QString &market, const quint8 side ) const { return ( ( side == SIDE_BUY ) ? m_snapback_trigger1_count_buys.value( market ) : m_snapback_trigger1_count_sells.value( market ) ) >= m_snapback_trigger1_iterations; }

    // snapback trigger 1 settings
    void setSnapbackTrigger1Window( const qint64 window ) { m_snapback_trigger1_time_window_secs = window; }
    qint64 getSnapbackTrigger1Window() const { return m_snapback_trigger1_time_window_secs; }
    void setSnapbackTrigger1Iterations( const qint64 iter ) { m_snapback_trigger1_iterations = iter; }
    qint64 getSnapbackTrigger1Iterations() const { return m_snapback_trigger1_iterations; }

    // snapback trigger 2 settings
    void setSnapbackTrigger2MASamples( const qint32 samples ) { m_snapback_trigger2_ma_samples = samples; }
    qint32 getSnapbackTrigger2MASamples() const { return m_snapback_trigger2_ma_samples; }
    void setSnapbackTrigger2MARatio( const Coin &ratio ) { m_snapback_trigger2_ma_ratio = ratio; }
    Coin getSnapbackTrigger2MARatio() const { return m_snapback_trigger2_ma_ratio; }
    void setSnapbackTrigger2InitialRatio( const Coin &ratio ) { m_snapback_trigger2_initial_ratio = ratio; }
    Coin getSnapbackTrigger2InitialRatio() const { return m_snapback_trigger2_initial_ratio; }
    void setSnapbackTrigger2MessageInterval( const qint64 interval ) { m_snapback_trigger2_message_interval = interval; }
    qint64 getSnapbackTrigger2MessageInterval() const { return m_snapback_trigger2_message_interval; }

    void setOrdersPerSideFlux( quint16 orders ) { m_orders_per_side_flux = orders; }
    quint16 getOrdersPerSideFlux() const { return m_orders_per_side_flux; }

    void setOrdersPerSideMidspread( quint16 orders ) { m_orders_per_side_midspread = orders; }
    quint16 getOrdersPerSideMidspread() const { return m_orders_per_side_midspread; }

    // order timeout settings
    void setOrderTimeoutFlux( quint16 minutes_min, quint16 minutes_max ) { m_order_timeout_flux_min = minutes_min;
                                                                           m_order_timeout_flux_max = minutes_max; }
    QPair<quint16,quint16> getOrderTimeoutFlux() const { return qMakePair( m_order_timeout_flux_min, m_order_timeout_flux_max ); }

    void setOrderTimeoutMidspread( quint16 minutes ) { m_order_timeout_midspread = minutes; }
    quint16 getOrderTimeoutMidspread() const { return m_order_timeout_midspread; }

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
    QMap<QString, Coin> m_currency_profile_u, m_currency_reserve;

    QMap<QString/*currency*/, Coin> getMarketCoeffs();
    RelativeCoeffs getRelativeCoeffs();

    RelativeCoeffs m_relative_coeffs, m_start_coeffs;
    QMap<QString, Coin> m_quantity_to_shortlong_map;

    QMap<QString, Coin> original_quantity; // track original start quantity, since it changes
    QMap<QString, Coin> quantity_already_shortlong; // running total of shorted/longed coins
    QMap<QString, Coin> quantity_to_shortlong; // amount to shortlong now based on total above

    QString base_currency;
    QMap<QString, Coin> currency_weight; // note: weights are >0 and <=1
    QMultiMap<Coin, QString> currency_weight_by_coin; // note: weights are >0 and <=1
    QMap<QString, Coin> per_exchange_market_allocations; // note: market allocations are 0:1
    Coin m_order_greed, m_order_greed_minimum, m_order_greed_buy_randomness, m_order_greed_sell_randomness, m_market_buy_max,
    m_market_sell_max, m_order_size, m_order_nice_buys, m_order_nice_sells, m_order_nice_zerobound_buys, m_order_nice_zerobound_sells,
    m_order_nice_spreadput_buys, m_order_nice_spreadput_sells, m_order_nice_custom_buys, m_order_nice_custom_sells,
    m_order_nice_custom_zerobound_buys, m_order_nice_custom_zerobound_sells;
    QMap<QString, Coin> m_order_nice_market_offset_buys, m_order_nice_market_offset_sells, m_order_nice_market_offset_zerobound_buys,
    m_order_nice_market_offset_zerobound_sells;

    QList<Node*> nodes_start, nodes_now;
    QMap<QString, Node*> nodes_now_by_currency;
    QMap<QString/*currency*/,Coin> m_last_coeffs;
    QVector<QMap<QString/*currency*/, Coin>> m_qtys;
    QList<Market> m_markets_beta;

    // snapback settings
    QMap<QString, bool> m_snapback_state_buys, m_snapback_state_sells;

    // note: trigger mechanism #1 has a time quotient window and a counter that's valid within the window and triggers above SNAPBACK_TRIGGER1_ITERATIONS
    QMap<QString, qint64> m_snapback_trigger1_timequotient_buys, m_snapback_trigger1_timequotient_sells,
                          m_snapback_trigger1_count_buys, m_snapback_trigger1_count_sells;

    // note: trigger mechanism #2 has an amount_to_sl_ma that triggers when it crosses under ma * SNAPBACK_TRIGGER2_RATIO
    QMap<QString, CoinMovingAverage> m_snapback_trigger2_sl_abs_ma_buys, m_snapback_trigger2_sl_abs_ma_sells;
    QMap<QString, Coin> m_snapback_trigger2_trigger_sl_abs_initial_buys, m_snapback_trigger2_trigger_sl_abs_initial_sells;

    // snapback mechanism settings
    Coin m_snapback_ratio{ "0.1" }; // snap back to x * nice value

    // snapback trigger 1 settings
    qint64 m_snapback_trigger1_time_window_secs{ 600 }; // we must reach the number of iterations below within this number of seconds to progress to trigger 2
    qint64 m_snapback_trigger1_iterations{ 5 };

    // snapback trigger 2 settings
    qint32 m_snapback_trigger2_ma_samples{ 450 }; // ma samples to use
    Coin m_snapback_trigger2_ma_ratio{ "0.80" }; // snapback triggers below this ma ratio
    Coin m_snapback_trigger2_initial_ratio{ "0.90" }; // OR snapback triggers below this sl abs ratio
    qint64 m_snapback_trigger2_message_interval{ 300 }; // debug message every x seconds

    // order scaling settings
    quint16 m_orders_per_side_flux{ 10 };
    quint16 m_orders_per_side_midspread{ 3 };

    // order timeout settings
    quint16 m_order_timeout_flux_min{ 60 };
    quint16 m_order_timeout_flux_max{ 90 };
    quint16 m_order_timeout_midspread{ 5 };

    Coin m_amplification;
    qint64 m_interval_secs{ 60 * 2 }; // 2min default
    bool m_order_cancel_mode{ false }; // false = cancel edges, true = cancel random
};

#endif // SPRUCE_H

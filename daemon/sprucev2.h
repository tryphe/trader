#ifndef SPRUCEV2_H
#define SPRUCEV2_H

#include "build-config.h"
#include "coinamount.h"
#include "market.h"
#include "misctypes.h"

#include <functional>

#include <QString>
#include <QMap>
#include <QMultiMap>
#include <QVector>
#include <QList>

struct BaseCapitalModulator
{
    explicit BaseCapitalModulator( const int slow_len, const int fast_len, const Coin &factor, const Coin &thresh )
    {
        ma_slow.setMaxSamples( slow_len );
        ma_fast.setMaxSamples( fast_len );
        base_row_modulation = factor;
        ma_slow_threshold = thresh;
    }

    Signal ma_slow;
    Signal ma_fast;
    Coin base_row_modulation;
    Coin ma_slow_threshold;
};

class SpruceV2 final
{
public:
    explicit SpruceV2();
    ~SpruceV2();

    void clear();

    void clearCurrentQtys();
    void setCurrentQty( const QString &currency, const Coin &qty ) { m_current_qty[ currency ] = qty; }

    void clearCurrentPrices();
    void setCurrentPrice( const QString &currency, const Coin &price ) { m_current_price[ currency ] = price; }
    Coin getCurrentPrice( const QString &currency ) const { return m_current_price.value( currency ); }
    QMap<QString, Coin> getCurrentPrices() { return m_current_price; }

    void clearSignalPrices();
    void setSignalPrice( const QString &currency, const Coin &signal_price ) { m_signal_price[ currency ] = signal_price; }
    QMap<QString, Coin> getSignalPrices() { return m_signal_price; }

    bool calculateAmountToShortLong(); // run to get amount to sl
    void doCapitalMomentumModulation( const Coin &base_capital ); // run every m_base_ma_length period

    Coin getQuantityToShortLongByCurrency( const QString &currency ) { return m_qty_to_sl.value( currency ); }
    Coin getQuantityToShortLongByMarket( const Market &market ) { return market.getBase() != getBaseCurrency() ? Coin() : m_qty_to_sl.value( market.getQuote() ); }
    QMap<QString, Coin> &getQuantityToShortLongMap() { return m_qty_to_sl; }

    void adjustCurrentQty( const QString &currency, const Coin &qty );

    void setAllocationFunction( const int index ) { m_allocation_function_index = std::min( std::max( 0, index ), m_allocaton_function_vec.size() -1 ); }
    int getAllocationFunctionIndex() const { return m_allocation_function_index; }

    void setVisualization( const bool enabled ) { m_visualize = enabled; }
    QString getVisualization();

    void setIntervalSecs( const qint64 secs ) { m_interval_secs = secs; }
    qint64 getIntervalSecs() const { return m_interval_secs; }

    void setBaseCurrency( QString currency ) { m_base_currency = currency; }
    QString getBaseCurrency() const { return m_base_currency.isEmpty() ? "disabled" : m_base_currency; }
    Coin getBaseCapital();

    void addBaseModulator( const BaseCapitalModulator &modulator ) { m_modulator += modulator; }

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

    void addMarketBeta( Market m );

    QVector<QString> getMarketsAlpha() const;
    QList<Market> &getMarketsBeta() { return m_markets_beta; }
    bool isActive();
    QString getSaveState();

    void setMarketBuyMax( Coin marketmax ) { m_market_buy_max = marketmax; }
    Coin getMarketBuyMax( QString market = "" ) const;
    void setMarketSellMax( Coin marketmax ) { m_market_sell_max = marketmax; }
    Coin getMarketSellMax( QString market = "" ) const;
    void setOrderSize( Coin ordersize ) { m_order_size = ordersize; }
    Coin getOrderSize( QString market = "" ) const;

    static inline Coin getUniversalMinOrderSize()
    {
        return std::max( std::max( Coin( WAVES_MINIMUM_ORDER_SIZE ), Coin( BITTREX_MINIMUM_ORDER_SIZE ) ),
                         std::max( Coin( BINANCE_MINIMUM_ORDER_SIZE ), Coin( POLONIEX_MINIMUM_ORDER_SIZE ) ) );
    }

private:
    /// new essential
    Coin allocationFunc0( const Coin &rp ) { return rp; } // y=x
    Coin allocationFunc1( const Coin &rp ) { return rp * rp; } // y=x^2
    Coin allocationFunc2( const Coin &rp ) { return rp + Coin("1"); } // y=x+1
    Coin allocationFunc3( const Coin &rp ) { return rp / ( rp + Coin("3") ); } // y=x/(x+3)
    Coin allocationFunc4( const Coin &rp ) { const Coin b = Coin("5") * rp; return ( rp * rp * rp ) - ( b * b ) +  ( Coin("9") * rp ); } // y=x^3 - 5x^2 + 9x
    Coin allocationFunc5( const Coin &rp ) { const Coin b = Coin("3") * rp; return ( rp * rp * rp ) - ( b * b ) +  ( Coin("3") * rp ); } // y=x^3 - 3x^2 + 3x
    Coin allocationFunc6( const Coin &rp ) { return rp * ( ( rp * rp ) - rp +  CoinAmount::COIN ); } // y=x^3 - x^2 + x
    Coin allocationFunc7( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.08") + rp * ( ( rp - CoinAmount::COIN ) * rp + Coin("0.3334") ) ); }
    Coin allocationFunc8( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.18") + rp * ( ( rp - CoinAmount::COIN ) * rp + Coin("0.3334") ) ); }
    Coin allocationFunc9( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.28") + rp * ( ( rp - CoinAmount::COIN ) * rp + Coin("0.3334") ) ); }
    Coin allocationFunc10( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.1") + rp * ( ( rp - CoinAmount::COIN ) * rp + Coin("0.3334") ) ); }
    Coin allocationFunc11( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.210") + rp * ( ( rp - CoinAmount::COIN ) * rp + Coin("0.3334") ) ); }
    Coin allocationFunc12( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.3") + rp * ( ( rp - CoinAmount::COIN ) * rp + Coin("0.3334") )); }
    Coin allocationFunc13( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.16") + rp * ( Coin("0.3333") + ( Coin("-1") + rp ) * rp ) ); }
    Coin allocationFunc14( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-0.68") + ( rp * rp ) ); }
    Coin allocationFunc15( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-1.02") + ( rp * rp ) ); }
    Coin allocationFunc16( const Coin &rp ) { return std::max( CoinAmount::SATOSHI, Coin("-1.36") + ( rp * rp ) ); }
    Coin allocationFunc17( const Coin &rp ) { return ( rp * rp ) + Coin("0.7"); }
    Coin allocationFunc18( const Coin &rp ) { return ( rp * rp ) + Coin("1.3"); }
    Coin allocationFunc19( const Coin &rp ) { return ( rp * rp ) + Coin("1.9"); }

    QString m_base_currency;

    QMap<QString/*currency*/, Coin> m_current_qty, m_qty_to_sl;
    QMap<QString/*currency*/, Coin> m_current_price, m_signal_price; // note: prices are in base

    QVector<BaseCapitalModulator> m_modulator;

    QVector<std::function<Coin(const Coin&)>> m_allocaton_function_vec;
    int m_allocation_function_index{ 0 };
    bool m_visualize{ false };
    ///

    /// non-essential, for external stats only
#ifndef SPRUCE_PERFORMANCE_TWEAKS_ENABLED
    QVector<QString> m_visualization_rows;
    QString m_visualized_row_cache;
#endif
    ///

    /// old spruce functionality, retain for SpruceOverseer compat
    QMap<QString, Coin> per_exchange_market_allocations; // note: market allocations are 0:1
    Coin m_order_greed, m_order_greed_minimum, m_order_greed_buy_randomness, m_order_greed_sell_randomness, m_market_buy_max,
    m_market_sell_max, m_order_size, m_order_nice_buys, m_order_nice_sells, m_order_nice_zerobound_buys, m_order_nice_zerobound_sells,
    m_order_nice_spreadput_buys, m_order_nice_spreadput_sells, m_order_nice_custom_buys, m_order_nice_custom_sells,
    m_order_nice_custom_zerobound_buys, m_order_nice_custom_zerobound_sells;
    QMap<QString, Coin> m_order_nice_market_offset_buys, m_order_nice_market_offset_sells, m_order_nice_market_offset_zerobound_buys,
    m_order_nice_market_offset_zerobound_sells;

    QList<Market> m_markets_beta;

    // snapback settings
    QMap<QString, bool> m_snapback_state_buys, m_snapback_state_sells;

    // note: trigger mechanism #1 has a time quotient window and a counter that's valid within the window and triggers above SNAPBACK_TRIGGER1_ITERATIONS
    QMap<QString, qint64> m_snapback_trigger1_timequotient_buys, m_snapback_trigger1_timequotient_sells,
                          m_snapback_trigger1_count_buys, m_snapback_trigger1_count_sells;

    // note: trigger mechanism #2 has an amount_to_sl_ma that triggers when it crosses under ma * SNAPBACK_TRIGGER2_RATIO
    QMap<QString, Signal> m_snapback_trigger2_sl_abs_ma_buys, m_snapback_trigger2_sl_abs_ma_sells;
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

    qint64 m_interval_secs{ 60 * 2 }; // 2min default
    ///
};

#endif // SPRUCEV2_H

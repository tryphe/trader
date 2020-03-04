#ifndef ENGINESETTINGS_H
#define ENGINESETTINGS_H

#include "global.h"
#include "coinamount.h"

struct EngineSettings
{
    explicit EngineSettings( const quint8 _engine_type )
    {
        if ( _engine_type == ENGINE_BITTREX )
        {
            fee = BITTREX_DEFAULT_FEERATE;
            safety_delay_time = BITTREX_SAFETY_DELAY;
            ticker_safety_delay_time = BITTREX_SAFETY_DELAY;
        }
        else if ( _engine_type == ENGINE_BINANCE )
        {
            fee = BINANCE_DEFAULT_FEERATE;
            safety_delay_time = BINANCE_SAFETY_DELAY;
            ticker_safety_delay_time = BINANCE_SAFETY_DELAY;
        }
        else if ( _engine_type == ENGINE_POLONIEX )
        {
            fee = POLONIEX_DEFAULT_FEERATE;
            safety_delay_time = POLONIEX_SAFETY_DELAY;
            ticker_safety_delay_time = POLONIEX_SAFETY_DELAY;
        }
    }
    ~EngineSettings() {}

    Coin fee;

    // global settings (probably shouldn't be modified)
    bool is_chatty{ false };
    bool should_clear_stray_orders{ true }; // auto cancels orders that aren't ours
    bool should_clear_stray_orders_all{ true }; // cancel orders not in our price index
    bool should_slippage_be_calculated{ true }; // calculated/additive preference of slippage
    bool should_adjust_hibuy_losell{ true }; // adjust hi_buy/lo_sell maps based on post-only price errors
    bool should_adjust_hibuy_losell_debugmsgs_ticker{ false }; // enable chatty messages for hi/lo bounds adjust for wss-ticker
    bool should_mitigate_blank_orderbook_flash{ true };
    bool should_dc_slippage_orders{ false };
    qint64 order_timeout{ 3 * 60000 }; // how long before we resend most requests
    qint64 cancel_timeout{ 5 * 60000 }; // how long before we resend a cancel request
    qint64 stray_grace_time_limit{ 10000 }; // how long before we cancel stray orders, if enabled
    qint64 safety_delay_time; // safety delay, should be more than your ping by a second or two
    qint64 ticker_safety_delay_time; // ^
};

#endif // ENGINESETTINGS_H

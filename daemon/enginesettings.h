#ifndef ENGINESETTINGS_H
#define ENGINESETTINGS_H

#include "global.h"
#include "coinamount.h"

struct EngineSettings
{
    explicit EngineSettings() {}
    ~EngineSettings() {}

    Coin fee{ DEFAULT_FEERATE };

    // global settings (probably shouldn't be modified)
    bool is_chatty{ false };
    bool should_clear_stray_orders{ false }; // auto cancels orders that aren't ours
    bool should_clear_stray_orders_all{ false }; // cancel orders not in our price index
    bool should_slippage_be_calculated{ true }; // calculated/additive preference of slippage
    bool should_adjust_hibuy_losell{ true }; // adjust hi_buy/lo_sell maps based on post-only price errors
    bool should_adjust_hibuy_losell_debugmsgs_ticker{ false }; // enable chatty messages for hi/lo bounds adjust for wss-ticker
    bool should_mitigate_blank_orderbook_flash{ true };
    bool should_dc_slippage_orders{ false };
    qint64 request_timeout{ 3 * 60000 }; // how long before we resend most requests
    qint64 cancel_timeout{ 5 * 60000 }; // how long before we resend a cancel request
    qint64 stray_grace_time_limit{ 10 * 60000 }; // how long before we cancel stray orders, if enabled
    qint64 safety_delay_time{ SAFETY_DELAY }; // safety delay, should be more than your ping by a second or two
    qint64 ticker_safety_delay_time{ SAFETY_DELAY }; // ^
};

#endif // ENGINESETTINGS_H

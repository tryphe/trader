#include "enginesettings.h"

EngineSettings::EngineSettings()
{
    // global settings (probably shouldn't be modified
    is_chatty = false;
    should_clear_stray_orders = false /*false*/; // auto cancels orders that aren't ours - set false when multiple bots are running the same api key
    should_clear_stray_orders_all = false; // cancel orders not in our price index
    should_adjust_hibuy_losell = true; // adjust hi_buy/lo_sell maps based on post-only price errors
    should_adjust_hibuy_losell_debugmsgs_ticker = false; // enable chatty messages for hi/lo bounds adjust for wss-ticker
    should_mitigate_blank_orderbook_flash = true;
    should_dc_slippage_orders = false;
    should_use_aggressive_spread = true;
    stray_grace_time_limit = 10 * 60000;

    // per exchange settings
#if defined(EXCHANGE_POLONIEX)
    fee = "0.0010"; // preset the fee, this is overriden by the timer
    request_timeout = 3 * 60000;  // how long before we resend most requests
    cancel_timeout = 5 * 60000;  // how long before we resend a cancel request
    should_slippage_be_calculated = true;  // try calculated slippage before additive. false = additive + additive2 only
    safety_delay_time = 2000;  // only detect a filled order after this amount of time - fixes possible orderbook lag
    ticker_safety_delay_time = 2000;
#elif defined(EXCHANGE_BITTREX)
    fee = "0.0025"; // preset the fee, not read by timer
    request_timeout = 3 * 60000;
    cancel_timeout = 5 * 60000;
    safety_delay_time = 8500;
    ticker_safety_delay_time = 8500;
#elif defined(EXCHANGE_BINANCE)
    fee = "0.00075"; // preset the fee, not read by timer (maybe an easy fix)
    request_timeout = 3 * 60000;
    cancel_timeout = 3 * 60000;
    should_slippage_be_calculated = true;
    safety_delay_time = 2000;
    ticker_safety_delay_time = 2000;
#endif
}

EngineSettings::~EngineSettings()
{
}

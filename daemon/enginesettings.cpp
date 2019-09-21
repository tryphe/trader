#include "enginesettings.h"
#include "build-config.h"

EngineSettings::EngineSettings()
{
    // per exchange settings
#if defined(EXCHANGE_POLONIEX)
    fee = "0.0010"; // preset the fee, this is overriden by the timer
#elif defined(EXCHANGE_BITTREX)
    fee = "0.0025"; // preset the fee, not read by timer
    safety_delay_time = 8500;
    ticker_safety_delay_time = 8500;
#elif defined(EXCHANGE_BINANCE)
    fee = "0.00075"; // preset the fee, not read by timer (maybe an easy fix)
#endif
}

EngineSettings::~EngineSettings()
{
}

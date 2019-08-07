#ifndef ENGINESETTINGS_H
#define ENGINESETTINGS_H

#include "coinamount.h"

#include <QObject>

class EngineSettings : public QObject
{
    Q_OBJECT
public:
    explicit EngineSettings();
    ~EngineSettings();

    Coin fee;

    bool is_chatty;
    bool should_clear_stray_orders;
    bool should_clear_stray_orders_all;
    bool should_slippage_be_calculated; // calculated/additive preference of slippage
    bool should_adjust_hibuy_losell;
    bool should_adjust_hibuy_losell_debugmsgs_ticker;
    bool should_mitigate_blank_orderbook_flash;
    bool should_dc_slippage_orders;
    bool should_use_aggressive_spread;
    qint64 request_timeout;
    qint64 cancel_timeout;
    qint64 stray_grace_time_limit;
    qint64 safety_delay_time;
    qint64 ticker_safety_delay_time;
};

#endif // ENGINESETTINGS_H

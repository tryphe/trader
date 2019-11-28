#ifndef POSITION_H
#define POSITION_H

#include "global.h"
#include "coinamount.h"
#include "market.h"

#include <QVector>

class Engine;

class Position
{
public:
    explicit Position( QString _market, quint8 _side, QString _buy_price, QString _sell_price,
                       QString _order_size, QString _strategy_tag = QLatin1String(),
                       QVector<qint32> _market_indices = QVector<qint32>(),
                       bool _landmark = false, Engine *_engine = nullptr );
    ~Position();

    void calculateQuantity();
    void flip();
    QString getFlippedPrice() { return side == SIDE_BUY ? sell_price_original : buy_price_original; }
    Coin getAmountFilled() const { return btc_amount - btc_amount_remaining; }
    bool applyPriceSide();
    void applyOffset();
    void applyOffset( qreal offset, bool sentiment = true );
    void jsonifyPositionFill( QJsonArray &arr );
    void jsonifyPositionSet( QJsonArray &arr );
    void jsonifyPositionCancel( QJsonArray &arr );
    QString stringifyOrder();
    QString stringifyOrderWithoutOrderID();
    QString stringifyNewPosition();
    QString stringifyPositionChange();
    QString sideStr() const { return side == SIDE_BUY  ? QString( BUY ) :
                                                         QString( SELL ); }
    qint32 getLowestMarketIndex() const;
    qint32 getHighestMarketIndex() const;

    // exchange data
    Market market; // BTC_CLAM...
    QString order_number;

    Coin quantity;

    quint8 side; // buy = 1, sell = 2
    quint8 cancel_reason;

    qint64 order_set_time; // 0 when not set
    qint64 order_request_time; // 0 when not requested
    qint64 order_cancel_time; // 0 when not cancelling
    qint64 order_getorder_time; // 0 or last getorder time

    // our position data
    QString indices_str;
    Coin price, buy_price, sell_price;
    Coin buy_price_original, sell_price_original;
    Coin original_size, btc_amount, btc_amount_remaining, per_trade_profit, profit_margin;
    quint32 price_reset_count;
    quint32 max_age_minutes; // how many minutes the order should exist for before we cancel it
    QString strategy_tag; // tag for short/long

    // track indices for market map
    QVector<qint32> market_indices;

    bool is_cancelling,
    is_landmark,
    is_slippage, // order has slippage
    is_new_hilo_order,
    is_onetime, // is a one-time order, ping-pong disabled
    is_taker, // is taker, post-only disabled
    is_spruce;

private:
    Engine *engine;
};


#endif // POSITION_H

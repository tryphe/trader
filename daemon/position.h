#ifndef POSITION_H
#define POSITION_H

#include "global.h"
#include "coinamount.h"

#include <QVector>

class Engine;

class Position
{
public:
    explicit Position( QString _market, quint8 _side, QString _price_lo, QString _price_hi,
                       QString _order_size, QVector<qint32> _market_indices = QVector<qint32>(),
                       bool _landmark = false, Engine *_engine = nullptr );
    ~Position();

    void calculateQuantity();
    void flip();
    bool applyPriceSide();
    void applyOffset();
    void applyOffset( qreal offset, bool sentiment = true );
    //QJsonObject jsonify();
    QString stringifyOrder();
    QString stringifyOrderWithoutOrderID();
    QString stringifyNewPosition();
    QString stringifyPositionChange();
    QString sideStr() const { return side == SIDE_BUY  ? QString( "buy" ) :
                                     side == SIDE_SELL ? QString( "sell" ) :
                                                         QString( "none" ); }
    qint32 getLowestMarketIndex() const;
    qint32 getHighestMarketIndex() const;

    // exchange data
    QString market, // BTC_CLAM...
            price,
            original_size,
            order_number;

    Coin quantity;

    quint8 side; // buy = 1, sell = 2
    quint8 cancel_reason;

    qint64 order_set_time; // 0 when not set
    qint64 order_request_time; // 0 when not requested
    qint64 order_cancel_time; // 0 when not cancelling
    qint64 order_getorder_time; // 0 or last getorder time

    // our position data
    QString indices_str;
    Coin price_lo, price_hi;
    QString price_lo_original, price_hi_original;
    Coin btc_amount, per_trade_profit, profit_margin;
    quint32 price_reset_count;
    quint32 max_age_minutes; // how many minutes the order should exist for before we cancel it

    // track indices for market map
    QVector<qint32> market_indices;

    bool is_cancelling,
    is_landmark,
    is_slippage, // order has slippage
    is_new_hilo_order,
    is_invalidated,
    is_onetime, // is a one-time order, ping-pong disabled
    is_taker; // is taker, post-only disabled

private:
    Engine *engine;
};

struct PositionData
{
    explicit PositionData() {}
    explicit PositionData( QString _price_lo, QString _price_hi, QString _order_size, QString _alternate_size )
    {
        price_lo = _price_lo;
        price_hi = _price_hi;
        order_size = _order_size;
        alternate_size = _alternate_size;
        fill_count = 0;
    }

    void incrementFillCount() { if ( ++fill_count == 1 && alternate_size.size() > 0 )
                                {
                                    order_size = alternate_size;
                                    alternate_size.clear();
                                }
                              }

    QString price_lo, price_hi, order_size, alternate_size;
    quint32 fill_count;
};


#endif // POSITION_H

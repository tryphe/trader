#ifndef POSITIONDATA_H
#define POSITIONDATA_H

#include <QString>

struct PositionData
{
    explicit PositionData() {}
    explicit PositionData( QString _buy_price, QString _sell_price, QString _order_size, QString _alternate_size )
    {
        buy_price = _buy_price;
        sell_price = _sell_price;
        order_size = _order_size;
        alternate_size = _alternate_size;
        fill_count = 0;
    }

    void iterateFillCount()
    {
        fill_count++;
        if ( !alternate_size.isEmpty() )
        {
            order_size = alternate_size;
            alternate_size.clear();
        }
    }

    QString buy_price, sell_price, order_size, alternate_size;
    quint32 fill_count;
};

#endif // POSITIONDATA_H

#ifndef SPRUCE_H
#define SPRUCE_H

#include "coinamount.h"

#include <QString>
#include <QMap>

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
    }

    QString hi_currency;
    QString lo_currency;
    Coin hi_coeff;
    Coin lo_coeff;
};

class Spruce
{
public:
    explicit Spruce();
    ~Spruce();

    void setBaseCurrency( QString currency ) { base_currency = currency; }
    QString getBaseCurrency() { return base_currency; }
    void setMarketWeight( QString market, Coin weight );
    Coin getMarketWeight( QString currency );

    void addStartNode( QString _currency, QString _quantity, QString _price );
    void addLiveNode( QString _currency, QString _price );
    void clearLiveNodes();

    void calculateAmountToShortLong();
    Coin getAmountToShortLongNow( QString market );
    void addToShortLonged( QString market, Coin amount );

    QList<QString> getCurrencies();
    QList<QString> getMarkets();
    bool isActive() { return !( base_currency.isEmpty() || nodes_start.isEmpty() || market_weight.isEmpty() ); }
    QString getSaveState();

private:
    void equalizeDates();
    void normalizeEquity();

    QMap<QString/*currency*/,Coin> getMarketCoeffs();
    RelativeCoeffs getHiLoCoeffs( QMap<QString,Coin> &coeffs );

    QString base_currency;
    QMap<QString,Coin> market_weight; // note: weights are >0 and <=1
    QMultiMap<Coin,QString> market_weight_by_coin; // note: weights are >0 and <=1
    QMap<QString,Coin> shortlonged_total; // running total of shorted/longed coins
    QMap<QString,Coin> amount_to_shortlong; // amount to shortlong now based on total above
    QMap<QString,Coin> original_quantity; // track original start quantity, since it changes

    QList<Node*> nodes_start, nodes_now;
};

#endif // SPRUCE_H

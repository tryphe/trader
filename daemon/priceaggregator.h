#ifndef PRICEAGGREGATOR_H
#define PRICEAGGREGATOR_H

#include "misctypes.h"

#include <QString>

class EngineMap;

class PriceAggregator
{
public:
    PriceAggregator();

    Spread getSpread( const QString &market );

private:
    EngineMap *m_engine_map{ nullptr };
};

#endif // PRICEAGGREGATOR_H

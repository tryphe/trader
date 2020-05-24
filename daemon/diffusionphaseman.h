#ifndef DIFFUSIONPHASEMAN_H
#define DIFFUSIONPHASEMAN_H

#include "global.h"

#include <QString>
#include <QVector>

enum DiffusionPhaseFluxType
{
    NO_FLUX,
    FLUX_PER_MARKET
};

class DiffusionPhaseMan final
{
public:
    DiffusionPhaseMan();
    DiffusionPhaseMan( QVector<QString> _markets );

    void addPhase( const DiffusionPhaseFluxType &_phase );
    DiffusionPhaseFluxType getCurrentPhase() const;
    QVector<QString> getCurrentPhaseMarkets();
    quint8 getCurrentPhaseSide() const { return m_side_selected; }

    void setMarkets( const QVector<QString> &_markets );
    QVector<QString> &getMarkets();
    int getMarketCount() const;

    void begin();
    void next();
    bool atEnd() const;

private:
    QVector<DiffusionPhaseFluxType> m_phases;
    QVector<QString> m_markets;
    int m_phase_selected{ 0 };
    int m_market_selected{ 0 };
    quint8 m_side_selected{ SIDE_BUY };
};

#endif // DIFFUSIONPHASEMAN_H

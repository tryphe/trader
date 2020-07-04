#include "diffusionphaseman.h"
#include "global.h"

#include <QString>
#include <QVector>

DiffusionPhaseMan::DiffusionPhaseMan() {}

DiffusionPhaseMan::DiffusionPhaseMan( QVector<QString> _markets)
{
    setMarkets( _markets );
}

void DiffusionPhaseMan::addPhase( const DiffusionPhaseFluxType &_phase )
{
    m_phases += _phase;
}

DiffusionPhaseFluxType DiffusionPhaseMan::getCurrentPhase() const
{
    return m_phases.value( m_phase_selected, NO_FLUX );
}

QVector<QString> DiffusionPhaseMan::getCurrentPhaseMarkets()
{
    if ( getCurrentPhase() == NO_FLUX )
        return m_markets;

    QVector<QString> ret;
    ret += m_markets.value( m_market_selected );
    return ret;
}

void DiffusionPhaseMan::setMarkets( const QVector<QString> &_markets )
{
    m_markets = _markets;
}

QVector<QString> &DiffusionPhaseMan::getMarkets()
{
    return m_markets;
}

int DiffusionPhaseMan::getMarketCount() const
{
    return m_markets.size();
}

void DiffusionPhaseMan::begin()
{
    m_phase_selected = 0;
    m_market_selected = 0;
    m_side_selected = SIDE_BUY;
}

void DiffusionPhaseMan::next()
{
    // if buy, flip to sell
    if ( m_side_selected == SIDE_BUY )
    {
        m_side_selected = SIDE_SELL;
    }
    // if sell, flip back to buy if we have more markets
    else if ( getCurrentPhase() == FLUX_PER_MARKET && m_market_selected < m_markets.size() -1 )
    {
        m_market_selected++;
        m_side_selected = SIDE_BUY;
    }
    // if we ran out of markets, switch to next phase
    else
    {
        m_phase_selected++;
        m_market_selected = 0;
        m_side_selected = SIDE_BUY;
    }
}

bool DiffusionPhaseMan::atEnd() const
{
    return m_phase_selected >= m_phases.size() || m_phase_selected < 0; // TODO: remove second condition when we use size_t
}

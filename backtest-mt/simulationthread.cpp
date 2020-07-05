#include "simulationthread.h"

#include "../daemon/alphatracker.h"
#include "../daemon/priceaggregator.h"
#include "../daemon/sprucev2.h"

#include <QString>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>

SimulationThread::SimulationThread( const int id )
    : QThread(),
      m_id( id )
{
}

SimulationThread::~SimulationThread()
{
}

void SimulationThread::run()
{
    assert( ext_mutex != nullptr );
    assert( ext_work_done != nullptr );
    assert( ext_work_queued != nullptr );
    assert( ext_work_count_total != nullptr );
    assert( ext_work_count_done != nullptr );

    kDebug() << QString( "[Thread %1] started" ).arg( m_id );

    ext_mutex->lock();
    while ( ext_work_queued->size() > 0 )
    {
        ++*ext_work_count_started;
        m_task_id = *ext_work_count_started;
        m_task = ext_work_queued->takeFirst();
        int tasks_total = *ext_work_count_total;
        ext_mutex->unlock();

        // run simulation on each map of price data
        for ( int i = 0; i < m_price_data.size(); i++ )
        {
            // print subsimulation if there are multiple
            if ( m_price_data.size() > 1 )
                kDebug() << QString( "[Thread %1] [%2 of %3] [Phase %4] started" )
                             .arg( m_id )
                             .arg( m_task_id )
                             .arg( tasks_total )
                             .arg( i );

            assert( m_price_data[ i ] != nullptr );
            runSimulation( m_price_data[ i ] );
        }

        ext_mutex->lock();
        ++*ext_work_count_done;
        ext_work_done->operator +=( m_task );
    }
    ext_mutex->unlock();

    kDebug() << QString( "[Thread %1] finished" ).arg( m_id );
}

void SimulationThread::runSimulation( const QMap<Market, PriceData> *const &price_data )
{
    static const int CANDLE_INTERVAL_SECS = 300; // 5 minutes per sample
    static const qint64 LATE_START_SAMPLES = 00000;
    static const Coin ORDER_SIZE_LIMIT = Coin( "0.005" );
    static const Coin FEE = -Coin( CoinAmount::SATOSHI * 200 );
    static const bool INVERT_RSI_MA = true;
    const Coin MAX_TAKE_PER_SIMULATION( Coin("0.001428571") * m_task->m_base_ma_length ); // we can only take so much btc per relative base length
    const int BASE_MA_LENGTH = m_task->m_base_ma_length; // copy to skip ptr walk

    // for score keeping
    Signal base_capital_sma0 = Signal( SMA, qint64( 1000 * 24 * 60 * 60 ) / ( BASE_MA_LENGTH * CANDLE_INTERVAL_SECS ) ); // 1000 days
    Signal base_capital_sma1 = Signal( SMA, qint64( 300 * 24 * 60 * 60 ) / ( BASE_MA_LENGTH * CANDLE_INTERVAL_SECS ) ); // 300 days
    Coin initial_btc_value, highest_btc_value;

    assert( BASE_MA_LENGTH > 0 );
    assert( m_task->m_rsi_length > 0 );
    assert( CANDLE_INTERVAL_SECS > 0 );
    assert( MAX_TAKE_PER_SIMULATION.isGreaterThanZero() );
    assert( FEE.isLessThanZero() );
    assert( m_price_data.size() > 0 );

    AlphaTracker alpha;
    SpruceV2 sp;

    // set new spruce capital modulation options
    for ( int i = 0; i < m_task->m_modulation_length_slow.size(); i++ )
        if ( m_task->m_modulation_length_slow.value( i ) > 0 && m_task->m_modulation_length_fast.value( i ) > 0 )
            sp.addBaseModulator( BaseCapitalModulator( m_task->m_modulation_length_slow[ i ], m_task->m_modulation_length_fast[ i ], m_task->m_modulation_factor[ i ], m_task->m_modulation_threshold[ i ] ) );

    /// set initial spruce config
    //sp.setVisualization( true );
    sp.setAllocationFunction( m_task->m_allocation_func );
    sp.setBaseCurrency( "BTC" );

    // start with ~0.2 btc value for each currency
    sp.clearCurrentQtys();
    sp.setCurrentQty( "BTC",   Coin( "0.2" ) );
    if ( price_data->contains( Market( "BTC_DASH" ) ) )
        sp.setCurrentQty( "DASH", Coin( "16.84" ) );
    if ( price_data->contains( Market( "BTC_ETH" ) ) )
        sp.setCurrentQty( "ETH",  Coin( "18.76" ) );
    if ( price_data->contains( Market( "BTC_LTC" ) ) )
        sp.setCurrentQty( "LTC",  Coin( "42.97" ) );
    if ( price_data->contains( Market( "BTC_USDT" ) ) )
        sp.setCurrentQty( "USDT", Coin( "156" ) );
    if ( price_data->contains( Market( "BTC_WAVES" ) ) )
        sp.setCurrentQty( "WAVES", Coin( "522.90" ) );
    if ( price_data->contains( Market( "BTC_XMR" ) ) )
        sp.setCurrentQty( "XMR",  Coin( "19.65" ) );
    if ( price_data->contains( Market( "BTC_ZEC" ) ) )
        sp.setCurrentQty( "ZEC",  Coin( "5" ) );

    /// step 1: initialize price data and fill signal samples while seeking ahead
    // clear members
    qint64 m_latest_ts = 0;
    QString latest_ts_market;

    /// calculate latest index-0 timestamp out of all markets
    for ( QMap<Market, PriceData>::const_iterator i = price_data->begin(); i != price_data->end(); i++ )
    {
        const Market &market = i.key();
        const PriceData &data = i.value();

        if ( data.data_start_secs <= m_latest_ts )
            continue;

        m_latest_ts = data.data_start_secs;
        latest_ts_market = market;
    }

    qint64 m_current_date_secs = m_latest_ts;
    QVector<qint64> m_current_idx;
    QVector<Signal> m_base_ma, m_price_ma, m_signal_ma;

    for ( int i = 0; i < price_data->size(); i++ )
    {
        m_current_idx += qint64( 0 );
        m_base_ma += Signal();
        m_price_ma += Signal();
        m_signal_ma += Signal();
    }

    /// step 2: construct current prices and signals loop from a common starting point m_latest_ts
    qint64 base_elapsed = 0;
    int market_i = -1;
    for ( QMap<Market, PriceData>::const_iterator i = price_data->begin(); i != price_data->end(); i++ )
    {
        ++market_i;

//        const Market &market = i.key();
        const PriceData &data = i.value();
        Signal &base_ma = m_base_ma[ market_i ];
        Signal &strategy_signal = m_signal_ma[ market_i ];
        Signal &price_signal = m_price_ma[ market_i ];
        qint64 &current_idx = m_current_idx[ market_i ];
        base_elapsed = m_current_date_secs;

        // init ma
        base_ma.setType( SMA ); // set base MA type
        base_ma.setMaxSamples( BASE_MA_LENGTH ); // maintain ma of the last m_base_ma_length samples

        // turn rsi_ma if non-zero
        if ( m_task->m_strategy_signal_type == RSI && m_task->m_rsi_ma_length > 0 )
            strategy_signal.setGeneralOption0( m_task->m_rsi_ma_length );
        strategy_signal.setType( m_task->m_strategy_signal_type );
        strategy_signal.setMaxSamples( m_task->m_rsi_length );

        price_signal.setType( SMA ); // set price MA type
        price_signal.setMaxSamples( BASE_MA_LENGTH );

        assert( base_ma.getSignal().isZero() &&
                strategy_signal.getSignal().isZero() &&
                price_signal.getSignal().isZero() );

        // init start data idx
        const qint64 start_idx = LATE_START_SAMPLES + ( ( m_latest_ts - data.data_start_secs ) / CANDLE_INTERVAL_SECS );
        current_idx = start_idx;

        // fill initial ma samples, ensure base_ma is full before we fill the first strategy sample. fill until
        strategy_signal.resetIntervalCounter( BASE_MA_LENGTH );
        while ( !strategy_signal.hasSignal() )
        {
            const Coin &sample = data.data.at( current_idx );
            // add to base ma
            base_ma.addSample( sample );

            // for every base sample, iterate strategy signal counter
            strategy_signal.iterateIntervalCounter();

            // add base ma to strategy ma every m_base_ma_length samples
            if ( strategy_signal.shouldUpdate() )
            {
                assert( base_ma.getSignal().isGreaterThanZero() );
                strategy_signal.resetIntervalCounter( BASE_MA_LENGTH );
                strategy_signal.addSample( base_ma.getSignal() );
            }

            current_idx++;
            base_elapsed += CANDLE_INTERVAL_SECS;
        }

        // reset strategy signal again to prepare for the next loop
        strategy_signal.resetIntervalCounter( m_task->m_base_ma_length );

        // ensure strategy signal has correct number of samples
//        if ( strategy_signal.getCurrentSamples() != m_task->m_rsi_length )
//        {
//            kDebug() << "strategy samples:" << strategy_signal.getCurrentSamples() << "rsi len" << m_task->m_rsi_length;
//        }
//        assert( strategy_signal.getCurrentSamples() == m_task->m_rsi_length );

//        kDebug() << QString( "%1  signal %2*%3=%4  start idx %5  current idx %6" )
//                     .arg( market, -10 )
//                     .arg( m_task->m_base_ma_length )
//                     .arg( strategy_signal.getCurrentSamples() )
//                     .arg( strategy_signal.getSignal(), -10 )
//                     .arg( start_idx, -7 )
//                     .arg( current_idx, -7 );
    }

    // update current timestamp
    m_current_date_secs = base_elapsed;

//    QDateTime start_date = m_current_date;
//    kDebug() << QString( "latest index-0 market %1  data start date %2(%3) simulation start date %4(%5)" )
//                 .arg( latest_ts_market )
//                 .arg( start_date.toString() )
//                 .arg( start_date.toSecsSinceEpoch() )
//                 .arg( m_current_date.toString() )
//                 .arg( m_current_date.toSecsSinceEpoch() );

    /// step 3: loop until out of data samples. each iteration: set price, update ma, run simulation
    qint64 total_samples = 0; // total sample count, not per-market
    int simulation_keeper = 0;
    Coin signal;
    const int END_TIME = qint64(1591574400) - ( ( ( BASE_MA_LENGTH / 2 ) +1 ) * CANDLE_INTERVAL_SECS );
    do
    {
        // check to run simulation this iteration
        const bool should_run_simulation = ++simulation_keeper % BASE_MA_LENGTH == 0;

        // reset to prevent overflow, also clear prices for each market
        if ( should_run_simulation )
        {
            total_samples += BASE_MA_LENGTH;
            simulation_keeper = 0;
            sp.clearCurrentPrices();
            sp.clearSignalPrices();
        }

        //kDebug() << "time:" << m_current_date.toString() << "run simulation:" << should_run_simulation;
        market_i = -1;
        for ( QMap<Market, PriceData>::const_iterator i = price_data->begin(); i != price_data->end(); i++ )
        {
            ++market_i;

            const Market &market = i.key();
            const PriceData &data = i.value();
            Signal &base_ma = m_base_ma[ market_i ];
            Signal &strategy_signal = m_signal_ma[ market_i ];
            Signal &price_signal = m_price_ma[ market_i ];
            qint64 &current_idx = m_current_idx[ market_i ];

            // read price
            const Coin &price = data.data.at( current_idx );
            const Coin &price_ahead = data.data.at( current_idx + ( BASE_MA_LENGTH / 2 ) );
            ++current_idx;

            assert( price_ahead.isGreaterThanZero() );

            // add signal samples
            base_ma.addSample( price );
            price_signal.addSample( price_ahead );

//            kDebug() << "adding base ma sample";

            // add base ma to strategy ma every m_base_ma_length samples
            strategy_signal.iterateIntervalCounter();
            if ( !strategy_signal.shouldUpdate() )
                continue;

            // if shouldUpdate() is true, should_run_simulation should also be true (only make a trade on a new sample)
            assert( should_run_simulation );

            strategy_signal.resetIntervalCounter( BASE_MA_LENGTH );
            strategy_signal.addSample( base_ma.getSignal() );

            /// fill price/signal price
            signal = strategy_signal.getSignal();

            // don't modulate signal unless the modulator has reached maximum samples
            if ( strategy_signal.isRSISMAPopulated() )
                signal /= ( INVERT_RSI_MA ) ? CoinAmount::COIN / strategy_signal.getRSISMA() :
                                              strategy_signal.getRSISMA();

            const QString &currency = market.getQuote();
            sp.setCurrentPrice( currency, price_signal.getSignal() );
            sp.setSignalPrice( currency, signal );

//            kDebug() << market << "rsi:" << strategy_signal.getSignal();
        }
        m_current_date_secs += CANDLE_INTERVAL_SECS;

        if ( !should_run_simulation )
            continue;

        // calculate ratio table
        if ( !sp.calculateAmountToShortLong() )
        {
            kDebug() << "calculateAmountToShortLong() failed";
            return;
        }

        // cache base capital
        const Coin base_capital = sp.getBaseCapital();
        if ( base_capital.isZeroOrLess() )
        {
            kDebug() << "ran out of capital!";
            return;
        }

        // note: only run this every m_base_ma_length * CANDLE_INTERVAL_SECS seconds
        sp.doCapitalMomentumModulation( base_capital );

        /// measure if we should make a trade, and add to alphatracker
        const QMap<QString, Coin> &qsl = sp.getQuantityToShortLongMap();
        //kDebug() << qsl;

        for ( QMap<QString, Coin>::const_iterator j = qsl.begin(); j != qsl.end(); j++ )
        {
            const QString &market = j.key();
            const Market m( market );

            const quint8 side = j.value().isGreaterThanZero() ? SIDE_SELL : SIDE_BUY;
            const Coin price = sp.getCurrentPrice( m.getQuote() );

            assert( price.isGreaterThanZero() );

            Coin qty_abs = j.value().abs();
            Coin amt_abs = qty_abs * price;
//            assert( !amt_abs.isLessThanZero() );

            // if non-actionable amount, skip
            if ( amt_abs < ORDER_SIZE_LIMIT )
                continue;

            // clamp by max amount of btc to take this simulation
            if ( amt_abs > MAX_TAKE_PER_SIMULATION )
            {
                amt_abs = MAX_TAKE_PER_SIMULATION;
                qty_abs = amt_abs / price;
                amt_abs = qty_abs * price;
            }

            assert( qty_abs.isGreaterThanZero() );
//            assert( amt_abs == qty_abs * price );

//            if ( side == SIDE_BUY )
//                kDebug() << "buying " << qty_abs << m.getQuote() << "for" << -amt_abs << m.getBase();
//            else
//                kDebug() << "selling" << -qty_abs << m.getQuote() << "for" << amt_abs << m.getBase();

            alpha.addAlpha( market, side, amt_abs, price );
            sp.adjustCurrentQty( m.getQuote(), side == SIDE_BUY ? qty_abs : -qty_abs );
            sp.adjustCurrentQty( m.getBase(), side == SIDE_BUY ? -amt_abs : amt_abs );
            sp.adjustCurrentQty( m.getBase(), FEE ); // subtract FEE
        }

        base_capital_sma0.addSample( base_capital ); // record score 0
        base_capital_sma1.addSample( base_capital ); // record score 1

        if ( base_capital > highest_btc_value ) // record score 3
        {
            highest_btc_value = base_capital;

            // initialize initial value, do it here as a small optimization
            if ( initial_btc_value.isZero() )
                initial_btc_value = base_capital;
        }

//        QMap<QString, Coin> prices = sp.getCurrentPrices();
//        QString prices_str;
//        for ( QMap<QString, Coin>::const_iterator i = prices.begin(); i != prices.end(); i++ )
//            prices_str += QString( " %1" ).arg( i.value() );

//        QMap<QString, Coin> signal_prices = sp.getSignalPrices();
//        QString signal_prices_str;
//        for ( QMap<QString, Coin>::const_iterator i = signal_prices.begin(); i != signal_prices.end(); i++ )
//            signal_prices_str += QString( " %1" ).arg( i.value() );
    }
    while ( m_current_date_secs < END_TIME );

    // set simulation id
    if ( m_task->m_simulation_id.isEmpty() )
    {
        QString modulation_str;
        for ( int i = 0; i < m_task->m_modulation_length_slow.size(); i++ )
        {
            if ( m_task->m_modulation_length_slow.value( i ) < 1 && m_task->m_modulation_length_fast.value( i ) < 1 )
                continue;

            modulation_str += QString( "-mod%1[%2/%3/%4/%5]" )
                               .arg( i )
                               .arg( m_task->m_modulation_length_slow.value( i ) )
                               .arg( m_task->m_modulation_length_fast.value( i ) )
                               .arg( m_task->m_modulation_factor.value( i ).toCompact() )
                               .arg( m_task->m_modulation_threshold.value( i ).toCompact() );
        }

        m_task->m_simulation_id = QString( "sig[%1]-rsi[%2/%3]-rsi_sma[%4%5]-func[%6]%7-maxtake[%8]-startamt[%9]-fee[%10]-pricema[%11]-[+%12]" )
                          .arg( m_task->m_strategy_signal_type )
                          .arg( BASE_MA_LENGTH )
                          .arg( m_task->m_rsi_length )
                          .arg( m_signal_ma.first().isRSISMAEnabled() ? m_task->m_rsi_ma_length : 0 )
                          .arg( INVERT_RSI_MA ? "I" : "" )
                          .arg( sp.getAllocationFunctionIndex() )
                          .arg( modulation_str )
                          .arg( MAX_TAKE_PER_SIMULATION.toCompact() )
                          .arg( initial_btc_value.toCompact() )
                          .arg( FEE.toCompact() )
                          .arg( BASE_MA_LENGTH )
                          .arg( LATE_START_SAMPLES );
    }

    // submit high score
    const Coin unique_value = CoinAmount::SUBSATOSHI * ( QDateTime::currentSecsSinceEpoch() - qint64(1592625713) );
    const Coin score_0 = CoinAmount::COIN * 10000 * ( base_capital_sma0.getSignal() / initial_btc_value ) / total_samples + unique_value;
    const Coin score_1 = CoinAmount::COIN * 10000 * ( base_capital_sma1.getSignal() / initial_btc_value ) / total_samples + unique_value;
    const Coin score_2 = score_1 / score_0 + unique_value;
    const Coin score_3 = highest_btc_value + unique_value;
    const Coin score_4 = sp.getBaseCapital() + unique_value;
//    kDebug() << score_0 << score_1 << score_2 << score_3 << score_4;

    // insert score
    m_task->m_scores[ 0 ] += score_0;
    m_task->m_scores[ 1 ] += score_1;
    m_task->m_scores[ 2 ] += score_2;
    m_task->m_scores[ 3 ] += score_3;
    m_task->m_scores[ 4 ] += score_4;

    // append alpha readout
    if ( !m_task->m_alpha_readout.isEmpty() )
        m_task->m_alpha_readout += '\n';

    m_task->m_alpha_readout += alpha.getAlphaReadout();
}

#include "simulationthread.h"
#include "tester.h"

#include "../daemon/coinamount.h"
#include "../daemon/misctypes.h"
#include "../daemon/priceaggregator.h"
#include "../daemon/sprucev2.h"

#include <QByteArray>
#include <QString>
#include <QDataStream>
#include <QDebug>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QCryptographicHash>

void SignalContainer::initSignals(const int signal_count)
{
    strategy_signals.clear();
    strategy_signals.resize( signal_count );

    price_signal.clear();
    current_idx = 0;
}

bool SignalContainer::isStrategyInitialized()
{
    const int STRATEGY_COUNT = strategy_signals.size();
    for ( int i = 0; i < STRATEGY_COUNT; i++ )
        if ( !strategy_signals[ i ].hasSignal() )
            return false;

    return true;
}

QByteArray &SimulationTask::getUniqueID()
{
    if ( !m_unique_id.isEmpty() )
        return m_unique_id;

    int i, j;
    QByteArray raw;
    QDataStream raw_s( &raw, QIODevice::WriteOnly );

    // stream basic args
    raw_s << Tester::BASE_INTERVAL;
    raw_s << Tester::PRICE_SIGNAL_LENGTH;
    raw_s << Tester::RELATIVE_SATS_TRADED_PER_BASE_INTERVAL;
    raw_s << Tester::WORK_SAMPLES_START_OFFSET;
    raw_s << m_allocation_func;
    assert( raw.size() == 17 );

    // assemble sorted strategy list to detect collisions from variant combinations of identical strategies
    QVector<QByteArray> strategy_list;
    for ( i = 0; i < m_strategy_args.size(); i++ )
    {
        QByteArray strat;
        QDataStream strat_s( &strat, QIODevice::WriteOnly );
        const StrategyArgs &args = m_strategy_args.at( i );

        strat_s << args.type;
        strat_s << args.length_fast;
        strat_s << args.length_slow;
        assert( strat.size() == 8 );

        if ( args.weight > CoinAmount::COIN ) // maintain hash compatability with version of code with no weights
        {
            strat += args.weight.toCompact(); // note: use += with strings
            assert( strat.size() > 9 );
        }

        strategy_list += strat;
    }
    std::sort( strategy_list.begin(), strategy_list.end() );

    // append sorted strategies
    for ( i = 0; i < strategy_list.size(); i++ )
        raw_s << strategy_list[ i ];

    // append base market followed by markets tested
    assert( !m_markets_tested.isEmpty() );
    assert( !m_markets_tested.first().isEmpty() );
    raw += m_markets_tested.first().first().getBase();
    for ( i = 0; i < m_markets_tested.size(); i++ )
    {
        raw += '.';
        for ( j = 0; j < m_markets_tested[ i ].size(); j++ )
            raw += m_markets_tested[ i ][ j ].getQuote();
    }

    // return sha3-256( ret )
    m_unique_id = QCryptographicHash::hash( raw, QCryptographicHash::Keccak_256 );
    return m_unique_id;
}

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
    static qint64 last_message_secs = 0;

    // init persistent thread stuff
    m_base_capital_sma0.setMaxSamples( qint64( 1500 * 24 * 60 * 60 ) / Tester::ACTUAL_CANDLE_INTERVAL_SECS ); // 1500 days

    assert( ext_mutex != nullptr );
    assert( ext_work_done != nullptr );
    assert( ext_work_queued != nullptr );
    assert( ext_work_count_total != nullptr );
    assert( ext_work_count_done != nullptr );

    kDebug() << QString( "[Thread %1] started" ).arg( m_id );

    ext_mutex->lock();
    while ( !ext_work_queued->isEmpty() )
    {
        // get new work, iterate work started
        m_work = ext_work_queued->takeFirst();
        ++*ext_work_count_started;

        // if work is infinite, subtract from work count
        if ( Tester::WORK_RANDOM && Tester::WORK_INFINITE )
            --*ext_work_count_total;

        // copy number of work units started/total so we can print them out of lock
        const int work_sequence = *ext_work_count_started;
        const int tasks_total = *ext_work_count_total;
        ext_mutex->unlock();

        // only print message every
        const qint64 current_secs = QDateTime::currentSecsSinceEpoch();
        if ( last_message_secs < current_secs - Tester::RESULTS_OUTPUT_THREAD_SECS )
        {
            last_message_secs = current_secs;

            kDebug() << QString( "[Thread %1] [%2 of %3]" )
                         .arg( m_id )
                         .arg( work_sequence )
                         .arg( Tester::WORK_RANDOM && Tester::WORK_INFINITE ? "inf" : QString( "%1" ).arg( tasks_total ) );
        }

        // run simulation on each map of price data
        for ( int i = 0; i < m_price_data.size(); i++ )
        {
            assert( m_price_data[ i ] != nullptr );
            runSimulation( m_price_data[ i ] );
        }

        // relock, iterate done counter, submit done work
        ext_mutex->lock();
        ++*ext_work_count_done;
        ext_work_done->operator +=( m_work );
    }
    ext_threads->removeOne( this );
    ext_mutex->unlock();

    kDebug() << QString( "[Thread %1] finished" ).arg( m_id );
}

void SimulationThread::runSimulation( const QMap<Market, PriceData> *const &price_data )
{
    static const Coin MINIMUM_ORDER_SIZE = Coin("0.01");
    static const int BASE_INTERVAL = Tester::BASE_INTERVAL;
    static const int PRICE_SIGNAL_OFFSET = Tester::PRICE_SIGNAL_LENGTH + Tester::PRICE_SIGNAL_BIAS;
    const int STRATEGY_COUNT = m_work->m_strategy_args.size();

#if defined(OUTPUT_SIMULATION_TIME)
    const qint64 t0 = QDateTime::currentMSecsSinceEpoch();
#endif

    static_assert( BASE_INTERVAL > 0 );
    static_assert( Tester::ACTUAL_CANDLE_INTERVAL_SECS > 0 );
    assert( m_price_data.size() > 0 );

    // init strategy signals
    m_signals.resize( price_data->size() );
    const QVector<SignalContainer>::const_iterator &m_signals_end = m_signals.end();
    for ( QVector<SignalContainer>::iterator i = m_signals.begin(); i != m_signals_end; i++ )
        i->initSignals( STRATEGY_COUNT );

    // construct signals string
    m_signals_str.clear();
    for ( int i = 0; i < STRATEGY_COUNT; i++ )
    {
        const StrategyArgs &args = m_work->m_strategy_args.value( i );

        QString current_signal = QString( "sig%1[%2]-" )
                                        .arg( i )
                                        .arg( args.operator QString() );

        m_signals_str += current_signal;
//        m_signals[ i ].filename = current_signal.replace( "/", "_" ).prepend( "signal_cache/" );

//        // if cache file exists, load cache
//        if ( QFile::exists( m_signals[ i ].filename ) )
//        {
//            m_signals[ i ].signal_is_cached = true;
//            PriceAggregator::loadPriceSamples( m_signal_cache[ m_signals[ i ].filename ], m_signals[ i ].filename );
//        }
    }

    // for score keeping
    m_base_capital_sma0.clear();
    initial_btc_value.clear();
    highest_btc_value.clear();
    simulation_cutoff_value.clear();
    total_volume.clear();

#if defined(PRICE_SIGNAL_CACHED_DEF)
    const bool should_use_price_signal_cache = !m_signals.first().price_signal_cache.isEmpty();
    uint32_t price_signal_cache_idx = 0;
#endif

//    AlphaTracker alpha;
    sp.clear();
    /// set initial spruce config
//    sp.setVisualization( true );
    sp.setAllocationFunction( m_work->m_allocation_func );
    sp.setBaseCurrency( "BTC" );

    // set new spruce capital modulation options (disabled for now)
//    for ( int i = 0; i < m_work->m_modulation_length_slow.size(); i++ )
//        if ( m_work->m_modulation_length_slow.value( i ) > 0 && m_work->m_modulation_length_fast.value( i ) > 0 )
//            sp.addBaseModulator( BaseCapitalModulator( m_work->m_modulation_length_slow[ i ], m_work->m_modulation_length_fast[ i ], m_work->m_modulation_factor[ i ], m_work->m_modulation_threshold[ i ] ) );

    // start with ~0.2 btc value for each currency
    //sp.clearCurrentQtys();
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
    m_latest_ts = 0;
//    QString latest_ts_market;
    /// calculate latest index-0 timestamp out of all markets
    const auto &price_data_end0 = price_data->end();
    for ( QMap<Market, PriceData>::const_iterator i = price_data->begin(); i != price_data_end0; i++ )
    {
//        const Market &market = i.key();
        const PriceData &data = i.value();

        if ( data.data_start_secs <= m_latest_ts )
            continue;

        m_latest_ts = data.data_start_secs;
//        latest_ts_market = market;
    }
#if defined(PRICE_SIGNAL_CACHED_DEF)
    // we are filling the cache, store latest ts
    if ( !should_use_price_signal_cache )
        m_latest_ts_price_cache = m_latest_ts;
#endif

    /// step 2: construct current prices and signals loop from a common starting point m_latest_ts
#if defined(REPAIR_INDEX_FRAGMENTATION)
    QVector<qint64> indices_elapsed;
    indices_elapsed.resize( m_signals.size() );
#endif
    int market_i = -1, j;

    for ( QMap<Market, PriceData>::const_iterator i = price_data->begin(); i != price_data_end0; i++ )
    {
        ++market_i;

//        const Market &market = i.key();
        const PriceData &data = i.value();
        SignalContainer &container = m_signals[ market_i ];
        QVector<PriceSignal> &strategy_signals = container.strategy_signals;
        PriceSignal &price_signal = container.price_signal;
        qint64 &current_idx = container.current_idx;

        // init start data idx
        current_idx = Tester::WORK_SAMPLES_START_OFFSET + ( m_latest_ts - data.data_start_secs ) / Tester::ACTUAL_CANDLE_INTERVAL_SECS;

//        kDebug() << i.key() << "start idx" << current_idx;

        // init price signal
        price_signal.setMaxSamples( Tester::PRICE_SIGNAL_LENGTH );
        assert( price_signal.getSignal().isZero() );

        j = 0;
        const auto &strategy_signals_end = strategy_signals.end();
        for ( QVector<PriceSignal>::iterator it = strategy_signals.begin(); it != strategy_signals_end; ++it )
        {
            const StrategyArgs &args = m_work->m_strategy_args.value( j++ );
            PriceSignal &strategy_signal = *it;

            strategy_signal.setSignalArgs( args.type, args.length_fast, args.length_slow, args.weight );
//            strategy_signal.setCounterMax( Tester::SIGNAL_INTERVAL );
            assert( strategy_signal.getSignal().isZero() );
        }

        // fill all signals until strategy signals are non-zero
        while ( !container.isStrategyInitialized() )
        {
            // check for end
            const int price_ahead_idx = current_idx + PRICE_SIGNAL_OFFSET;
            if ( price_ahead_idx == data.data.size() )
            {
                // print signals config so we can reproduce it later
                kDebug() << "warning: reached end before signals could be initialized" << m_signals_str;
                return;
            }

            // read price, iterate index
            const Coin &price = data.data[ current_idx ];
            const Coin &price_ahead = data.data[ price_ahead_idx ];
            ++current_idx;
#if defined(REPAIR_INDEX_FRAGMENTATION)
            ++indices_elapsed[ market_i ];
#endif

            // add to price signal
            price_signal.addSample( price_ahead );

            // add to strategy signals
            for ( auto it = strategy_signals.begin(); it != strategy_signals_end; ++it )
            {
                const StrategyArgs &args = m_work->m_strategy_args.value( j++ );
                auto &strategy_signal = *it;

                strategy_signal.addSample( price );
            }
        }
    }

#if defined(REPAIR_INDEX_FRAGMENTATION)
    /// index fragmentation check, check that the number of indices elapsed for each market is the same
    /// only necessary for buggy signal lengths.
    /// TODO: fix this. this should actually be done in the loop above and the samples skipped here should
    ///       be incorporated into the signals rather than skipped.
    qint64 elapsed_min = std::numeric_limits<qint64>::max(), elapsed_max = 0;
    const auto &indices_elapsed_end = indices_elapsed.end();
    for ( QVector<qint64>::const_iterator i = indices_elapsed.begin(); i != indices_elapsed_end; i++ )
    {
        const qint64 &elapsed = *i;

        if ( elapsed > elapsed_max )
            elapsed_max = elapsed;

        if ( elapsed < elapsed_min )
            elapsed_min = elapsed;
    }

    /// repair index fragmentation if needed
    if ( elapsed_min != elapsed_max )
    {
        kDebug() << "warning: repairing index fragmentation" << indices_elapsed << "for work id" << m_work->getUniqueID().toHex();

        market_i = -1;
        for ( QVector<qint64>::iterator i = indices_elapsed.begin(); i != indices_elapsed.end(); i++ )
        {
            ++market_i;
            qint64 &elapsed = *i;

            if ( elapsed < elapsed_max )
            {
                m_signals[ market_i ].current_idx += elapsed_max - elapsed;
                elapsed += elapsed_max - elapsed;
            }
        }

        kDebug() << "warning: repaired index fragmentation" << indices_elapsed;
    }
#endif

    /// step 3: loop until out of data samples. each iteration: set price, update ma, run simulation
    Coin ORDER_SIZE_LIMIT, FEE;
    int price_ahead_idx;
    bool at_end = false;
    qint64 total_samples = 0; // total sample count, not per-market
    Coin base_capital, qty_abs, amt_abs, price_signal_value;
    QVector<Coin> signal_values, take_price;
    const QString base_currency = sp.getBaseCurrency();
    QMap<Market, PriceData>::const_iterator price_it;
    const QMap<Market, PriceData>::const_iterator &price_data_begin = price_data->begin();
    const QMap<Market, PriceData>::const_iterator &price_data_end = price_data->end();
    QMap<QString, Coin>::const_iterator qsl_it;

    take_price.resize( m_signals.size() );

    while ( !at_end )
    {
//        sp.clearCurrentAndSignalPrices();
        sp.clearCurrentPrices();
        total_samples += BASE_INTERVAL;

        //kDebug() << "time:" << m_current_date.toString() << "run simulation:" << should_run_simulation;
        market_i = -1;
#if defined(PRICE_SIGNAL_CACHED_DEF)
        const qint64 cache_seek_ts = m_latest_ts + price_signal_cache_idx * Tester::ACTUAL_CANDLE_INTERVAL_SECS;
#endif
        for ( price_it = price_data_begin; price_it != price_data_end; price_it++ )
        {
            ++market_i;

            const Market &market = price_it.key();
            const auto &data = price_it.value().data;
            SignalContainer &container = m_signals[ market_i ];
            QVector<PriceSignal> &strategy_signals = container.strategy_signals;
            PriceSignal &price_signal = container.price_signal;
            qint64 &current_idx = container.current_idx;

            // look ahead at price
            price_ahead_idx = current_idx + PRICE_SIGNAL_OFFSET;

            // read price, iterate index
            const Coin &price = data[ current_idx++ ];
            const Coin &price_ahead = data[ price_ahead_idx ];

            assert( price_ahead.isGreaterThanZero() );

            // add price signal sample
            price_signal.addSample( price_ahead );

//            kDebug() << "current price" << price << "ahead" << price_ahead << "ahead sig" << price_signal.getSignal();

            // add strategy sample
            signal_values.clear();
            const auto &strategy_signals_end = strategy_signals.end();
            for ( QVector<PriceSignal>::iterator i = strategy_signals.begin(); i != strategy_signals_end; i++ )
            {
                // add price sample to signal
                PriceSignal &strategy_signal = *i;
                strategy_signal.addSample( price );

                // add signal value to signals
                const Coin &signal_value = strategy_signal.getSignal();
                if ( signal_value.isZeroOrLess() )
                {
                    kDebug() << "warning: aborted simulation on bad strategy signal value:" << signal_value;
                    return;
                }

                signal_values += signal_value;
            }

            // push price and signal values
            const QString &currency = market.getQuote();
            sp.setCurrentPrice( currency, price );//, signal_values );

#if defined(PRICE_SIGNAL_CACHED_DEF)
            //kDebug() << ( should_use_price_signal_cache ? "using cache" : "not using cache" );
            if ( should_use_price_signal_cache && cache_seek_ts >= m_latest_ts_price_cache )
            {
                price_signal_value = container.price_signal_cache.at( price_signal_cache_idx );
//                kDebug() << "loaded signal for ts" << cache_seek_ts << price_signal_value;
            }
            else
            {
#endif
                price_signal_value = price_signal.getSignal();
#if defined(PRICE_SIGNAL_CACHED_DEF)
                container.price_signal_cache += price_signal_value;
//                kDebug() << "wrote signal for ts" << cache_seek_ts << price_signal_value;
            }
#endif
            take_price[ market_i ] = price_signal_value;

            // check for end of data
            if ( price_ahead_idx == data.size() -1 )
                at_end = true;
        }
#if defined(PRICE_SIGNAL_CACHED_DEF)
        price_signal_cache_idx++;
#endif
        // calculate ratio table
        if ( !sp.calculateAmountToShortLong() )
        {
            kDebug() << "calculateAmountToShortLong() failed";
            return;
        }

        // cache base capital
        base_capital = sp.getBaseCapital();
        if ( base_capital < simulation_cutoff_value )
        {
#if defined(OUTPUT_SIMULATION_CUTOFF_WARNING)
            kDebug() << "warning: ran out of capital" << m_signals_str;
#endif
            return;
        }

        // note: only run this every base_signal_length * Tester::ACTUAL_CANDLE_INTERVAL_SECS seconds
//        if ( sp.getBaseModulatorCount() > 0 )
//            sp.doCapitalMomentumModulation( base_capital );

//        kDebug() << sp.getVisualization();

        // we can only take so much btc per relative base length, per market. 0.05% limit, 0.05%/market_count max take
        static const Coin PRECOMPUTED_ORDERSIZE_PARAMS = CoinAmount::SATOSHI * Tester::RELATIVE_SATS_TRADED_PER_BASE_INTERVAL * BASE_INTERVAL;
        ORDER_SIZE_LIMIT = ( base_capital * PRECOMPUTED_ORDERSIZE_PARAMS ) / m_signals.size();

        // calculate dynamic fee based on relative sizing
        FEE = base_capital / Tester::FEE_DIV;

        /// measure if we should make a trade, and add to alphatracker
//        const QMap<QString, Coin> &current_prices = sp.getCurrentPrices();
        const QMap<QString, Coin> &qsl = sp.getQuantityToShortLongMap();
        const QMap<QString, Coin>::const_iterator &qsl_end = qsl.end();
        //kDebug() << qsl;
        market_i = -1;
        for ( qsl_it = qsl.begin(); qsl_it != qsl_end; qsl_it++ )
        {
            ++market_i;

            const Coin &qty = qsl_it.value();

            if ( qty.isZero() )
                continue;

            const Coin &price = take_price[ market_i ];

            qty_abs = qty.abs();
            amt_abs = qty_abs * price;

            // continue if actionable amount is under minimum order size
            if ( amt_abs < MINIMUM_ORDER_SIZE )
                continue;

            const Coin &current_amt = sp.getCurrentQty( base_currency );

            if ( amt_abs < ORDER_SIZE_LIMIT && // if non-actionable amount, skip
                 current_amt < MINIMUM_ORDER_SIZE ) // but only if current amount is under the min order size (allow liquidiation of leftovers that are under the limit)
                continue;

            // clamp by max amount of btc to take this simulation
            if ( amt_abs > ORDER_SIZE_LIMIT )
            {
                amt_abs = ORDER_SIZE_LIMIT;
                qty_abs = amt_abs / price;
                amt_abs = qty_abs * price;
            }

            const QString &market_str = qsl_it.key();
            const Market m( market_str );
            const QString &quote = m.getQuote();
            const quint8 side = qty.isGreaterThanZero() ? SIDE_SELL : SIDE_BUY;
            const Coin &current_qty = sp.getCurrentQty( quote );

            // apply balance constraints
            if ( side == SIDE_SELL && current_qty < qty_abs )
            {
                if ( current_qty.isGreaterThanZero() )
                {
                    qty_abs = current_qty;
                    amt_abs = qty_abs * price;
                }
                else
                {
                    continue;
                }
            }
            else if ( side == SIDE_BUY && current_amt < amt_abs )
            {
                if ( current_amt.isGreaterThanZero() )
                {
                    amt_abs = current_amt;
                    qty_abs = amt_abs / price;
                }
                else
                {
                    continue;
                }
            }

//            if ( side == SIDE_BUY )
//                kDebug() << "buying " << qty_abs << m.getQuote() << "for" << -amt_abs << m.getBase();
//            else
//                kDebug() << "selling" << -qty_abs << m.getQuote() << "for" << amt_abs << m.getBase();

            // add fee. when we buy, spend slightly more btc than than the trade, to incorporate the fee
            if ( side == SIDE_BUY && current_amt >= amt_abs + FEE )
                amt_abs += FEE;

            assert( qty_abs.isGreaterThanZero() );
            assert( amt_abs.isGreaterThanZero() );

//            alpha.addAlpha( market_str, side, amt_abs, price );
            total_volume += amt_abs;
            sp.adjustCurrentQty( quote,         side == SIDE_BUY ?  qty_abs : -qty_abs );
            sp.adjustCurrentQty( base_currency, side == SIDE_BUY ? -amt_abs :  amt_abs );
        }

        m_base_capital_sma0.addSample( base_capital ); // record score 0

        if ( base_capital > highest_btc_value ) // record score 3
        {
            highest_btc_value = base_capital;

            // initialize initial value, do it here as a small optimization
            if ( initial_btc_value.isZero() )
            {
                initial_btc_value = base_capital;
                simulation_cutoff_value = base_capital / 2;
            }
        }

#if defined(OUTPUT_BASE_CAPITAL)
//        static PriceSignal sma = PriceSignal( SMA, 1051 );
//        Coin sample = base_capital / initial_btc_value;
//        sma.addSample( sample );
//        kDebug() << sma.getSignal();
        kDebug() << base_capital / initial_btc_value;
#endif
    }

    if ( Tester::OUTPUT_QTY_TARGETS )
    {
        QFile savefile( "qtytargets" );
        if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
        {
            kDebug() << "local error: couldn't open qtytargets for writing";
            return;
        }

        QTextStream out_savefile( &savefile );

        // save base capital
        out_savefile << QString( "%1\n" )
                         .arg( base_capital );

        // save each target
        QMap<QString, Coin> &qty_map = sp.getCurrentQtyMap();
        const QMap<QString, Coin>::const_iterator &qty_map_end = qty_map.end();
        for ( QMap<QString, Coin>::const_iterator i = qty_map.begin(); i != qty_map_end; i++ )
        {
            QString currency = i.key();
            const Coin &qty = i.value();

            if ( currency == "USDT" )
                currency = "USDN";

            out_savefile << QString( "%1 %2\n" )
                             .arg( currency )
                             .arg( qty );
        }

        // write extra line break to signify that nothing was truncated
        out_savefile << "\n";

        // save the buffer
        out_savefile.flush();
        savefile.close();

        kDebug() << "wrote qty targets";

        // print current/total ratios
        kDebug() << "currency allocation ratios:";
        for ( QMap<QString, Coin>::const_iterator i = qty_map.begin(); i != qty_map.end(); i++ )
        {
            const QString &currency = i.key();
            const Coin &qty = i.value();

            if ( currency == sp.getBaseCurrency() )
                kDebug() << currency << qty / base_capital;
            else
                kDebug() << currency << qty * sp.getCurrentPrice( currency ) / base_capital;
        }

        kDebug() << "total volume:" << total_volume;
    }

    if ( sp.getVisualizationState() )
    {
//        kDebug() << sp.getVisualization();
//        kDebug() << alpha.getAlphaReadout();

//#ifndef SPRUCE_PERFORMANCE_TWEAKS_ENABLED
//        QMap<QString, Coin> &qty_map_target = sp.getQSLTarget();
//        const int current_weight = 1;
//        const int target_weight = 6;
//        const int total_weight = current_weight + target_weight;

//        kDebug() << "current qtys adjusted between current size and target portfolio size" << target_portfolio_size.toCompact() << ":";
//        for ( QMap<QString, Coin>::const_iterator i = qty_map_target.begin(); i != qty_map_target.end(); i++ )
//        {
//            const QString &currency = i.key();
//            const Coin &qty_target = i.value();
//            const Coin &qty_current = qty_map.value( currency );

//            kDebug() << currency << ( ( qty_target  * ( target_portfolio_size / base_capital ) * target_weight ) +
//                                      ( qty_current * ( target_portfolio_size / base_capital ) * current_weight ) ) / ( total_weight );
//        }
//#endif
    }

    // set simulation id if empty (if we have multiple market variations, prevent from executing multiple times)
    if ( m_work->m_simulation_result.isEmpty() )
    {
        m_work->m_simulation_result = QString( "%1func[%2]-take[%3]-startamt[%4]-fee[%5]-candlelen[%6]-pricelen[%7]-pricebias[%8]-[+%9]" )
                          .arg( m_signals_str )
                          .arg( sp.getAllocationFunctionIndex() )
                          .arg( Tester::RELATIVE_SATS_TRADED_PER_BASE_INTERVAL )
                          .arg( initial_btc_value.toCompact() )
                          .arg( Tester::FEE_DIV )
                          .arg( Tester::ACTUAL_CANDLE_INTERVAL_SECS )
                          .arg( Tester::PRICE_SIGNAL_LENGTH )
                          .arg( Tester::PRICE_SIGNAL_BIAS )
                          .arg( Tester::WORK_SAMPLES_START_OFFSET );
    }

    // insert scores
    const Coin unique_value = CoinAmount::SUBSATOSHI * ( QDateTime::currentSecsSinceEpoch() - qint64(1592625713) );
    m_work->m_scores += CoinAmount::COIN * 100000 * ( m_base_capital_sma0.getSignal() / initial_btc_value ) / total_samples + unique_value;
    m_work->m_scores += highest_btc_value / initial_btc_value + unique_value;
    m_work->m_scores += base_capital / initial_btc_value + unique_value;
    m_work->m_scores += CoinAmount::COIN * 1000 * ( m_work->m_scores[ 0 ].pow( 2 ) / total_volume ) + unique_value;

    // append alpha readout
//    if ( !m_work->m_alpha_readout.isEmpty() )
//        m_work->m_alpha_readout += '\n';

//    m_work->m_alpha_readout += alpha.getAlphaReadout();
//    kDebug() << alpha.getAlphaReadout();

#if defined(OUTPUT_SIMULATION_TIME)
    kDebug() << "simulation time" << QDateTime::currentMSecsSinceEpoch() - t0 << "ms";
#endif
}

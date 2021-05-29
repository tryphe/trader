#include "tester.h"
#include "simulationthread.h"

#include "../daemon/coinamount.h"
#include "../daemon/coinamount_test.h"
#include "../daemon/global.h"
#include "../daemon/misctypes.h"
#include "../daemon/pricesignal.h"
#include "../daemon/pricesignal_test.h"
#include "../daemon/priceaggregator.h"

#include <math.h>

#include <QByteArray>
#include <QString>
#include <QTextStream>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QMessageLogger>

Tester::Tester()
{
//    exit(0);

//    int i;

//    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

//    for ( int j = 0; j < 200000; j++ )
//    {
//        PriceSignal s;
//        s.setMaxSamples( 1000 );

//        for ( int k = 0; k < 10000; k++ )
//        {
//            s.addSample( CoinAmount::COIN );
//        }
//    }

//    kDebug() << "1" << QDateTime::currentMSecsSinceEpoch() - t0;
//    t0 = QDateTime::currentMSecsSinceEpoch();



//    kDebug() << "2" << QDateTime::currentMSecsSinceEpoch() - t0;

//    kDebug() << Coin("1") / 10000;
//    exit( 0 );

    CoinAmountTest c;
    c.test();

    PriceSignalTest t;
    t.test();

    // load data
    loadPriceData();

    // load work history
    if ( USE_SAVED_WORK )
    {
        kDebug() << "loading finished work...";
        loadFinishedWork();
    }

//    generateWorkFromResultString( "1500d[9.7100]-hiX[86.610]-finalX[37.380]-volscore[30.930]:sig0[7/154/458/11.]-sig1[7/258/298/9.]-sig2[7/499/633/2.]-sig3[4/143/404/2.]-sig4[7/196/442/14.]-sig5[4/459/491/14.]-func[1]-take[50000]-startamt[1.43498065]-fee[10000]-candlelen[30000]-pricelen[15]-pricebias[5]-[+0]" );

    // generate work
    kDebug() << "generating work...";
    if ( WORK_RANDOM )
    {
        fillRandomWorkQueue();
    }
    else
        generateWork();

    kDebug() << "generated" << m_work_queued.size() << "work units, skipped" << m_work_skipped_duplicate << "duplicate work units";

    // start threads
    startWork();

    // start work result processing timer
    m_work_timer = new QTimer( this );
    connect( m_work_timer, &QTimer::timeout, this, &Tester::onWorkTimer );
    m_work_timer->setTimerType( Qt::VeryCoarseTimer );
    m_work_timer->setInterval( RESULTS_OUTPUT_INTERVAL_SECS * 1000 );
    m_work_timer->start();
}

Tester::~Tester()
{
    // delete work timer
    if ( m_work_timer != nullptr )
    {
        m_work_timer->stop();
        delete m_work_timer;
    }
}

void Tester::loadPriceDataSingle( const QString &file_name, const Market &market )
{
    const QString path = USE_CANDLES_FROM_THIS_DIRECTORY ? file_name :
                                                           Global::getTraderPath() + QDir::separator() + "candles" + QDir::separator() + file_name;

    // load data but don't calculate ma, since we are manually doing it
    const bool ret = PriceAggregator::loadPriceSamples( m_price_data_0[ 0 ][ market ], path );
    assert( ret );

    // reindex samples to desired interval
    reindexPriceData( m_price_data_0[ 0 ][ market ], BASE_INTERVAL );

    // duplicate the new data into data_0 and data_1
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        if ( i > 0 )
            m_price_data_0[ i ][ market ] = m_price_data_0[ 0 ][ market ];

//        m_price_data_1[ i ][ market ] = m_price_data_0[ 0 ][ market ];
    }
}

void Tester::loadPriceData()
{
    // for now, use 2 price data selections, add a map for each thread
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        m_price_data_0 += QMap<Market, PriceData>();
//        m_price_data_1 += QMap<Market, PriceData>();
    }

    /// load samples
    const qint64 t0_secs = QDateTime::currentMSecsSinceEpoch();

    loadPriceDataSingle( "BITTREX.BTC_DASH.5", Market( "BTC_DASH" ) );
    loadPriceDataSingle( "BITTREX.BTC_ETH.5", Market( "BTC_ETH" ) );
    loadPriceDataSingle( "BITTREX.BTC_LTC.5", Market( "BTC_LTC" ) );
    loadPriceDataSingle( "BITTREX.BTC_USDT.5", Market( "BTC_USDT" ) );
    loadPriceDataSingle( "BITTREX.BTC_WAVES.5", Market( "BTC_WAVES" ) );
    loadPriceDataSingle( "BITTREX.BTC_XMR.5", Market( "BTC_XMR" ) );
//    loadPriceDataSingle( "BITTREX.BTC_ZEC.5", Market( "BTC_ZEC" ) );

    // remove some price data for a different simulation outcome
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
//        m_price_data_1[ i ].remove( Market( "BTC_USDT" ) );
//        m_price_data_1[ i ].remove( Market( "BTC_WAVES" ) );
    }

//    assert( m_price_data_0.size() == m_price_data_1.size() );
    kDebug() << "loaded price data for" << m_price_data_0.size() << "threads in" << QDateTime::currentMSecsSinceEpoch() - t0_secs << "ms";
}

void Tester::reindexPriceData( PriceData &data, const int interval )
{
    if ( interval < 2 )
        return;

    PriceSignal base = PriceSignal( SMA, interval );

    int interval_counter = 0, skipped_count = 0, data_overwrite_idx = 0;
    const int original_data_size = data.data.size();
    const int skip_n = WORK_SAMPLES_BASE_LEVELER ? data.data.size() % interval : 0;
    const auto &data_end = data.data.end();
    for ( auto i = data.data.begin(); i < data_end; i++ )
    {
        // skip the first WORK_SAMPLES_BASE_START_OFFSET base samples
        if ( WORK_SAMPLES_BASE_LEVELER && skipped_count++ < skip_n )
        {
            data.data_start_secs += CANDLE_INTERVAL_SECS; // push start time ahead for each candle we skip
            continue;
        }

        const Coin &sample = *i;
        const bool should_run = ++interval_counter % interval == 0;

        // add sample to base every iteration
        base.addSample( sample );

//        kDebug() << "sample" << interval_counter;

        // check if we should add base to newdata
        if ( !should_run )
            continue;

//        kDebug() << "created new candle";

        // reset counter, overwrite already used candle with new combined candle
        interval_counter = 0;
        data.data[ data_overwrite_idx++ ] = base.getSignal();
    }

    // trim old data
    while ( data.data.size() > data_overwrite_idx )
        data.data.removeLast();

    kDebug() << "reindexed price data," << original_data_size - skip_n << "->" << data.data.size() << "samples";
}

void Tester::fillRandomWorkQueue()
{
    qint64 current_count = 0, current_tries = 0;
    while ( m_work_count_total < WORK_UNITS_BUFFER )
    {
        generateRandomWork();

        // when new work is generated, reset tries
        if ( current_count != m_work_count_total )
        {
            current_count = m_work_count_total;
            current_tries = 0;
        }

        // if we try too many times without generating any work, break
        if ( ++current_tries > WORK_RANDOM_TRIES_MAX )
            break;
    }
}

void Tester::generateWork()
{
    SimulationTask *work = new SimulationTask;

    /// test
//    work->addStrategyArgs( StrategyArgs( RSIRatio,  40, 600, Coin("1.") ) );
//    work->m_allocation_func = 1;
    ///

    /// active strat
    work->addStrategyArgs( StrategyArgs( SMARatio, 449, 659, Coin("1.") ) );
    work->addStrategyArgs( StrategyArgs( SMARatio, 38,  148, Coin("2.") ) );
    work->addStrategyArgs( StrategyArgs( SMARatio, 475, 579, Coin("6.") ) );
    work->addStrategyArgs( StrategyArgs( RSIRatio, 301, 474, Coin("10.") ) );
    work->addStrategyArgs( StrategyArgs( RSIRatio, 232, 257, Coin("8.") ) );
    work->addStrategyArgs( StrategyArgs( SMARatio, 106, 465, Coin("7.") ) );
    work->addStrategyArgs( StrategyArgs( RSIRatio, 287, 344, Coin("10.") ) );
    work->m_allocation_func = 1;
    ///

    /// old brief strat
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 451, 677, Coin("2.") ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 302, 445, Coin("3.") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 222, 438, Coin("15.") ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 8,   155, Coin("10.") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 140, 577, Coin("10.") ) );
//    work->m_allocation_func = 1;
    ///

    /// old strat
//    work->addStrategyArgs( StrategyArgs( SMARatio, 147, 442, Coin("4.") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 130, 613, Coin("3.") ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 29,  211, Coin("7.") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 237, 337, Coin("3.") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 307, 362, Coin("11.") ) );
//    work->m_allocation_func = 1;
    ///

    /// old strat
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 108, 643, Coin("9.5") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 263, 414, Coin("8.") ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 283, 405, Coin("5.5") ) );
//    work->addStrategyArgs( StrategyArgs( EMARatio, 32,  299, Coin("7.") ) );
//    work->m_allocation_func = 1;
    ///

    /// old active strat with different weights
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 108, 643, Coin("8.1") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 263, 414, Coin("10.7") ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 283, 405, Coin("1.6") ) );
//    work->addStrategyArgs( StrategyArgs( EMARatio, 32,  299, Coin("7.5") ) );
//    work->m_allocation_func = 1;
    ///

    /// old strat
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 27,  227, Coin("2.5") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 123, 486, Coin("11.5") ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 2,   76,  Coin("2.3") ) );
//    work->addStrategyArgs( StrategyArgs( WMARatio, 5,   10,  Coin("1") ) );
//    work->m_allocation_func = 1;
    ///

    /// old strat
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 27,  227, Coin("2.5") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 123, 486, Coin("11.5") ) );
//    work->m_allocation_func = 0;
    ///

    /// old strat
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 30, 142, Coin("2") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 78, 139, Coin("1") ) );
//    work->m_allocation_func = 0;
    ///

    /// old test strat
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 27,  227, Coin("2.5") ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 123, 486, Coin("11.5") ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 2,   76,  Coin("2.3") ) );
//    work->addStrategyArgs( StrategyArgs( WMARatio, 5,   10,  Coin("1") ) );
//    work->m_allocation_func = 1;
    ///

    // add markets
    work->m_markets_tested += m_price_data_0.first().keys().toVector();

    // delete work on empty args or duplicate work id
    const QByteArray &work_id = work->getUniqueID();
    if ( WORK_PREVENT_RETRY && ( work->m_strategy_args.isEmpty() || m_work_ids_generated_or_done.contains( work_id ) ) )
    {
        m_work_skipped_duplicate++;
        delete work;
        return;
    }

    if ( WORK_PREVENT_RETRY )
        m_work_ids_generated_or_done += work->getUniqueID();

    m_work_queued += work;
    m_work_count_total++;
}

void Tester::generateRandomWork()
{
    SimulationTask *work = new SimulationTask;

    /// 1
//    const int strategy_count = Global::getSecureRandomRange32( 2, 4 );
//    work->m_strategy_args.resize( strategy_count );
//    for ( int i = 0; i < strategy_count; i++ )
//    {
//        // SMAR or RSIR: Global::getSecureRandomRange32( 0, 1 ) == 0 ? SMARatio : RSIRatio
//        // SMAR:RSIR:    static_cast<PriceSignalType>( Global::getSecureRandomRange32( SMARatio, RSIRatio ) );
//        const PriceSignalType type = RSIRatio;//static_cast<PriceSignalType>( Global::getSecureRandomRange32( SMARatio, RSIRatio ) );

//        const int length_fast = 2 + std::pow( Global::getSecureRandomRange32( 3, 28 ), 2 );
//        const int length_slow = /*( type == RSIRatio && Global::getSecureRandomRange32( 0, 1 ) == 0 ) ? 0 :*/
//                                2 + std::pow( Global::getSecureRandomRange32( 3, 28 ), 2 );

//        if ( length_fast >= length_slow )
//        {
//            i--;
//            continue;
//        }

//        assert( length_fast > 0 );
//        assert( length_slow > 0 || type < SMARatio || type == RSIRatio );

//        work->m_strategy_args[ i ].type = type;
//        work->m_strategy_args[ i ].length_fast = length_fast;
//        work->m_strategy_args[ i ].length_slow = length_slow;

//        // check for duplicate type/lengths
//        bool duplicate_lengths = false;
//        for ( int j = 0; j < strategy_count; j++ )
//        {
//            if ( j != i && work->m_strategy_args[ i ].isEqualExcludingWeights( work->m_strategy_args[ j ] ) )
//            {
//                duplicate_lengths = true;
//                break;
//            }
//        }

//        // continue on duplicate
//        if ( duplicate_lengths )
//        {
//            i--;
//            continue;
//        }

//        // weight 1:15
//        work->m_strategy_args[ i ].weight = CoinAmount::SATOSHI * 100000000 + CoinAmount::SATOSHI * 50000000 * Global::getSecureRandomRange32( 0, 28 );
//    }

//    work->m_allocation_func = Global::getSecureRandomRange32( 0, 22 );
    /// 1

    /// 2 random mixup
    work->m_allocation_func = 1;

    QMap<PriceSignalType, int> signal_count;

    const int signal_total = Global::getSecureRandomRange32( 4, 8 );
    for ( int i = 0; i < signal_total; i++ )
    {
        // smar or rsir for now
        const PriceSignalType t = Global::getSecureRandomRange32( 0, 1 ) == 0 ? SMARatio : RSIRatio;

        // only allow <=3 of each type (2 or 3 seems good)
//        if ( ++signal_count[ t ] > 4 )
//        {
//            --i;
//            continue;
//        }

        const int fast = Global::getSecureRandomRange32( 2, 500 );
        const int slow = Global::getSecureRandomRange32( fast +/*1*/20, 700 );
        const Coin w = CoinAmount::COIN + CoinAmount::SATOSHI * 100000000 * Global::getSecureRandomRange32( 0, 14 );
        work->addStrategyArgs( StrategyArgs( t, fast, slow, w ) );
    }
    ///

    /// old style mixup, seems to work good
//    work->m_strategy_args[ 0 ].type = RSIRatio;
//    work->m_strategy_args[ 0 ].length_fast = Global::getSecureRandomRange32( 2, 500 );
//    work->m_strategy_args[ 0 ].length_slow = Global::getSecureRandomRange32( work->m_strategy_args[ 0 ].length_fast +1, 1000 );
//    work->m_strategy_args[ 0 ].weight = Coin("1") + CoinAmount::SATOSHI * 10000000 * Global::getSecureRandomRange32( 1, 110 );

//    work->m_strategy_args[ 1 ].type = RSIRatio;
//    work->m_strategy_args[ 1 ].length_fast = Global::getSecureRandomRange32( 2, 500 );
//    work->m_strategy_args[ 1 ].length_slow = Global::getSecureRandomRange32( work->m_strategy_args[ 1 ].length_fast +1, 1000 );
//    work->m_strategy_args[ 1 ].weight = Coin("1") + CoinAmount::SATOSHI * 10000000 * Global::getSecureRandomRange32( 1, 110 );

//    work->m_strategy_args[ 2 ].type = SMARatio;
//    work->m_strategy_args[ 2 ].length_fast = Global::getSecureRandomRange32( 1, 500 );
//    work->m_strategy_args[ 2 ].length_slow = Global::getSecureRandomRange32( work->m_strategy_args[ 2 ].length_fast +1, 1000 );
//    work->m_strategy_args[ 2 ].weight = Coin("1") + CoinAmount::SATOSHI * 10000000 * Global::getSecureRandomRange32( 1, 110 );

//    work->m_strategy_args[ 3 ].type = static_cast<PriceSignalType>( Global::getSecureRandomRange32( 5, 6 ) );
//    work->m_strategy_args[ 3 ].length_fast = Global::getSecureRandomRange32( 1, 500 );
//    work->m_strategy_args[ 3 ].length_slow = Global::getSecureRandomRange32( work->m_strategy_args[ 3 ].length_fast +1, 1000 );
//    work->m_strategy_args[ 3 ].weight = Coin("1") + CoinAmount::SATOSHI * 10000000 * Global::getSecureRandomRange32( 1, 110 );

//    work->m_allocation_func = 1;//Global::getSecureRandomRange32( 0, 1 ) == 0 ? 0 : Global::getSecureRandomRange32( 1, 22 );
    ///

    /// refine weights
//    work->m_allocation_func = 1;
//    const Coin w0 = CoinAmount::COIN * Global::getSecureRandomRange32( 1, 2 );
//    const Coin w1 = CoinAmount::COIN * Global::getSecureRandomRange32( 1, 3 );
//    const Coin w2 = CoinAmount::COIN * Global::getSecureRandomRange32( 5, 7 );
//    const Coin w3 = CoinAmount::COIN * Global::getSecureRandomRange32( 9, 11 );
//    const Coin w4 = CoinAmount::COIN * Global::getSecureRandomRange32( 7, 9 );
//    const Coin w5 = CoinAmount::COIN * Global::getSecureRandomRange32( 6, 8 );
//    const Coin w6 = CoinAmount::COIN /* + CoinAmount::SATOSHI * 100000000*/ * Global::getSecureRandomRange32( 8, 11 );

//    work->addStrategyArgs( StrategyArgs( SMARatio, 449, 659, w0 ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 38,  148, w1 ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 475, 579, w2 ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 301, 474, w3 ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 232, 257, w4 ) );
//    work->addStrategyArgs( StrategyArgs( SMARatio, 106, 465, w5 ) );
//    work->addStrategyArgs( StrategyArgs( RSIRatio, 287, 344, w6 ) );
    ///

    // add markets
    work->m_markets_tested += m_price_data_0.first().keys().toVector();

    // if hash of raw data exists in QSet, skip adding duplicate work
    const QByteArray &work_id = work->getUniqueID();
    if ( WORK_PREVENT_RETRY && m_work_ids_generated_or_done.contains( work_id ) )
    {
        m_work_skipped_duplicate++;
        delete work;
        return;
    }

    if ( WORK_PREVENT_RETRY )
        m_work_ids_generated_or_done += work_id;

    m_work_queued += work;
    m_work_count_total++;
}

void Tester::generateWorkFromResultString( const QString &construct )
{   // reads the alloc func and sig args from a simulation result string. does not verify the string or read other args.
    SimulationTask *work = new SimulationTask;

    // read alloc func
    const int alloc_func_start = construct.indexOf( "func[" ) +5;
    const int alloc_func_end = construct.indexOf( "]", alloc_func_start );
    const int alloc_func = construct.mid( alloc_func_start, alloc_func_end - alloc_func_start ).toInt();

//    kDebug() << "func" << alloc_func;
    work->m_allocation_func = alloc_func;

    // read sigs
    const QList<QString> arg_sections = construct.split( QChar(']') );
    for ( QList<QString>::const_iterator i = arg_sections.begin(); i != arg_sections.end(); i++ )
    {
        const QString &arg = *i;

        // look for sig marker
        if ( !arg.contains( "sig" ) )
            continue;

        const QList<QString> sig_parts = arg.split( QChar('[') );
        if ( sig_parts.size() < 2 )
            continue;

        const QString sig_back = sig_parts.value( 1 );
        const QList<QString> sig_args = sig_back.split( QChar('/') );

        if ( sig_args.size() < 4 )
            continue;

//        kDebug() << sig_idx << sig_args;
        work->addStrategyArgs( StrategyArgs( static_cast<PriceSignalType>( sig_args[ 0 ].toInt() ),
                                             sig_args[ 1 ].toInt(),
                                             sig_args[ 2 ].toInt(),
                                             sig_args[ 3 ] ) );
    }

    // add markets
    work->m_markets_tested += m_price_data_0.first().keys().toVector();

    // delete work on empty args or duplicate work id
    const QByteArray &work_id = work->getUniqueID();
    if ( WORK_PREVENT_RETRY && ( work->m_strategy_args.isEmpty() || m_work_ids_generated_or_done.contains( work_id ) ) )
    {
        m_work_skipped_duplicate++;
        delete work;
        return;
    }

    if ( WORK_PREVENT_RETRY )
        m_work_ids_generated_or_done += work_id;

    m_work_queued += work;
    m_work_count_total++;
}

void Tester::startWork()
{
    // init threads
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        SimulationThread *const t = new SimulationThread( i );
        m_threads += t;

        t->m_price_data += &m_price_data_0[ i ];
//        t->m_price_data += &m_price_data_1[ i ];

        t->ext_mutex = &m_work_mutex;
        t->ext_threads = &m_threads;
        t->ext_work_done = &m_work_done;
        t->ext_work_queued = &m_work_queued;
        t->ext_work_count_total = &m_work_count_total;
        t->ext_work_count_done = &m_work_count_done;
        t->ext_work_count_started = &m_work_count_started;

        connect( t, &QThread::finished, this, &Tester::onThreadFinished );

        t->start();
    }

#if defined(OUTPUT_TOTAL_RUNTIME)
    m_start_time = QDateTime::currentMSecsSinceEpoch();
#endif
}

void Tester::processFinishedWork()
{
    QVector<SimulationTask*> work_to_delete;
    QVector<QString> processed_results;
    int tasks_processed = 0;

    QMutexLocker lock0( &m_work_mutex );
    for ( QVector<SimulationTask*>::iterator i = m_work_done.begin(); i != m_work_done.end(); i++ )
    {
        SimulationTask *const &task = *i;

        // don't remove during iteration, queue for deletion
        work_to_delete += task;
        tasks_processed++;

        // if zero score, evict if RESULTS_EVICT_ZERO_SCORE policy is enabled
        if ( RESULTS_EVICT_ZERO_SCORE && task->m_scores[ 0 ].isZeroOrLess() )
            continue;

        // normalize scores
        for ( int score_type = 0; score_type < task->m_scores.size(); score_type++ )
            task->m_scores[ score_type ] = task->m_scores.value( score_type ) / MARKET_VARIATIONS;

        // prepend scores to simulation result
        const QString score_str = QString( "1500d[%1]-hiX[%2]-finalX[%3]-volscore[%4]:" )
                .arg( task->m_scores[ 0 ].toString( 2 ), -6, QChar('0') )
                .arg( task->m_scores[ 1 ].toString( 2 ), -6, QChar('0') )
                .arg( task->m_scores[ 2 ].toString( 2 ), -6, QChar('0') )
                .arg( task->m_scores[ 3 ].toString( 2 ), -6, QChar('0') );

        task->m_simulation_result.prepend( score_str );
        processed_results += task->m_simulation_result;

        // copy scores into local data
        for ( int score_type = 0; score_type < task->m_scores.size(); score_type++ )
        {
            const Coin &score = task->m_scores[ score_type ];

            m_highscores_by_result[ score_type ][ task->m_simulation_result ] = score;
            m_highscores_by_score[ score_type ].insert( score, task->m_simulation_result );
        }

        m_work_results_unsaved[ task->getUniqueID() ] = task->m_simulation_result;
    }

    // cleanup finished work
    while ( !work_to_delete.isEmpty() )
    {
        SimulationTask *task = work_to_delete.takeFirst();
        m_work_done.removeOne( task );
        delete task;
    }

    if ( tasks_processed < 1 )
        return;

    // open results log file
    static const qint64 START_SECS = QDateTime::currentSecsSinceEpoch();
    const QString filename = QString( "simulation.results.%1.txt" ).arg( START_SECS );
    QFile savefile( filename );
    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't open" << filename << "for writing";
        return;
    }

    QTextStream out_savefile( &savefile );

    printHighScores( m_highscores_by_score[ 0 ], out_savefile, " 1500d " );
    printHighScores( m_highscores_by_score[ 1 ], out_savefile, " PEAK " );
    printHighScores( m_highscores_by_score[ 2 ], out_savefile, " FINAL " );
    printHighScores( m_highscores_by_score[ 3 ], out_savefile, " VOLSCORE " );

    // save the buffer
    out_savefile.flush();
    savefile.close();

    // trim scores maps
    trimHighScores( m_highscores_by_score[ 0 ], m_highscores_by_result[ 0 ] );
    trimHighScores( m_highscores_by_score[ 1 ], m_highscores_by_result[ 1 ] );
    trimHighScores( m_highscores_by_score[ 2 ], m_highscores_by_result[ 2 ] );
    trimHighScores( m_highscores_by_score[ 3 ], m_highscores_by_result[ 3 ] );

    kDebug() << QString( "[%1 of %2] %3% done, %4 threads active, %5 new work results processed" )
                 .arg( m_work_count_done )
                 .arg( Tester::WORK_RANDOM && Tester::WORK_INFINITE ? "inf" : QString( "%1" ).arg( m_work_count_total ) )
                 .arg( Tester::WORK_RANDOM && Tester::WORK_INFINITE ? "0" : QString( "%1" ).arg( Coin( m_work_count_done ) / Coin( m_work_count_total ) * 100 ) )
                 .arg( m_threads.size() )
                 .arg( processed_results.size() );

    if ( RESULTS_OUTPUT_NEWLY_FINISHED )
    {
        kDebug() << "new work results:";
        for ( QVector<QString>::const_iterator i = processed_results.begin(); i != processed_results.end(); i++ )
            kDebug() << *i;
    }

    // save unsaved work
    saveFinishedWork();

    // if infinite work, generate more work
    if ( WORK_RANDOM && WORK_INFINITE )
        fillRandomWorkQueue();

    // exit when we are done
    if ( m_threads.isEmpty()
         /*m_work_queued.size() == 0 &&
         m_work_done.size() == 0 &&
         m_work_count_total == m_work_count_done*/ )
    {
        kDebug() << "done!";
#if defined(OUTPUT_TOTAL_RUNTIME)
        kDebug() << "total runtime" << QDateTime::currentMSecsSinceEpoch() - m_start_time << "ms";
#endif
        lock0.unlock();
        this->~Tester();
        exit( 0 );
    }
}

void Tester::printHighScores( const QMultiMap<Coin, QString> &scores, QTextStream &out, QString description, const int print_count )
{
    Global::centerString( description, QChar('='), 40 );

    out << description << "\n";
    kDebug() << description;

    const int score_count = scores.size();
    int scores_iterated = 0;
    for ( QMultiMap<Coin, QString>::const_iterator j = scores.begin(); j != scores.end(); j++ )
    {
        // note: this is a multimap, so we can see identical scores with different configurations

        // only show the 5 highest scores
        if ( ++scores_iterated < score_count - print_count +1 )
            continue;

        out << j.value() << "\n";
        kDebug() << j.value();
    }
}

void Tester::trimHighScores( QMultiMap<Coin, QString> &scores, QMap<QString, Coin> &scores_by_result )
{
    // check if we should trim
    if ( scores.size() < RESULTS_HIGH_SCORE_COUNT )
        return;

    QMultiMap<Coin, QString> new_scores;
    QMap<QString, Coin> new_scores_by_result;

    // assemble trimmed version of maps
    const QMultiMap<Coin, QString>::const_iterator &scores_end = scores.end();
    for ( QMultiMap<Coin, QString>::const_iterator i = scores_end - RESULTS_HIGH_SCORE_COUNT; i != scores_end; i++ )
    {
        new_scores.insert( i.key(), i.value() );
        new_scores_by_result.insert( i.value(), i.key() );
    }

    // copy into maps
    scores = new_scores;
    scores_by_result = new_scores_by_result;

//    kDebug() << "scores trimmed to" << scores.size() << scores_by_result.size();
}

void Tester::saveFinishedWork()
{
    // if we aren't using saved work, empty the unsaved work
    if ( !USE_SAVED_WORK )
        m_work_results_unsaved.clear();

    // return on empty unsaved work
    if ( m_work_results_unsaved.isEmpty() )
        return;

    // open data file
    const QString path = "simulation.storage.txt";

    QFile savefile( path );
    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append ) )
    {
        kDebug() << "local error: couldn't open spruce settings file" << path;
        return;
    }

    QTextStream out_savefile( &savefile );

    // save state
    int work_ids_saved_count = 0;
    QVector<QByteArray> work_ids_raw_saved;
    for ( QMap<QByteArray, QString>::const_iterator i = m_work_results_unsaved.begin(); i != m_work_results_unsaved.end(); i++ )
    {
        const QString work_id_hex = i.key().toHex();
        const QString &work_result = i.value();

        // if result is not in the top scores, skip saving if policy allows
        if ( RESULTS_EVICT_NON_HIGH_SCORE )
        {
            // check if the result is a high score
            bool is_high_score = false;
            for ( QMap<int, QMap<QString, Coin>>::const_iterator j = m_highscores_by_result.begin(); j != m_highscores_by_result.end(); j++ )
            {
                if ( j.value().contains( work_result ) )
                    is_high_score = true;
            }

            // if not high score, skip iteration but mark the work as complete
            if ( !is_high_score )
            {
                // if we are to prevent a retry, we keep it in the map
                if ( !WORK_PREVENT_RETRY )
                    m_work_ids_generated_or_done.remove( i.key() );

                work_ids_raw_saved += i.key();
                continue;
            }
        }

        // save id
        out_savefile << work_id_hex << ' ';

        // save scores
        for ( QMap<int, QMap<QString, Coin>>::const_iterator j = m_highscores_by_result.begin(); j != m_highscores_by_result.end(); j++ )
            out_savefile << j.value()[ work_result ] << ' ';

        // save result
        out_savefile << work_result << '\n';

        // transfer into saved map, and queue for removal from unsaved map
        work_ids_raw_saved += i.key();
        work_ids_saved_count++;
    }

    // removed saved ids from unsaved map
    while ( !work_ids_raw_saved.isEmpty() )
        m_work_results_unsaved.remove( work_ids_raw_saved.takeFirst() );

    // save the buffer
    out_savefile.flush();
    savefile.close();

    kDebug() << "saved" << work_ids_saved_count << "new work results";
}

void Tester::loadFinishedWork()
{
    QString path = "simulation.storage.txt";
    QFile loadfile( path );

    if ( !loadfile.open( QIODevice::ReadWrite | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't load stats file" << path;
        qFatal( "failed" );
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    QList<QByteArray> data = loadfile.readAll().split( '\n' );
    int results_loaded = 0;

    QMap<int, QMultiMap<Coin, QString>> m_highscores_by_score_tmp; // for each score type, store kv<score,id>

    QByteArray work_id;
    QString result, score_0, score_1, score_2, score_3;
    const int data_length = data.size() -1; // skip last line
    for ( int i = 0; i < data_length; i++ )
    {
        QList<QByteArray> line_data = data[ i ].split( ' ' );

        // fatal, warn about possible corruption
        if ( line_data.size() != 6 )
        {
            kDebug() << "loadFinishedWork() failed, line" << i +1 << "is corrupt, data:" << data[ i ];
            exit( 10 );
        }

        work_id = QByteArray::fromHex( line_data[ 0 ] );
        result = line_data[ 5 ];

        score_0 = line_data[ 1 ];
        score_1 = line_data[ 2 ];
        score_2 = line_data[ 3 ];
        score_3 = line_data[ 4 ];

        m_highscores_by_score_tmp[ 0 ].insert( score_0, result );
        m_highscores_by_score_tmp[ 1 ].insert( score_1, result );
        m_highscores_by_score_tmp[ 2 ].insert( score_2, result );
        m_highscores_by_score_tmp[ 3 ].insert( score_3, result );

        if ( WORK_PREVENT_RETRY )
            m_work_ids_generated_or_done += work_id;

        results_loaded++;
    }
//    kDebug() << "loaded" << m_work_ids_generated_or_done.size() << "work ids";

    // only load top RESULTS_HIGH_SCORE_COUNT scores into long-term ram
    for ( int i = 0; i < 4; i++ )
    {
        if ( m_highscores_by_score_tmp[ i ].size() < RESULTS_HIGH_SCORE_COUNT )
            continue;

        for ( QMultiMap<Coin, QString>::const_iterator j = m_highscores_by_score_tmp[ i ].end() - RESULTS_HIGH_SCORE_COUNT; j != m_highscores_by_score_tmp[ i ].end(); j++ )
        {
            const Coin &score = j.key();
            const QString &result = j.value();

            m_highscores_by_result[ i ][ result ] = score;
            m_highscores_by_score[ i ].insert( score, result );
        }

//        kDebug() << "filled indices for scores" << i << ", sizes:" << m_highscores_by_result[ i ].size() << m_highscores_by_score[ i ].size();
    }

    kDebug() << "loaded" << results_loaded << "historical work results";
}

void Tester::onWorkTimer()
{
    m_work_timer->stop();
    processFinishedWork();
    m_work_timer->start();
}

void Tester::onThreadFinished()
{
    QMutexLocker lock0( &m_work_mutex );
    if ( m_threads.isEmpty() )
    {
        lock0.unlock();
        processFinishedWork();
    }
}

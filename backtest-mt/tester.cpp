#include "tester.h"
#include "../daemon/global.h"
#include "../daemon/coinamount.h"
#include "../daemon/coinamount_test.h"
#include "../daemon/sprucev2.h"
#include "../daemon/priceaggregator.h"
#include "../daemon/alphatracker.h"
#include "../daemon/misctypes.h"

#include <QString>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>

Tester::Tester()
{
//    Coin p( CoinAmount::COIN * 2300 + CoinAmount::SUBSATOSHI );
//    kDebug() << p;
//    exit( 0 );

//    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

//    SpruceV2 sp;
//    sp.setBaseCurrency( "BTC" );
//    sp.setVisualization( true );

//    for ( int i = 0; i < 1; i++ )
//    {
//        // add current quantities
//        sp.clearCurrentQtys();
//        sp.setCurrentQty( "BTC",   Coin( "0.0" ) );
//        sp.setCurrentQty( "DASH",  Coin( "100" ) );
//        sp.setCurrentQty( "LTC",   Coin( "100" ) );
//        sp.setCurrentQty( "USDN",  Coin( "10000" ) );
//        sp.setCurrentQty( "WAVES", Coin( "10000" ) );
//        sp.setCurrentQty( "XMR",   Coin( "100" ) );
//        sp.setCurrentQty( "ZEC",   Coin( "100" ) );

//        // add current prices
//        sp.clearCurrentPrices();
//        sp.setCurrentPrice( "DASH",  Coin( "0.00802" ) );
//        sp.setCurrentPrice( "LTC",   Coin( "0.00478" ) );
//        sp.setCurrentPrice( "USDN",  Coin( "0.000102" ) );
//        sp.setCurrentPrice( "WAVES", Coin( "0.000117" ) );
//        sp.setCurrentPrice( "XMR",   Coin( "0.00681" ) );
//        sp.setCurrentPrice( "ZEC",   Coin( "0.00533" ) );

//        // add signal prices
//        sp.clearSignalPrices();
//        sp.setSignalPrice( "DASH",  Coin( "0.00975" ) );
//        sp.setSignalPrice( "LTC",   Coin( "0.00713" ) );
//        sp.setSignalPrice( "USDN",  Coin( "0.000116" ) );
//        sp.setSignalPrice( "WAVES", Coin( "0.000126" ) );
//        sp.setSignalPrice( "XMR",   Coin( "0.00756" ) );
//        sp.setSignalPrice( "ZEC",   Coin( "0.00559" ) );

//        // calculate ratio table
//        if ( !sp.calculateAmountToShortLong() )
//        {
//            kDebug() << "calculateAmountToShortLong() failed";
//            return;
//        }

//        //kDebug() << sp.getQuantityToShortLongMap();
//        //kDebug() << sp.getBaseCapital();

//        kDebug() << sp.getVisualization();
//    }

//    return;
//    kDebug() << "took" << QDateTime::currentMSecsSinceEpoch() - t0 << "ms";

    SignalTest t;
    t.test();

//    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    CoinAmountTest c;
    c.test();

//    kDebug() << "tests passed in" << QDateTime::currentMSecsSinceEpoch() - t0 << "ms";

    // load data
    loadPriceData();

    // generate work
    if ( RUN_RANDOM_TESTS )
        for ( int i = 0; i < 100000; i++ )
            generateRandomWork();
    else
        generateWork();

    kDebug() << "generated" << m_work_queued.size() << "work units";

    // start threads
    startWork();

    // start work result processing timer
    m_work_timer = new QTimer( this );
    connect( m_work_timer, &QTimer::timeout, this, &Tester::onWorkTimer );
    m_work_timer->setTimerType( Qt::CoarseTimer );
    m_work_timer->setInterval( 2000 );
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
    const QString path = Global::getTraderPath() + QDir::separator() + "candles" + QDir::separator() + file_name;

    // load data but don't calculate ma, since we are manually doing it
    const bool ret0 = PriceAggregator::loadPriceSamples( m_price_data_0[ 0 ][ market ], path, 0, 0 );
    assert( ret0 );

    // duplicate the new data into data_0 and data_1
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        if ( i > 0 )
            m_price_data_0[ i ][ market ] = m_price_data_0[ 0 ][ market ];

        m_price_data_1[ i ][ market ] = m_price_data_0[ 0 ][ market ];
    }
}

void Tester::loadPriceData()
{
    // for now, use 2 price data selections, add a map for each thread
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        m_price_data_0 += QMap<Market, PriceData>();
        m_price_data_1 += QMap<Market, PriceData>();
    }

    /// load samples
    const qint64 t0_secs = QDateTime::currentMSecsSinceEpoch();

    loadPriceDataSingle( "BITTREX.BTC_DASH.5", Market( "BTC_DASH" ) );
    loadPriceDataSingle( "BITTREX.BTC_ETH.5", Market( "BTC_ETH" ) );
    loadPriceDataSingle( "BITTREX.BTC_LTC.5", Market( "BTC_LTC" ) );
    loadPriceDataSingle( "BITTREX.BTC_USDT.5", Market( "BTC_USDT" ) );
    loadPriceDataSingle( "BITTREX.BTC_WAVES.5", Market( "BTC_WAVES" ) );
    loadPriceDataSingle( "BITTREX.BTC_XMR.5", Market( "BTC_XMR" ) );
    //loadPriceData( "BITTREX.BTC_ZEC.5", "BTC_ZEC" );

    // remove some price data for a different simulation outcome
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        m_price_data_1[ i ].remove( Market( "BTC_USDT" ) );
        m_price_data_1[ i ].remove( Market( "BTC_WAVES" ) );
    }

    assert( m_price_data_0.size() == m_price_data_1.size() );
    kDebug() << "loaded price data for" << m_price_data_0.size() << "threads in" << QDateTime::currentMSecsSinceEpoch() - t0_secs << "ms";
}

void Tester::generateWork()
{
    kDebug() << "generating work...";

    // non-loop initialized options to generate
    QVector<int> modulation_length_slow;
    QVector<int> modulation_length_fast;
    QVector<Coin> modulation_factor;
    QVector<Coin> modulation_threshold;

    for ( int signal_type = SMA; signal_type <= RSI; signal_type++ )
    {
    for ( int allocation_func = 0; allocation_func <= 19; allocation_func++ ) // 30m to 30m, increment 30m
    {
    for ( int base_ma_length = 10; base_ma_length <= 1000; base_ma_length *= 2 ) // 40 == 3.3h
    {
    for ( int rsi_length = 10; rsi_length <= 1000; rsi_length *= 3 ) // 1h to 1d
    {
    for ( int rsi_ma_length = 10; rsi_ma_length <= 1000; rsi_ma_length *= 3 )
    {
    // modulation 1
//    for ( modulation_length_slow += 250; modulation_length_slow[ 0 ] <= 500; modulation_length_slow[ 0 ] *= 2 )
//    {
//    for ( modulation_length_fast += 10; modulation_length_fast[ 0 ] <= 30; modulation_length_fast[ 0 ] *= 3 )
//    {
//    for ( modulation_factor += Coin( "1.1" ); modulation_factor[ 0 ] <= Coin( "8.8" ); modulation_factor[ 0 ] *= Coin( "2" ) )
//    {
//    for ( modulation_threshold += Coin( "1" ); modulation_threshold[ 0 ] <= Coin( "1.95" ); modulation_threshold[ 0 ] *= Coin( "1.2" ) )
//    {
//    // modulation 2
//    for ( modulation_length_slow += 250; modulation_length_slow[ 1 ] <= 500; modulation_length_slow[ 1 ] *= 2 )
//    {
//    for ( modulation_length_fast += 10; modulation_length_fast[ 1 ] <= 30; modulation_length_fast[ 1 ] *= 3 )
//    {
//    for ( modulation_factor += Coin( "1.1" ); modulation_factor[ 1 ] <= Coin( "8.8" ); modulation_factor[ 1 ] *= Coin( "2" ) )
//    {
//    for ( modulation_threshold += Coin( "1" ); modulation_threshold[ 1 ] <= Coin( "1.95" ); modulation_threshold[ 1 ] *= Coin( "1.2" ) )
//    {
    // modulation 3
//    for ( modulation_length_slow += 62; modulation_length_slow[ 2 ] <= 248; modulation_length_slow[ 2 ] *= 2 )
//    {
//    for ( modulation_length_fast += 6; modulation_length_fast[ 2 ] <= 12; modulation_length_fast[ 2 ] *= 2 )
//    {
//    for ( modulation_factor += Coin( "1" ); modulation_factor[ 2 ] <= Coin( "9" ); modulation_factor[ 2 ] *= Coin( "3" ) )
//    {
//    for ( modulation_threshold += Coin( "1" ); modulation_threshold[ 2 ] <= Coin( "1" ); modulation_threshold[ 2 ] *= Coin( "2" ) )
//    {
        // generate next work
        SimulationTask *work = new SimulationTask;
        work->m_strategy_signal_type = static_cast<SignalType>( signal_type );
        work->m_base_ma_length = base_ma_length;
        work->m_rsi_length = rsi_length;
        work->m_rsi_ma_length = rsi_ma_length;
        work->m_allocation_func = allocation_func;

        work->m_modulation_length_slow = modulation_length_slow;
        work->m_modulation_length_fast = modulation_length_fast;
        work->m_modulation_factor = modulation_factor;
        work->m_modulation_threshold = modulation_threshold;

        m_work_queued += work;
        m_work_count_total++;
//    }
//    modulation_threshold.remove( 2 );
//    }
//    modulation_factor.remove( 2 );
//    }
//    modulation_length_fast.remove( 2 );
//    }
//    modulation_length_slow.remove( 2 );
//    }
//    modulation_threshold.remove( 1 );
//    }
//    modulation_factor.remove( 1 );
//    }
//    modulation_length_fast.remove( 1 );
//    }
//    modulation_length_slow.remove( 1 );
//    }
//    modulation_threshold.remove( 0 );
//    }
//    modulation_factor.remove( 0 );
//    }
//    modulation_length_fast.remove( 0 );
//    }
//    modulation_length_slow.remove( 0 );
    }
    }
    }
    }
    }
}

void Tester::generateRandomWork()
{
    SimulationTask *work = new SimulationTask;

    //int base_ma_length_rand = Global::getSecureRandomRange32( 1, 5 );
    int rsi_length_rand = Global::getSecureRandomRange32( 1, 15 );
    int rsi_ma_length_rand = Global::getSecureRandomRange32( 1, 15 );

    work->m_strategy_signal_type = Global::getSecureRandomRange32( 0, 1 ) == 0 ? SMA : RSI;
    work->m_base_ma_length =  40;
    work->m_rsi_length = rsi_length_rand * rsi_length_rand * 10;
    work->m_rsi_ma_length = rsi_ma_length_rand * rsi_ma_length_rand * 10;
    work->m_allocation_func = Global::getSecureRandomRange32( 0, 19 );

    int modulation_count = Global::getSecureRandomRange32( 0, 2 );
    for ( int i = 0; i < modulation_count; i++ )
    {
        int modulation_length_slow = Global::getSecureRandomRange32( 1, 15 );
        int modulation_length_fast = Global::getSecureRandomRange32( 1, 15 );

        work->m_modulation_length_slow += modulation_length_slow * modulation_length_slow * 10;
        work->m_modulation_length_fast += modulation_length_fast * modulation_length_fast * 10;
        work->m_modulation_factor += CoinAmount::COIN + ( Coin("0.1") * Global::getSecureRandomRange32( 0, 80 ) );
        work->m_modulation_threshold += CoinAmount::COIN + ( Coin("0.1") * Global::getSecureRandomRange32( 0, 30 ) );
    }

    m_work_queued += work;
    m_work_count_total++;
}

void Tester::startWork()
{
    // init threads
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        SimulationThread *t = new SimulationThread;
        m_threads += t;

        t->m_price_data += &m_price_data_0[ i ];
        t->m_price_data += &m_price_data_1[ i ];

        t->ext_mutex = &m_work_mutex;
        t->ext_threads = &m_threads;
        t->ext_work_done = &m_work_done;
        t->ext_work_queued = &m_work_queued;
        t->ext_work_count_total = &m_work_count_total;
        t->ext_work_count_done = &m_work_count_done;
        t->ext_work_count_started = &m_work_count_started;

        t->start();
        t->setPriority( QThread::HighestPriority );
    }
}

void Tester::processFinishedWork()
{
    QVector<SimulationTask*> work_to_delete;
    int tasks_processed = 0;

    QMutexLocker lock0( &m_work_mutex );
    for ( QVector<SimulationTask*>::iterator i = m_work_done.begin(); i != m_work_done.end(); i++ )
    {
        SimulationTask *task = *i;

        // don't remove during iteration, queue for deletion
        work_to_delete += task;
        tasks_processed++;

        // copy normalized scores into scores
        QMap<int, Coin> scores;
        for ( int j = 0; j < task->m_scores.size(); j++ )
            scores[ j ] = task->m_scores.value( j ) / MARKET_VARIATIONS;

        const QString score_out = QString( "------------------------------------------------------------------------------------\nscore-1000d[%1] score-300d[%2] score-300d/1000d[%3] peak[%4] final[%5] gainloss[%6]: %7\n%8" )
                                   .arg( scores[ 0 ] )
                                   .arg( scores[ 1 ] )
                                   .arg( scores[ 2 ] )
                                   .arg( scores[ 3 ] )
                                   .arg( scores[ 4 ] )
                                   .arg( scores[ 5 ] )
                                   .arg( task->m_simulation_id )
                                   .arg( task->m_alpha_readout );

        // copy scores into local data
        for ( int j = 0; j < scores.size(); j++ )
            m_highscores[ j ][ scores[ j ] ] = score_out;
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

    kDebug() << "========================== HIGH 1000d SCORES ==========================";
    printHighScores( m_highscores[ 0 ] );
    kDebug() << "========================== HIGH 300d SCORES ===========================";
    printHighScores( m_highscores[ 1 ] );
    kDebug() << "========================== HIGH 300d/1000d SCORES =====================";
    printHighScores( m_highscores[ 2 ] );
    kDebug() << "========================== HIGH PEAK SCORES ===========================";
    printHighScores( m_highscores[ 3 ] );
    kDebug() << "========================== HIGH FINAL SCORES ==========================";
    printHighScores( m_highscores[ 4 ] );
    kDebug() << "========================== HIGH GAINLOSS ==============================";
    printHighScores( m_highscores[ 5 ] );

    if ( m_work_queued.size() == 0 &&
         m_work_done.size() == 0 &&
         m_work_count_total == m_work_count_done )
    {
        kDebug() << "done!";
        this->~Tester();
        exit( 0 );
    }
}

void Tester::printHighScores( const QMap<Coin, QString> &scores, const int print_count )
{
    const int score_count = scores.size();
    int scores_iterated = 0;
    for ( QMap<Coin, QString>::const_iterator j = scores.begin(); j != scores.end(); j++ )
    {
        // only show the 5 highest scores
        if ( ++scores_iterated < score_count - print_count +1 )
            continue;

        kDebug() << j.value();
    }
}

void Tester::onWorkTimer()
{
    m_work_timer->stop();

    processFinishedWork();

    m_work_timer->start();
}

SimulationThread::SimulationThread()
    : QThread()
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

    kDebug() << "started thread" << QThread::currentThreadId();

    ext_mutex->lock();
    while ( ext_work_queued->size() > 0 )
    {
        ++*ext_work_count_started;
        m_task = ext_work_queued->takeFirst();
        int tasks_total = *ext_work_count_total;
        int tasks_started = *ext_work_count_started;
        int threads_active = ext_threads->size();
        ext_mutex->unlock();


        kDebug() << QString( "[%1 of %2] %3%, %4 threads active" )
                     .arg( tasks_started )
                     .arg( tasks_total )
                     .arg( Coin( tasks_started ) / Coin( tasks_total ) * 100 )
                     .arg( threads_active );

        // run simulation on each map of price data
        for ( int i = 0; i < m_price_data.size(); i++ )
        {
            // print subsimulation if there are multiple
            if ( m_price_data.size() > 1 )
                kDebug() << QString( "[%1 of %2] running sub simulation %3" )
                             .arg( tasks_started )
                             .arg( tasks_total )
                             .arg( i +1 );

            assert( m_price_data[ i ] != nullptr );
            runSimulation( m_price_data[ i ] );
        }

        ext_mutex->lock();
        ++*ext_work_count_done;
        ext_work_done->operator +=( m_task );
    }
    ext_mutex->unlock();

    kDebug() << "finished thread" << QThread::currentThreadId();
}

void SimulationThread::runSimulation( const QMap<Market, PriceData> *const &price_data )
{
//    kDebug() << "running simulation for markets" << price_data->keys();

    static const int BASE_MA_INTERVAL = 300; // 5 minutes per sample
    static const qint64 LATE_START_SAMPLES = 00000;
    static const Coin ORDER_SIZE_LIMIT = Coin( "0.005" );
    static const Coin FEE = -Coin( CoinAmount::SATOSHI * 200 );
    static const bool INVERT_RSI_MA = true;
    const Coin MAX_TAKE_PER_SIMULATION( Coin("0.001428571") * m_task->m_base_ma_length ); // we can only take so much btc per relative base length

    // for score keeping
    Signal base_capital_sma0 = Signal( SMA, qint64( 1000 * 24 * 60 * 60 ) / ( m_task->m_base_ma_length * BASE_MA_INTERVAL ) ); // 1000 days
    Signal base_capital_sma1 = Signal( SMA, qint64( 300 * 24 * 60 * 60 ) / ( m_task->m_base_ma_length * BASE_MA_INTERVAL ) ); // 300 days
    qint64 gain_periods = 0, loss_periods = 0;
    Coin last_capital, gain, loss;
    Coin initial_btc_value, highest_btc_value;

    assert( m_task->m_base_ma_length > 0 );
    assert( m_task->m_rsi_length > 0 );
    assert( BASE_MA_INTERVAL > 0 );
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

//    kDebug() << "start data" << price_data->keys();

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

    QDateTime m_current_date = QDateTime::fromSecsSinceEpoch( m_latest_ts );
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
        qint64 date_test = m_current_date.toSecsSinceEpoch();
        base_elapsed = m_current_date.toSecsSinceEpoch();

        // init ma
        base_ma.setType( SMA ); // set base MA type
        base_ma.setMaxSamples( m_task->m_base_ma_length ); // maintain ma of the last m_base_ma_length samples

        // turn rsi_ma if non-zero
        if ( m_task->m_strategy_signal_type == RSI && m_task->m_rsi_ma_length > 0 )
            strategy_signal.setGeneralOption0( m_task->m_rsi_ma_length );
        strategy_signal.setType( m_task->m_strategy_signal_type );
        strategy_signal.setMaxSamples( m_task->m_rsi_length );

        price_signal.setType( SMA ); // set price MA type
        price_signal.setMaxSamples( m_task->m_base_ma_length );

        assert( base_ma.getSignal().isZero() &&
                strategy_signal.getSignal().isZero() &&
                price_signal.getSignal().isZero() );

        // init start data idx
        const qint64 start_idx = LATE_START_SAMPLES + ( ( m_latest_ts - data.data_start_secs ) / BASE_MA_INTERVAL );
        current_idx = start_idx;

        // fill initial ma samples, ensure base_ma is full before we fill the first strategy sample. fill until
        strategy_signal.resetIntervalCounter( m_task->m_base_ma_length );
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
                strategy_signal.resetIntervalCounter( m_task->m_base_ma_length );
                strategy_signal.addSample( base_ma.getSignal() );

                date_test += BASE_MA_INTERVAL * m_task->m_base_ma_length;
            }

            current_idx++;
            base_elapsed += BASE_MA_INTERVAL;
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
    QDateTime start_date = m_current_date;
    m_current_date.setSecsSinceEpoch( base_elapsed );

//    kDebug() << QString( "latest index-0 market %1  data start date %2(%3) simulation start date %4(%5)" )
//                 .arg( latest_ts_market )
//                 .arg( start_date.toString() )
//                 .arg( start_date.toSecsSinceEpoch() )
//                 .arg( m_current_date.toString() )
//                 .arg( m_current_date.toSecsSinceEpoch() );

    /// step 3: loop until out of data samples. each iteration: set price, update ma, run simulation
    qint64 total_samples = 0; // total sample count, not per-market
    int simulation_keeper = 0;
    do
    {
        // check to run simulation this iteration
        ++total_samples;
        const bool should_run_simulation = ++simulation_keeper % m_task->m_base_ma_length == 0;

        // reset to prevent overflow, also clear prices for each market
        if ( should_run_simulation )
        {
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
            const Coin &price_ahead = data.data.at( current_idx + ( m_task->m_base_ma_length / 2 ) );
            ++current_idx;

            assert( price.isGreaterThanZero() && price_ahead.isGreaterThanZero() );

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

            strategy_signal.resetIntervalCounter( m_task->m_base_ma_length );
            strategy_signal.addSample( base_ma.getSignal() );

            /// fill price/signal price
            Coin signal = strategy_signal.getSignal();

            // don't modulate signal unless the modulator has reached maximum samples
            if ( strategy_signal.isRSISMAPopulated() )
                signal /= ( INVERT_RSI_MA ) ? CoinAmount::COIN / strategy_signal.getRSISMA() :
                                              strategy_signal.getRSISMA();

            const QString &currency = market.getQuote();
            sp.setCurrentPrice( currency, price_signal.getSignal() );
            sp.setSignalPrice( currency, signal );

//            kDebug() << market << "rsi:" << strategy_signal.getSignal();
        }
        m_current_date = m_current_date.addSecs( BASE_MA_INTERVAL );

        if ( !should_run_simulation )
            continue;

        // calculate ratio table
        if ( !sp.calculateAmountToShortLong() )
        {
            kDebug() << "calculateAmountToShortLong() failed";
            return;
        }

        // note: only run this every m_base_ma_length * BASE_MA_INTERVAL seconds
        sp.doCapitalMomentumModulation();

        /// measure if we should make a trade, and add to alphatracker
        const Coin base_capital = sp.getBaseCapital();
        const QMap<QString, Coin> &qsl = sp.getQuantityToShortLongMap();
        //kDebug() << qsl;

        if ( base_capital.isZeroOrLess() )
        {
            kDebug() << "ran out of capital!";
            return;
        }

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

        // record score 5
        if ( last_capital.isGreaterThanZero() )
        {
            if ( base_capital > last_capital )
            {
                gain += ( base_capital / last_capital ) - CoinAmount::COIN;
                gain_periods++;
            }
            else if ( base_capital < last_capital )
            {
                loss += ( last_capital / base_capital ) - CoinAmount::COIN;
                loss_periods++;
            }
        }
        last_capital = base_capital;

//        QMap<QString, Coin> prices = sp.getCurrentPrices();
//        QString prices_str;
//        for ( QMap<QString, Coin>::const_iterator i = prices.begin(); i != prices.end(); i++ )
//            prices_str += QString( " %1" ).arg( i.value() );

//        QMap<QString, Coin> signal_prices = sp.getSignalPrices();
//        QString signal_prices_str;
//        for ( QMap<QString, Coin>::const_iterator i = signal_prices.begin(); i != signal_prices.end(); i++ )
//            signal_prices_str += QString( " %1" ).arg( i.value() );
    }
    while ( m_current_date.toSecsSinceEpoch() < qint64(1591574400) - ( ( ( m_task->m_base_ma_length / 2 ) +1 ) * BASE_MA_INTERVAL ) );

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
                          .arg( m_task->m_base_ma_length )
                          .arg( m_task->m_rsi_length )
                          .arg( m_signal_ma.first().isRSISMAEnabled() ? m_task->m_rsi_ma_length : 0 )
                          .arg( INVERT_RSI_MA ? "I" : "" )
                          .arg( sp.getAllocationFunctionIndex() )
                          .arg( modulation_str )
                          .arg( MAX_TAKE_PER_SIMULATION.toCompact() )
                          .arg( initial_btc_value.toCompact() )
                          .arg( FEE.toCompact() )
                          .arg( m_task->m_base_ma_length )
                          .arg( LATE_START_SAMPLES );
    }

    // submit high score
    const Coin unique_value = CoinAmount::SUBSATOSHI * ( QDateTime::currentSecsSinceEpoch() - qint64(1592625713) );
    const Coin score_0 = CoinAmount::COIN * 10000 * ( base_capital_sma0.getSignal() / initial_btc_value ) / total_samples + unique_value;
    const Coin score_1 = CoinAmount::COIN * 10000 * ( base_capital_sma1.getSignal() / initial_btc_value ) / total_samples + unique_value;
    const Coin score_2 = score_1 / score_0 + unique_value;
    const Coin score_3 = highest_btc_value + unique_value;
    const Coin score_4 = sp.getBaseCapital() + unique_value;
    const Coin score_5 = ( gain / Coin( gain_periods +1 ) ) / ( loss / Coin( loss_periods +1 ) ) + unique_value;

//    kDebug() << "gain periods:" << gain_periods << "gain:" << gain << "loss periods:" << loss_periods << "loss:" << loss;
//    kDebug() << score_0 << score_1 << score_2 << score_3 << score_4 << score_5;

    // insert score
    m_task->m_scores[ 0 ] += score_0;
    m_task->m_scores[ 1 ] += score_1;
    m_task->m_scores[ 2 ] += score_2;
    m_task->m_scores[ 3 ] += score_3;
    m_task->m_scores[ 4 ] += score_4;
    m_task->m_scores[ 5 ] += score_5;

    // append alpha readout
    if ( !m_task->m_alpha_readout.isEmpty() )
        m_task->m_alpha_readout += '\n';

    m_task->m_alpha_readout += alpha.getAlphaReadout();
}

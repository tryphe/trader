#include "tester.h"

#include <math.h>

#include "../daemon/coinamount.h"
#include "../daemon/coinamount_test.h"
#include "../daemon/global.h"
#include "../daemon/misctypes.h"
#include "../daemon/priceaggregator.h"
#include "simulationthread.h"

#include <QString>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>

#include "../daemon/sprucev2.h"

Tester::Tester()
{
    //    kDebug() << sp.allocationFunc21( Coin("1") ) << sp.allocationFunc21( Coin("3") );
//    kDebug() << sp.allocationFunc22( Coin("1") ) << sp.allocationFunc22( Coin("3") );

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

//    generateCustomWork();

    kDebug() << "generated" << m_work_queued.size() << "work units";

    // start threads
    startWork();

    // start work result processing timer
    m_work_timer = new QTimer( this );
    connect( m_work_timer, &QTimer::timeout, this, &Tester::onWorkTimer );
    m_work_timer->setTimerType( Qt::VeryCoarseTimer );
    m_work_timer->setInterval( 10000 );
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
    for ( int allocation_func = 0; allocation_func <= 22; allocation_func++ ) // 30m to 30m, increment 30m
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

    work->m_strategy_signal_type = /*Global::getSecureRandomRange32( 0, 1 ) == 0 ? SMA : */RSI;
    work->m_base_ma_length = 40;
    work->m_rsi_length = std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * 10;
    work->m_rsi_ma_length = std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * 10;
    work->m_allocation_func = Global::getSecureRandomRange32( 0, 22 );

    // select modulation, 50% of the time select up to 2 modulations
    int modulation_count = Global::getSecureRandomRange32( 0, 3 ) -1;
    if ( modulation_count > 0 )
    {
        for ( int i = 0; i < modulation_count; i++ )
        {
            work->m_modulation_length_slow += std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * 10;
            work->m_modulation_length_fast += std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * 10;
            work->m_modulation_factor += CoinAmount::COIN + ( Coin("0.1") * Global::getSecureRandomRange32( 1, 80 ) );
            work->m_modulation_threshold += CoinAmount::COIN + ( Coin("0.1") * Global::getSecureRandomRange32( 0, 30 ) );
        }
    }

    m_work_queued += work;
    m_work_count_total++;
}

void Tester::generateCustomWork()
{
    SimulationTask *work = new SimulationTask;

    work->m_strategy_signal_type = RSI;
    work->m_base_ma_length = 40;
    work->m_rsi_length = 75;
    work->m_rsi_ma_length = 25;
    work->m_allocation_func = 21;

    m_work_queued += work;
    m_work_count_total++;
}

void Tester::startWork()
{
    // init threads
    for ( int i = 0; i < MAX_WORKERS; i++ )
    {
        SimulationThread *t = new SimulationThread( i );
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

        const QString score_out = QString( "------------------------------------------------------------------------------------\nscore-1000d[%1] score-300d[%2] score-300d/1000d[%3] peak[%4] final[%5]: %6\n%7" )
                                   .arg( scores[ 0 ] )
                                   .arg( scores[ 1 ] )
                                   .arg( scores[ 2 ] )
                                   .arg( scores[ 3 ] )
                                   .arg( scores[ 4 ] )
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

    kDebug() << QString( "[%1 of %2] %3% done, %4 threads active" )
                 .arg( m_work_count_done )
                 .arg( m_work_count_total )
                 .arg( Coin( m_work_count_done ) / Coin( m_work_count_total ) * 100 )
                 .arg( m_threads.size() );

    if ( m_work_queued.size() == 0 &&
         m_work_done.size() == 0 &&
         m_work_count_total == m_work_count_done )
    {
        kDebug() << "done!";
        lock0.unlock();
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

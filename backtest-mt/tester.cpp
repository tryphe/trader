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
#include <QMessageLogger>

#include "../daemon/sprucev2.h"

Tester::Tester()
{
    SignalTest t;
    t.test();

    CoinAmountTest c;
    c.test();

    // load data
    loadPriceData();

    kDebug() << "loading finished work...";
    loadFinishedWork();

    kDebug() << "generating work...";

    // generate work
    if ( WORK_RANDOM )
        for ( int i = 0; i < WORK_RANDOM_TRIES; i++ )
            generateRandomWork();
    else
        generateWork();

    kDebug() << "generated" << m_work_queued.size() << "work units, skipped" << m_work_skipped_duplicate << "duplicate work units";

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
    // TODO: add m_samples_start_offset, m_markets_tested

//    // non-loop initialized options to generate
//    QVector<int> modulation_length_slow;
//    QVector<int> modulation_length_fast;
//    QVector<Coin> modulation_factor;
//    QVector<Coin> modulation_threshold;

//    for ( int signal_type = SMA; signal_type <= RSI; signal_type++ )
//    {
//    for ( int allocation_func = 0; allocation_func <= 22; allocation_func++ ) // 30m to 30m, increment 30m
//    {
//    for ( int base_ma_length = 10; base_ma_length <= 1000; base_ma_length *= 2 ) // 40 == 3.3h
//    {
//    for ( int rsi_length = 10; rsi_length <= 1000; rsi_length *= 3 ) // 1h to 1d
//    {
//    for ( int rsi_ma_length = 10; rsi_ma_length <= 1000; rsi_ma_length *= 3 )
//    {
//    // modulation 1
////    for ( modulation_length_slow += 250; modulation_length_slow[ 0 ] <= 500; modulation_length_slow[ 0 ] *= 2 )
////    {
////    for ( modulation_length_fast += 10; modulation_length_fast[ 0 ] <= 30; modulation_length_fast[ 0 ] *= 3 )
////    {
////    for ( modulation_factor += Coin( "1.1" ); modulation_factor[ 0 ] <= Coin( "8.8" ); modulation_factor[ 0 ] *= Coin( "2" ) )
////    {
////    for ( modulation_threshold += Coin( "1" ); modulation_threshold[ 0 ] <= Coin( "1.95" ); modulation_threshold[ 0 ] *= Coin( "1.2" ) )
////    {
////    // modulation 2
////    for ( modulation_length_slow += 250; modulation_length_slow[ 1 ] <= 500; modulation_length_slow[ 1 ] *= 2 )
////    {
////    for ( modulation_length_fast += 10; modulation_length_fast[ 1 ] <= 30; modulation_length_fast[ 1 ] *= 3 )
////    {
////    for ( modulation_factor += Coin( "1.1" ); modulation_factor[ 1 ] <= Coin( "8.8" ); modulation_factor[ 1 ] *= Coin( "2" ) )
////    {
////    for ( modulation_threshold += Coin( "1" ); modulation_threshold[ 1 ] <= Coin( "1.95" ); modulation_threshold[ 1 ] *= Coin( "1.2" ) )
////    {
//    // modulation 3
////    for ( modulation_length_slow += 62; modulation_length_slow[ 2 ] <= 248; modulation_length_slow[ 2 ] *= 2 )
////    {
////    for ( modulation_length_fast += 6; modulation_length_fast[ 2 ] <= 12; modulation_length_fast[ 2 ] *= 2 )
////    {
////    for ( modulation_factor += Coin( "1" ); modulation_factor[ 2 ] <= Coin( "9" ); modulation_factor[ 2 ] *= Coin( "3" ) )
////    {
////    for ( modulation_threshold += Coin( "1" ); modulation_threshold[ 2 ] <= Coin( "1" ); modulation_threshold[ 2 ] *= Coin( "2" ) )
////    {
//        // generate next work
//        SimulationTask *work = new SimulationTask;
//        work->m_strategy_signal_type = static_cast<SignalType>( signal_type );
//        work->m_base_ma_length = base_ma_length;
//        work->m_rsi_length = rsi_length;
//        work->m_rsi_ma_length = rsi_ma_length;
//        work->m_allocation_func = allocation_func;

//        work->m_modulation_length_slow = modulation_length_slow;
//        work->m_modulation_length_fast = modulation_length_fast;
//        work->m_modulation_factor = modulation_factor;
//        work->m_modulation_threshold = modulation_threshold;

//        m_work_queued += work;
//        m_work_count_total++;
////    }
////    modulation_threshold.remove( 2 );
////    }
////    modulation_factor.remove( 2 );
////    }
////    modulation_length_fast.remove( 2 );
////    }
////    modulation_length_slow.remove( 2 );
////    }
////    modulation_threshold.remove( 1 );
////    }
////    modulation_factor.remove( 1 );
////    }
////    modulation_length_fast.remove( 1 );
////    }
////    modulation_length_slow.remove( 1 );
////    }
////    modulation_threshold.remove( 0 );
////    }
////    modulation_factor.remove( 0 );
////    }
////    modulation_length_fast.remove( 0 );
////    }
////    modulation_length_slow.remove( 0 );
//    }
//    }
//    }
//    }
//    }
}

void Tester::generateRandomWork()
{
    SimulationTask *work = new SimulationTask;

    const int base_multiplier = 10;
    work->m_markets_tested += m_price_data_0.first().keys().toVector();
    work->m_markets_tested += m_price_data_1.first().keys().toVector();

    work->m_samples_start_offset = WORK_SAMPLES_START_OFFSET;
    work->m_strategy_signal_type = /*Global::getSecureRandomRange32( 0, 1 ) == 0 ? SMA : */RSI;
    work->m_base_ma_length = 40;
    work->m_rsi_length = std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * base_multiplier;
    work->m_rsi_ma_length = std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * base_multiplier;
    work->m_allocation_func = Global::getSecureRandomRange32( 0, 22 );

    // select modulation, 50% of the time select up to 2 modulations
//    const int modulation_count = Global::getSecureRandomRange32( 0, 3 ) -1;
//    if ( modulation_count > 0 )
//    {
//        for ( int i = 0; i < modulation_count; i++ )
//        {
//            work->m_modulation_length_slow += std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * base_multiplier;
//            work->m_modulation_length_fast += std::pow( Global::getSecureRandomRange32( 1, 15 ), 2 ) * base_multiplier;
//            work->m_modulation_factor += CoinAmount::COIN + ( Coin("0.1") * Global::getSecureRandomRange32( 1, 80 ) );
//            work->m_modulation_threshold += CoinAmount::COIN + ( Coin("0.1") * Global::getSecureRandomRange32( 0, 30 ) );
//        }
//    }

    // if hash of raw data exists in QSet, skip adding duplicate work
    const QByteArray work_raw = work->getRaw();
    if ( m_work_ids_generated_or_done.contains( work_raw ) )
    {
        m_work_skipped_duplicate++;
        delete work;
        return;
    }

    m_work_ids_generated_or_done += work_raw;
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

        // normalize scores
        for ( int score_type = 0; score_type < task->m_scores.size(); score_type++ )
            task->m_scores[ score_type ] = task->m_scores.value( score_type ) / MARKET_VARIATIONS;

        // prepend scores to simulation result
        const QString score_str = QString( "1000d[%1]-300d[%2]-300d/1000d[%3]-peak[%4]-final[%5]:" )
                .arg( task->m_scores[ 0 ], -12, QChar('0') )
                .arg( task->m_scores[ 1 ], -12, QChar('0') )
                .arg( task->m_scores[ 2 ], -12, QChar('0') )
                .arg( task->m_scores[ 3 ], -12, QChar('0') )
                .arg( task->m_scores[ 4 ], -12, QChar('0') );

        task->m_simulation_result.prepend( score_str );

//        const QString score_out = QString( "------------------------------------------------------------------------------------\n%1\n%2" )
//                                   .arg( task->m_simulation_result )
//                                   .arg( task->m_alpha_readout );

        // copy scores into local data
        for ( int score_type = 0; score_type < task->m_scores.size(); score_type++ )
        {
            const Coin &score = task->m_scores[ score_type ];

            m_highscores_by_result[ score_type ][ task->m_simulation_result ] = score;
            m_highscores_by_score[ score_type ].insert( score, task->m_simulation_result );
        }

        m_work_results_unsaved[ task->getRaw() ] = task->m_simulation_result;
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

    printHighScores( m_highscores_by_score[ 0 ], out_savefile, "1000d" );
    printHighScores( m_highscores_by_score[ 1 ], out_savefile, "300d" );
    printHighScores( m_highscores_by_score[ 2 ], out_savefile, "300d/1000d" );
    printHighScores( m_highscores_by_score[ 3 ], out_savefile, "PEAK" );
    printHighScores( m_highscores_by_score[ 4 ], out_savefile, "FINAL" );

    // save the buffer
    out_savefile.flush();
    savefile.close();

    kDebug() << QString( "[%1 of %2] %3% done, %4 threads active" )
                 .arg( m_work_count_done )
                 .arg( m_work_count_total )
                 .arg( Coin( m_work_count_done ) / Coin( m_work_count_total ) * 100 )
                 .arg( m_threads.size() );

    // save unsaved work
    saveFinishedWork();

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

void Tester::printHighScores( const QMultiMap<Coin, QString> &scores, QTextStream &out, QString description, const int print_count )
{
    Global::centerString( description, QChar('='), 40 );

    out << description;
    kDebug() << description;

    const int score_count = scores.size();
    int scores_iterated = 0;
    for ( QMultiMap<Coin, QString>::const_iterator j = scores.constBegin(); j != scores.constEnd(); j++ )
    {
        // note: this is a multimap, so we can see identical scores with different configurations

        // only show the 5 highest scores
        if ( ++scores_iterated < score_count - print_count +1 )
            continue;

        out << j.value();
        kDebug() << j.value();
    }
}

void Tester::saveFinishedWork()
{
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
    for ( QMap<QByteArray, QString>::const_iterator i = m_work_results_unsaved.constBegin(); i != m_work_results_unsaved.constEnd(); i++ )
    {
        QByteArray work_id = i.key().toHex();
        const QString &work_result = i.value();

        // save id
        out_savefile << work_id << ' ';

        // save scores
        for ( QMap<int, QMap<QString, Coin>>::const_iterator j = m_highscores_by_result.constBegin(); j != m_highscores_by_result.constEnd(); j++ )
            out_savefile << j.value()[ work_result ] << ' ';

        // save result
        out_savefile << work_result << '\n';

        // transfer into saved map, and queue for removal from unsaved map
        m_work_results_saved[ work_id ] = i.value();
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
        return;
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    QList<QByteArray> data = loadfile.readAll().split( '\n' );
    int results_loaded = 0;

    QByteArray work_id;
    QString result, score_0, score_1, score_2, score_3, score_4;
    for ( int i = 0; i < data.size(); i++ )
    {
        QList<QByteArray> line_data = data[ i ].split( ' ' );

        // skip last empty line
        if ( line_data.size() != 7 )
            continue;

        work_id = QByteArray::fromHex( line_data[ 0 ] );
        result = line_data[ 6 ];

        // TODO: add loops for this
        score_0 = line_data[ 1 ];
        score_1 = line_data[ 2 ];
        score_2 = line_data[ 3 ];
        score_3 = line_data[ 4 ];
        score_4 = line_data[ 5 ];

        m_highscores_by_result[ 0 ][ result ] = score_0;
        m_highscores_by_result[ 1 ][ result ] = score_1;
        m_highscores_by_result[ 2 ][ result ] = score_2;
        m_highscores_by_result[ 3 ][ result ] = score_3;
        m_highscores_by_result[ 4 ][ result ] = score_4;

        m_highscores_by_score[ 0 ].insert( score_0, result );
        m_highscores_by_score[ 1 ].insert( score_1, result );
        m_highscores_by_score[ 2 ].insert( score_2, result );
        m_highscores_by_score[ 3 ].insert( score_3, result );
        m_highscores_by_score[ 4 ].insert( score_4, result );

        m_work_ids_generated_or_done += work_id;
        m_work_results_saved[ work_id ] = result;

        results_loaded++;
    }

    kDebug() << "loaded" << results_loaded << "historical work results";
}

void Tester::onWorkTimer()
{
    m_work_timer->stop();

    processFinishedWork();

    m_work_timer->start();
}

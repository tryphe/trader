#include "priceaggregator.h"
#include "misctypes.h"
#include "market.h"
#include "enginemap.h"
#include "engine.h"
#include "global.h"

#include <QString>
#include <QMap>
#include <QDateTime>
#include <QTimer>

static const bool prices_uses_avg = false; // false = assemble widest combined spread between all exchanges, true = average spreads between all exchanges

PriceAggregatorConfig::PriceAggregatorConfig() {}

PriceAggregatorConfig::PriceAggregatorConfig( const QString &_market,
                                              const int _base_interval_secs,
                                              const int _base_len,
                                              const int _strategy_length,
                                              const int _strategy_length_internal_option0,
                                              const int _interval_counter,
                                              const Coin jumpstart_price )
    : market( _market ),
      base_interval_secs( _base_interval_secs ),
      base_length( _base_len ),
      strategy_length( _strategy_length )
{
    // configure base signal
    signal_base.setSignalArgs( SMA, base_length );

    // configure strategy signal
    signal_strategy.setSignalArgs( RSIRatio, strategy_length );
//    signal_strategy.resetIntervalCounter( _interval_counter );

    // exit if not jumpstarting state (aggregator should load if samples == 0)
    if ( jumpstart_price.isZeroOrLess() )
        return;

    jumpstart( jumpstart_price );
}

void PriceAggregatorConfig::jumpstart( const Coin &price )
{
//    signal_strategy.resetIntervalCounter( base_length );
//    while ( !signal_strategy.hasSignal() )
//    {
//        base_data.data += price;
//        signal_base.addSample( price );

//        signal_strategy.iterateIntervalCounter();
//        if ( signal_strategy.shouldUpdate() )
//        {
//            signal_strategy.resetIntervalCounter( base_length );
//            signal_strategy.addSample( signal_base.getSignal() );
//        }
//    }

    // set ma start times to now (we updated all of the samples)
    const qint64 epoch_secs = QDateTime::currentSecsSinceEpoch();
    base_data.data_start_secs = epoch_secs - ( base_data.data.size() * base_interval_secs );

    kDebug() << "[PriceAggregator] jumpstarted" << market
             << ", signal_base:" << signal_base.getSignal() << "x" << signal_base.getCurrentSamples()
             << ", signal_strategy:" << signal_strategy.getSignal() << "x" << signal_strategy.getCurrentSamples();
}

PriceAggregator::PriceAggregator( EngineMap *_engine_map )
    : m_engine_map( _engine_map )
{
    // this timer records price samples
    m_timer = new QTimer( this );
    connect( m_timer, &QTimer::timeout, this, &PriceAggregator::onTimerUp );
    m_timer->setTimerType( Qt::VeryCoarseTimer );
    m_timer->start( 60000 ); // 1 min
}

PriceAggregator::~PriceAggregator()
{
    m_timer->stop();
    delete m_timer;
}

QString PriceAggregator::getSettingsPath()
{
    return Global::getTraderPath() + QDir::separator() + "price.settings";
}

QString PriceAggregator::getSamplesPath( const QString &market, const qint64 interval_secs )
{
    return QString( "%1%2%3.%4.%5" )
           .arg( Global::getTraderPath() )
           .arg( QDir::separator() )
           .arg( "price.samples" )
           .arg( market )
           .arg( interval_secs );
}

void PriceAggregator::savePriceSamples( const PriceAggregatorConfig &config, const QString filename_override )
{
    // open file. if custom filename is supplied use it, otherwise choose a filename
    const QString path = !filename_override.isEmpty() ? filename_override :
                                                        getSamplesPath( config.market, config.base_interval_secs );

    QFile::remove( path );
    QFile savefile( path );
    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "[PriceAggregator] error: couldn't open price sample file" << path;
        return;
    }

    // save state
    QTextStream out( &savefile );
    // prepend a marker, and the start date
    out << QString( "p %1" ).arg( config.base_data.data_start_secs );
    for ( QVector<Coin>::const_iterator i = config.base_data.data.begin(); i != config.base_data.data.end(); i++ )
        out << QChar(' ') << (*i).toCompact();

    // close file
    out.flush();
    savefile.close();

    // write stuff
    kDebug() << "[PriceAggregator] success!" << config.base_data.data.size() << "samples saved to" << path;
}

bool PriceAggregator::loadPriceSamples( PriceData &data, const QString &path )
{
    static const QChar separator = QChar(' ');

    // open sample file
    QFile sample_file( path );
    if ( !sample_file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        kDebug() << "[PriceAggregator] error: couldn't load sample file" << path;
        return false;
    }

    const QByteArray data_in = sample_file.readAll();

    // close file
    sample_file.close();

    // check prefix
    if ( !data_in.startsWith( "p " ) )
    {
        kDebug() << "[PriceAggregator] error: bad prefix in sample file" << path;
        return false;
    }

    // init counters
    int z = 2;
    int s = data_in.indexOf( separator, z );

    // read timestamp
    bool ok = false;
    qint64 ts = data_in.mid( z, s -1 ).toLongLong( &ok );
    z += s -1;
    if ( !ok || ts < 1 )
    {
        kDebug() << "[PriceAggregator] bad timestamp" << ts;
        return false;
    }

    data.data_start_secs = ts;
    //kDebug() << "read timestamp" << ts;

    // read the data
    int s_minus_z;
    do
    {
        s = data_in.indexOf( separator, z );
        s_minus_z = s-z;

        Coin sample = QString( data_in.mid( z, s_minus_z ) );

        // check for bad sample
        if ( sample.isZeroOrLess() )
        {
            kDebug() << "[PriceAggregator] bad sample" << sample;
            return false;
        }

        z += s_minus_z +1;
        data.data += sample;
        //kDebug() << "read sample" << sample;
    }
    while ( s > -1 );

    kDebug() << "[PriceAggregator] loaded" << path << "," << data.data.size() << "samples";
    return true;
}

Spread PriceAggregator::getSpread( const QString &market ) const
{
    const qint64 epoch_msecs = QDateTime::currentMSecsSinceEpoch();

    Spread ret;
    quint16 samples = 0;

    for ( EngineMap::const_iterator i = m_engine_map->begin(); i != m_engine_map->end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->getMarketInfoStructure().contains( market ) )
            continue;

        // ensure ticker isn't stale
        if ( engine->rest_arr.at( engine->engine_type )->ticker_update_time < epoch_msecs - 60000 )
            continue;

        const MarketInfo &info = engine->getMarketInfo( market );

        // ensure prices are valid
        if ( !info.spread.isValid() )
            continue;

        // use avg spread
        if ( prices_uses_avg )
        {
            samples++;

            // incorporate prices of this exchange
            ret.bid += info.spread.bid;
            ret.ask += info.spread.ask;
        }
        // or, use combined spread edges
        else
        {
            // incorporate bid price of this exchange
            if ( ret.bid.isZeroOrLess() || // bid doesn't exist yet
                 ret.bid < info.spread.bid ) // bid is higher than the exchange bid
                ret.bid = info.spread.bid;

            // incorporate ask price of this exchange
            if ( ret.ask.isZeroOrLess() || // ask doesn't exist yet
                 ret.ask > info.spread.ask ) // ask is lower than the exchange ask
                ret.ask = info.spread.ask;
        }
    }

    if ( prices_uses_avg )
    {
        // on 0 samples, return here
        if ( samples < 1 )
            return Spread();

        // divide by num of samples if necessary
        if ( samples > 1 )
        {
            ret.bid /= samples;
            ret.ask /= samples;
        }
    }

    if ( !ret.isValid() )
        return Spread();

    return ret;
}

Coin PriceAggregator::getStrategySignal( const QString &market ) /*const*/
{
    if ( !m_config.contains( market ) )
        return Coin();

    return m_config[ market ].signal_strategy.getSignal();
}

void PriceAggregator::addPersistentMarket( const PriceAggregatorConfig &config )
{
    Market m( config.market );

    if ( !m.isValid() )
    {
        kDebug() << "[PriceAggregator] error: ignoring invalid market" << config.market;
        return;
    }

    kDebug() << "[PriceAggregator] added persistent market" << config.market;

    // add base to currencies
    if ( !m_currencies.contains( m.getBase() ) )
        m_currencies += m.getBase();

    // add quote to currencies
    if ( !m_currencies.contains( m.getQuote() ) )
        m_currencies += m.getQuote();

    // add tracked market
    if ( !m_markets.contains( config.market ) )
        m_markets += config.market;

    // overwrite config for market
    m_config[ config.market ] = config;
}

void PriceAggregator::saveMaybe()
{
    // save only if necessary
    if ( m_last_save_secs > QDateTime::currentSecsSinceEpoch() - m_save_interval_secs )
        return;

    save();
}

void PriceAggregator::save()
{
    // always save if this function is called, but only adjust last save time if necessary
    if ( m_last_save_secs <= QDateTime::currentSecsSinceEpoch() - m_save_interval_secs )
        m_last_save_secs += m_save_interval_secs;

    saveConfig();
    savePrices();
}

void PriceAggregator::saveConfig()
{
    // open file
    const QString path = getSettingsPath();

    QFile savefile( path );
    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "[PriceAggregator] error: couldn't open config file" << path;
        return;
    }

    // construct state
    QString state;
    for ( QMap<QString, PriceAggregatorConfig>::const_iterator i = m_config.begin(); i != m_config.end(); i++ )
    {
        const PriceAggregatorConfig &config = i.value();
        state += QString( "setpricetracking %1 %2 %3 %4 %5 %6\n" )
                  .arg( config.market )
                  .arg( config.base_interval_secs )
                  .arg( config.base_length )
                  .arg( config.strategy_length )
                  .arg( 0 ) //config.signal_strategy.getGeneralOption0() )
                  .arg( 0 ); //config.signal_strategy.getIntervalCounter() );
    }

    // save state
    QTextStream out_config( &savefile );
    out_config << state;

    // close file
    out_config.flush();
    savefile.close();
}

void PriceAggregator::savePrices()
{
    for ( QMap<QString, PriceAggregatorConfig>::const_iterator i = m_config.begin(); i != m_config.end(); i++ )
    {
        const PriceAggregatorConfig &config = i.value();

        savePriceSamples( config );
    }
}


void PriceAggregator::load()
{
    loadConfig();
    loadPrices();
}

void PriceAggregator::loadConfig()
{
    // load config
    const QString config_path = getSettingsPath();

    QFile config_file( config_path );
    if ( !config_file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        kDebug() << "[PriceAggregator] warning: couldn't load price aggregator config" << config_path;
        return;
    }

    if ( config_file.bytesAvailable() == 0 )
        return;

    // emit config
    QString data = config_file.readAll();
    kDebug() << "[PriceAggregator] loaded price tracking settings," << data.size() << "bytes.";

    emit gotUserCommandChunk( data );
}

void PriceAggregator::loadPrices()
{
    // tag markets with no sample file (they must be jumpstarted)
    QVector<QString> invalid_markets;

    // find price files
    for ( QMap<QString, PriceAggregatorConfig>::iterator i = m_config.begin(); i != m_config.end(); i++ )
    {
        PriceAggregatorConfig &config = i.value();

        // if we jumpstarted the samples, continue
        if ( !config.base_data.data.isEmpty() )
            continue;

        const bool ret0 = loadPriceSamples( config.base_data, getSamplesPath( config.market, config.base_interval_secs ) );

        if ( !ret0 )
        {
            invalid_markets += i.key();
            continue;
        }

        // load price samples into signals
        for ( QVector<Coin>::const_iterator j = config.base_data.data.begin(); j != config.base_data.data.end(); j++ )
        {
            const Coin &price = *j;

            config.signal_base.addSample( price );

//            config.signal_strategy.iterateIntervalCounter();
//            if ( config.signal_strategy.shouldUpdate() )
//            {
//                config.signal_strategy.resetIntervalCounter( config.base_length );
//                config.signal_strategy.addSample( config.signal_base.getSignal() );
//            }
        }
    }

    // remove invalid markets
    while ( !invalid_markets.isEmpty() )
        m_config.remove( invalid_markets.takeLast() );
}

void PriceAggregator::onTimerUp()
{
    nextPriceSample();
    saveMaybe();
}

void PriceAggregator::nextPriceSample()
{
    const qint64 epoch_secs = QDateTime::currentSecsSinceEpoch();

    // gather spreads for all markets in config
    for ( QMap<QString, PriceAggregatorConfig>::iterator i = m_config.begin(); i != m_config.end(); i++ )
    {
        PriceAggregatorConfig &config = i.value();

        // get center of spread price
        const Spread spread = getSpread( config.market );
        const Coin price = spread.getMidPrice();

        // ensure sample is valid
        if ( !spread.isValid() || price.isZeroOrLess() )
            continue;

        /// fill small_ma

        // fill all missing small_ma samples with the new sample
        qint64 samples_filled = 0;
        qint64 samples_to_fill = epoch_secs - ( config.base_data.data_start_secs + ( config.base_interval_secs * config.signal_base.getCurrentSamples() ) ) / config.base_interval_secs;

        while ( samples_to_fill > 0 )
        {
            config.base_data.data += price;
            config.signal_base.addSample( price );

//            config.signal_strategy.iterateIntervalCounter();
//            if ( config.signal_strategy.shouldUpdate() )
//            {
//                config.signal_strategy.resetIntervalCounter( config.base_length );
//                config.signal_strategy.addSample( config.signal_base.getSignal() );
//            }

            samples_to_fill--;
            samples_filled++;

            //kDebug() << config.market << "small_ma updated:" << config.small_ma.base_ma.getSignal();
        }

        if ( samples_filled > 1 )
            kDebug() << "[PriceAggregator] warning:" << config.market << "base_data filled" << samples_filled << "samples this iteration";
    }
}

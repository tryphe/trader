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

PriceAggregatorConfig::PriceAggregatorConfig() {}

PriceAggregatorConfig::PriceAggregatorConfig( const QString &_market, const int _small_ma_interval_secs, const int _signal_ma_interval_secs, const int _signal_ma_length, const Coin jumpstart_price )
    : market( _market ),
      small_ma_interval_secs( _small_ma_interval_secs ),
      signal_ma_interval_secs( _signal_ma_interval_secs ),
      signal_ma_length( _signal_ma_length )
{
    small_ma.base_ma.setMaxSamples( getSmallMALength() );
    signal_ma.base_ma.setMaxSamples( signal_ma_length );

    // exit if not jumpstarting state (aggregator should load if samples == 0)
    if ( jumpstart_price.isZeroOrLess() )
        return;

    jumpstart( jumpstart_price );
}

void PriceAggregatorConfig::jumpstart( const Coin &price )
{
    const int small_ma_length = getSmallMALength();

    // fill small_ma samples
    for ( int i = 0; i < small_ma_length; i++ )
    {
        small_ma.base_ma.addSample( price );
        small_ma.data += price;
    }

    // fill signal_ma samples
    for ( int i = 0; i < signal_ma_length; i++ )
    {
        signal_ma.base_ma.addSample( price );
        signal_ma.data += price;
    }

    // set ma start times to now (we updated all of the samples)
    const qint64 epoch_secs = QDateTime::currentSecsSinceEpoch();
    small_ma.data_start_secs = small_ma.start_secs = epoch_secs - ( small_ma_length * small_ma_interval_secs );
    signal_ma.data_start_secs = signal_ma.start_secs = epoch_secs - ( signal_ma_length * signal_ma_interval_secs );

    kDebug() << "[PriceAggregator] jumpstarted" << market
             << ", small_ma:" << small_ma.base_ma.getSignal() << "x" << small_ma.base_ma.getCurrentSamples()
             << ", signal_ma:" << signal_ma.base_ma.getSignal() << "x" << signal_ma.base_ma.getCurrentSamples();
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

void PriceAggregator::savePriceSamples( const QString &market, const qint64 sample_interval, const qint64 data_start_secs, const QVector<Coin> &data )
{
    // open file
    const QString path = getSamplesPath( market, sample_interval );

    QFile savefile( path );
    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "[PriceAggregator] error: couldn't open price sample file" << path;
        return;
    }

    // construct state
    QString state;
    for ( QVector<Coin>::const_iterator i = data.begin(); i != data.end(); i++ )
    {
        if ( !state.isEmpty() )
            state += QChar( ' ' );

        state += QString( "%1" ).arg( Coin( *i ).toCompact() );
    }
    state.prepend( QString( "p %1 " ).arg( data_start_secs ) );

    // save state
    QTextStream out_samples( &savefile );
    out_samples << state;

    // close file
    out_samples.flush();
    savefile.close();
}

bool PriceAggregator::loadPriceSamples( PriceData &data, const QString &path, const int ma_length, const int ma_interval )
{
    static const QChar separator = QChar(' ');

    kDebug() << "[PriceAggregator] loading" << path;

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

    // after loading the data, calculate the ma start time
    const int samples_count = data.data.size();
    const int start_idx = std::max( 0, samples_count - ma_length );
    data.start_secs = data.data_start_secs + start_idx * ma_interval;

    // load ma samples starting at start_secs
    for ( int i = start_idx; i < samples_count; i++ )
        data.base_ma.addSample( data.data.value( i ) );

    kDebug() << "[PriceAggregator] loaded" << data.data.size() << "samples, ma:" << data.base_ma.getSignal() << "ma length:" << ma_length;

    return true;
}

Spread PriceAggregator::getSpread( const QString &market ) const
{
    static const bool prices_uses_avg = true; // false = assemble widest combined spread between all exchanges, true = average spreads between all exchanges

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
                 ret.bid > info.spread.bid ) // bid is higher than the exchange bid
                ret.bid = info.spread.bid;

            // incorporate ask price of this exchange
            if ( ret.ask.isZeroOrLess() || // ask doesn't exist yet
                 ret.ask < info.spread.ask ) // ask is lower than the exchange ask
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

Coin PriceAggregator::getSignalMA( const QString &market ) const
{
    if ( !m_config.contains( market ) )
        return Coin();

    return m_config[ market ].signal_ma.base_ma.getSignal();
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
        state += QString( "setpricetracking %1 %2 %3 %4\n" )
                  .arg( config.market )
                  .arg( config.small_ma_interval_secs )
                  .arg( config.signal_ma_interval_secs )
                  .arg( config.signal_ma_length );
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

        savePriceSamples( config.market, config.small_ma_interval_secs, config.small_ma.data_start_secs, config.small_ma.data );
        savePriceSamples( config.market, config.signal_ma_interval_secs, config.signal_ma.data_start_secs, config.signal_ma.data );
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
        if ( !config.small_ma.data.isEmpty() )
            continue;

        bool ret0 = loadPriceSamples( config.small_ma, getSamplesPath( config.market, config.small_ma_interval_secs ), config.getSmallMALength(), config.small_ma_interval_secs );
        bool ret1 = loadPriceSamples( config.signal_ma, getSamplesPath( config.market, config.signal_ma_interval_secs ), config.signal_ma_length, config.signal_ma_interval_secs );

        if ( !ret0 || !ret1 )
            invalid_markets += i.key();
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
        Spread spread = getSpread( config.market );
        Coin midprice = spread.getMidPrice();

        // ensure sample is valid
        if ( !spread.isValid() || midprice.isZeroOrLess() )
            continue;

        /// fill small_ma

        // fill all missing small_ma samples with the new sample
        qint64 small_ma_filled = 0;
        qint64 small_ma_to_fill = ( epoch_secs - config.small_ma.start_secs ) / config.small_ma_interval_secs - config.small_ma.base_ma.getCurrentSamples();

        while ( small_ma_to_fill > 0 )
        {
            config.small_ma.base_ma.addSample( midprice );
            config.small_ma.data += midprice;
            config.small_ma.start_secs += config.small_ma_interval_secs;
            small_ma_to_fill--;
            small_ma_filled++;

            //kDebug() << config.market << "small_ma updated:" << config.small_ma.base_ma.getSignal();
        }

        if ( small_ma_filled > 1 )
            kDebug() << "[PriceAggregator] warning: small_ma filled" << small_ma_filled << "samples this iteration";

        /// fill signal_ma

        // fill all missing signal_ma samples with the latest small_ma sample
        qint64 signal_ma_filled = 0;
        qint64 signal_ma_to_fill = ( epoch_secs - config.signal_ma.start_secs ) / config.signal_ma_interval_secs - config.signal_ma.base_ma.getCurrentSamples();

        while ( signal_ma_to_fill > 0 )
        {
            config.signal_ma.base_ma.addSample( config.small_ma.base_ma.getSignal() );
            config.signal_ma.data += config.small_ma.base_ma.getSignal();
            config.signal_ma.start_secs += config.signal_ma_interval_secs;
            signal_ma_to_fill--;
            signal_ma_filled++;

            //kDebug() << config.market << "signal_ma updated:" << config.signal_ma.base_ma.getSignal();
        }

        if ( signal_ma_filled > 1 )
            kDebug() << "[PriceAggregator] warning: signal_ma filled" << signal_ma_filled << "samples this iteration";
    }
}

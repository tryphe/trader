#ifndef COMMANDRUNNER_H
#define COMMANDRUNNER_H

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

class Engine;
class Spruce;
class SpruceOverseer;
class BaseREST;
class TrexREST;
class BncREST;
class PoloREST;
class WavesREST;
class Stats;

class CommandRunner : public QObject
{
    Q_OBJECT

    friend class Trader;

public:
    explicit CommandRunner( const quint8 _engine_type, Engine *_e, QVector<BaseREST *> _rest_arr, QObject *parent = nullptr );
    ~CommandRunner();

signals:
    void exitSignal();

public slots:
    void runCommandChunk( QString &s );

private:
    bool checkArgs( const QStringList &args, qint32 expected_args_min, qint32 expected_args_max = -1 ); // -1 sets max=min

    void command_getbalances( QStringList &args );
    void command_getlastprices( QStringList &args );
    void command_getbuyselltotal( QStringList &args );
    void command_cancelall( QStringList &args );
    void command_cancellocal( QStringList &args );
    void command_cancelhighest( QStringList &args );
    void command_cancellowest( QStringList &args );
    void command_getorders( QStringList &args );
    void command_getpositions( QStringList &args );
    void command_getordersbyindex( QStringList &args );
    void command_setorder( QStringList &args );
    void command_setordermin( QStringList &args );
    void command_setordermax( QStringList &args );
    void command_setorderdc( QStringList &args );
    void command_setorderdcnice( QStringList &args );
    void command_setorderlandmarkthresh( QStringList &args );
    void command_setorderlandmarkstart( QStringList &args );
    void command_long( QStringList &args );
    void command_longindex( QStringList &args );
    void command_short( QStringList &args );
    void command_shortindex( QStringList &args );
    void command_setcancelthresh( QStringList &args );
    void command_setkeyandsecret( QStringList &args );
    void command_getdailyvolume( QStringList &args );
    void command_getdailyfills( QStringList &args );
    void command_getalpha( QStringList &args );
    void command_setalphamanual( QStringList &args );
    void command_getdailymarketvolume( QStringList &args );
    void command_getshortlong( QStringList &args );
    void command_gethibuylosell( QStringList &args );
    void command_setmarketsettings( QStringList &args );
    void command_setmarketoffset( QStringList &args );
    void command_setmarketsentiment( QStringList &args );
    void command_setnaminterval( QStringList &args );
    void command_setbookinterval( QStringList &args );
    void command_settickerinterval( QStringList &args );
    void command_setgracetimelimit( QStringList &args );
    void command_setcheckinterval( QStringList &args );
    void command_setdcinterval( QStringList &args );
    void command_setclearstrayorders( QStringList &args );
    void command_setclearstrayordersall( QStringList &args );
    void command_setslippagecalculated( QStringList &args );
    void command_setadjustbuysell( QStringList &args );
    void command_setdcslippage( QStringList &args );
    void command_setorderbookstaletolerance( QStringList &args );
    void command_setsafetydelaytime( QStringList &args );
    void command_settickersafetydelaytime( QStringList &args );
    void command_setslippagestaletime( QStringList &args );
    void command_setqueuedcommandsmax( QStringList &args );
    void command_setqueuedcommandsmaxdc( QStringList &args );
    void command_setsentcommandsmax( QStringList &args );
    void command_settimeoutyield( QStringList &args );
    void command_setrequesttimeout( QStringList &args );
    void command_setcanceltimeout( QStringList &args );
    void command_setslippagetimeout( QStringList &args );
    void command_setspruceinterval( QStringList &args );
    void command_setsprucebasecurrency( QStringList &args );
    void command_setspruceweight( QStringList &args );
    void command_setsprucestartnode( QStringList &args );
    void command_setspruceshortlongtotal( QStringList &args );
    void command_setsprucebetamarket( QStringList &args );
    void command_setspruceamplification( QStringList &args );
    void command_setspruceprofile( QStringList &args );
    void command_setsprucereserve( QStringList &args );
    void command_setspruceordergreed( QStringList &args );
    void command_setspruceordersize( QStringList &args );
    void command_setspruceordernice( QStringList &args );
    void command_setspruceordernicecustom( QStringList &args );
    void command_setspruceordernicemarketoffset( QStringList &args );
    void command_setspruceallocation( QStringList &args );
    void command_setspruceagitator( QStringList &args );
    void command_spruceup( QStringList &args );

    void command_getstatus( QStringList &args );
    void command_getconfig( QStringList &args );
    void command_getinternal( QStringList &args );
    void command_setmaintenancetime( QStringList &args );
    void command_clearallstats( QStringList &args );
    void command_savemarket( QStringList &args );
    void command_savesettings( QStringList &args );
    void command_savestats( QStringList &args );
    void command_sendcommand( QStringList &args );
    void command_setchatty( QStringList &args );
    void command_exit( QStringList &args );

    QMap<QString, std::function<void(QStringList&)>> command_map;

    QVector<BaseREST*> rest_arr;
    quint8 engine_type{ 0 };

    Engine *engine{ nullptr };
    SpruceOverseer *spruce_overseer{ nullptr };
};

#endif // COMMANDRUNNER_H

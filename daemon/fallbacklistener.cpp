#include "global.h"

#ifdef FALLBACK_FILE_INPUT
#include "fallbacklistener.h"

#include <QTimer>
#include <QFile>

FallbackListener::FallbackListener(QObject *parent)
    : QObject( parent ),
      input_file( nullptr ),
      input_timer( nullptr )
{
    openInputFile();

    // init input timer to read commands from in.txt
    input_timer = new QTimer( this );
    connect( input_timer, &QTimer::timeout, this, &FallbackListener::parseInputFile );
    input_timer->setTimerType( Qt::VeryCoarseTimer );
    input_timer->start( 1000 );
}

FallbackListener::~FallbackListener()
{
    input_file->close();
    input_timer->stop();

    delete input_file;
    delete input_timer;
}

bool FallbackListener::openInputFile()
{
    // the file is open and ready, exit
    if ( input_file && input_file->isReadable() && input_file->isWritable() )
        return true;

    QString input_file_path = Global::getTraderPath() + QDir::separator() + "in.txt";

    // if the file is open but stale, dispose of it
    if ( input_file && ( !input_file->isReadable() || !input_file->exists() ) )
    {
        input_file->deleteLater();
        input_file = nullptr;
    }

    // initialize input file if needed
    if ( !input_file )
    {
        input_file = new QFile( input_file_path );
        if ( !input_file->open( QIODevice::ReadWrite | QIODevice::Text ) )
        {
            // delete the file handle
            input_file->deleteLater();
            input_file = nullptr;

            kDebug() << "[FallbackListener] local error: failed to open input file for read/write:" << input_file->fileName();
            return false;
        }

        kDebug() << "[FallbackListener] opened fallback input file" << input_file->fileName();
    }

    return true;
}

void FallbackListener::parseInputFile()
{
    // check/try to open file, and check for bytes available
    if ( !openInputFile() || input_file->bytesAvailable() == 0 )
        return;

    // check for new lines
    QString data = input_file->readAll();

    // truncate the file before we parse the commands, incase we run 'exit', otherwise they'll run next time
    // NOTE: this causes problems, as we can't bombard the text file with frequent edits. use IPC sockets instead, except for bulk calls.
    if ( data.size() > 0 )
    {
        input_file->resize( 0 );
    }

    emit gotDataChunk( data );
}

#endif // FALLBACK_FILE_INPUT

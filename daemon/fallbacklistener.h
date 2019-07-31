#ifndef FALLBACKLISTENER_H
#define FALLBACKLISTENER_H

#include "build-config.h"

#ifdef FALLBACK_FILE_INPUT

#include <QObject>

class QFile;
class QTimer;

class FallbackListener : public QObject
{
    Q_OBJECT
public:
    explicit FallbackListener( QObject *parent = nullptr );
    ~FallbackListener();

signals:
    void gotDataChunk( QString &s );

public slots:
    void parseInputFile();

private:
    bool openInputFile();

    QFile *input_file;
    QTimer *input_timer;
};

#endif // FALLBACK_FILE_INPUT
#endif // FALLBACKLISTENER_H

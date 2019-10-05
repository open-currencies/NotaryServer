#ifndef INTERNALTHREAD_H
#define INTERNALTHREAD_H

#include <pthread.h>
#include "Database.h"
#include "OtherServersHandler.h"
#include "MessageBuilder.h"
#include "Type9Entry.h"
#include "Type12Entry.h"
#include "Type13Entry.h"

typedef unsigned char byte;

class InternalThread
{
public:
    InternalThread(Database *d, OtherServersHandler *s, MessageBuilder *m);
    ~InternalThread();
    void start();
    void stopSafely();
protected:
private:
    volatile bool running;
    volatile bool stopped;
    Database *db;
    OtherServersHandler *servers;
    MessageBuilder *msgBuilder;

    pthread_t thread;
    static void *routine(void *internalThread);
};

#endif // INTERNALTHREAD_H

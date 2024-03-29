#ifndef OTHERSERVERSHANDLER_H
#define OTHERSERVERSHANDLER_H

#include <string>
#include <map>
#include <list>
#include <set>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <thread>
#include <mutex>
#include "MessageBuilder.h"
#include "Type13Entry.h"
#include "CompleteID.h"

using namespace std;

class RequestBuilder;
class RequestProcessor;

class OtherServersHandler
{
public:
    OtherServersHandler(MessageBuilder *msgbuilder);
    ~OtherServersHandler();
    void addContact(unsigned long notary, string ip, int port, unsigned long long validSince, unsigned long long activeUntil);
    void startConnector(RequestProcessor* r);
    void stopSafely();
    void checkNewerEntry(unsigned char listType, CompleteID lastEntryID, unsigned short maxNotaries, bool changePos);
    void checkNewerEntry(unsigned char listType, unsigned long notaryNr, CompleteID lastEntryID);
    unsigned long long getWellConnectedSince();
    bool wellConnected();
    bool wellConnected(size_t notariesListLength);
    unsigned long getSomeReachableNotary();
    void requestEntry(CompleteID entryID, unsigned long notaryNr);
    void requestNotarization(Type12Entry* t12e, unsigned long notaryNr);
    set<unsigned long>* genNotariesList(set<unsigned long> &actingNotaries);
    void sendContactsList(list<string> &contacts, set<unsigned long> &notaries);
    void sendNewSignature(Type13Entry *entry, set<unsigned long> &notaries); // used by the moderator only
    void sendSignatureToAll(string *t13eStr); // inform everyone
    void sendNotarizationEntryToAll(list<Type13Entry*> &t13eList);
    void sendConsiderNotarizationEntryToAll(CompleteID &firstID);
    void askForInitialType13Entry(CompleteID &id, unsigned long notaryNr);
    void loadContactsReachable(list<unsigned long> &notariesList);
    void sendContactsRqst();
    void contactsReport();
protected:
private:
    volatile unsigned long long wellConnectedSince;
    volatile bool allowNewContacts;
    RequestProcessor* answers;
    MessageBuilder* msgBuilder;

    bool sendMessage(unsigned long notaryNr, string &msg);
    void trashAllContacts();
    bool removeTrash();

    struct ContactHandler
    {
        ContactHandler(unsigned long n, string i, int p, unsigned long long v, unsigned long long au, OtherServersHandler* sh);
        ~ContactHandler();
        const unsigned long notaryNr;
        const string ip;
        const int port;
        volatile unsigned long long validSince;
        volatile unsigned long long actingUntil;
        volatile int failedAttempts;
        // message buffer data
        mutex message_buffer_mutex;
        const string messagesStr;
        void addMessage(string &msg);
        size_t msgStrLength();
        // relevant if connection established:
        volatile unsigned long long lastConnectionTime;
        volatile unsigned long long lastListeningTime;
        volatile int socket;
        volatile RequestBuilder* answerBuilder;
        volatile thread* listenerThread;
        volatile bool listenerThreadStopped;
        // for time out thread
        volatile thread* attemptInterruptionThread;
        volatile bool attemptInterrupterStopped;
        volatile int socketNrInAttempt;
        // for connector thread
        volatile thread* connectToThread;
        volatile bool connectToThreadStopped;
        const OtherServersHandler* serversHandler;
    };

    static void connectToNotaryRoutine(ContactHandler *contactHandler);

    mutex contacts_mutex;
    map<unsigned long, ContactHandler*> contactsReachable;
    map<unsigned long, ContactHandler*> contactsToReach;
    map<unsigned long, ContactHandler*> contactsUnreachable;
    list<ContactHandler*> trashedContacts;
    void trashContact(ContactHandler* ch);

    pthread_t connectorThread;
    static void *reachOutRoutine(void *servers);
    volatile bool reachOutRunning;
    volatile bool reachOutStopped;

    static void attemptInterrupter(ContactHandler* contactHandler);
    static void socketReader(ContactHandler* contactHandler);
    static void closeconnection(int sock);

    static unsigned long long systemTimeInMs();

    void sendNewSignature(string *t13eStr, unsigned long notaryNr, unsigned short participationRank);

    vector<unsigned long> reachableNotariesVector;
    volatile bool reachableNotariesVectorCorrect;
    volatile unsigned short selectedNotaryPos;
};

#endif // OTHERSERVERSHANDLER_H

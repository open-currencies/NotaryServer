#ifndef REQUESTPROCESSOR_H
#define REQUESTPROCESSOR_H

#include <stdint.h>
#include <algorithm>
#include "Database.h"
#include "CompleteID.h"
#include "MessageBuilder.h"
#include "Type5Entry.h"
#include "Type15Entry.h"
#include "Type5Or15Entry.h"
#include "Type12Entry.h"
#include "Type13Entry.h"

typedef unsigned char byte;

class OtherServersHandler;

class RequestProcessor
{
public:
    RequestProcessor(Database *d, OtherServersHandler *s, MessageBuilder *msgbuilder);
    ~RequestProcessor();
    void process(const size_t n, byte *request, const int socket);
protected:
private:
    Database *db;
    OtherServersHandler *sh;
    MessageBuilder* msgBuilder;

    void notarizationRequest(const size_t n, byte *request, const int socket);
    void pblcKeyInfoRequest(const size_t n, byte *request, const int socket);
    void currOrOblInfoRequest(const size_t n, byte *request, const int socket);
    void idInfoRequest(const size_t n, byte *request, const int socket);
    void claimsInfoRequest(const size_t n, byte *request, const int socket);
    void nextClaimInfoRequest(const size_t n, byte *request, const int socket);
    void transferRqstsInfoRequest(const size_t n, byte *request, const int socket);
    void decThrInfoRequest(const size_t n, byte *request, const int socket);
    void refInfoRequest(const size_t n, byte *request, const int socket);
    void notaryInfoRequest(const size_t n, byte *request, const int socket);
    void exchangeOffersInfoRequest(const size_t n, byte *request, const int socket);
    void essentialsRequest(const size_t n, byte *request, const int socket);
    void newSignatureRequest(const size_t n, byte *request, const int socket);
    void initialType13EntryRequest(const size_t n, byte *request, const int socket);
    void notarizationEntryRequest(const size_t n, byte *request, const int socket);
    void checkNewerEntryRequest(const size_t n, byte *request, const int socket);
    void considerNewerEntriesRequest(const size_t n, byte *request);
    void considerNotarizationEntryRequest(const size_t n, byte *request);
    void considerContactInfoRequest(const size_t n, byte *request);
    void contactsRequest(const size_t n, byte *request, const int socket);
    void closeConnectionRequest(const int socket);
    void heartBeatRequest(const int socket);

    bool buildType13Entries(CompleteID &id, list<Type13Entry*> &signaturesList);
    bool loadSupportingType13Entries(CompleteID &id, list<Type13Entry*> &target);
    static void deleteContent(list<Type13Entry*> &entries);
    static void deleteContent(list<list<Type13Entry*>*> &listOfLists);
};

#endif // REQUESTPROCESSOR_H

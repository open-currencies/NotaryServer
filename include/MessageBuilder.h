#ifndef MESSAGEBUILDER_H
#define MESSAGEBUILDER_H

#include <string>
#include "Entry.h"
#include "Type11Entry.h"
#include "Type12Entry.h"
#include "Type13Entry.h"
#include "Type14Entry.h"
#include "CompleteID.h"
#include "TNtrNr.h"
#include "Util.h"
#include "rsa.h"
#include "secblock.h"
#include "pssr.h"
#include "sha3.h"
#include "osrng.h"
#include "RefereeInfo.h"
#include "NotaryInfo.h"
#include <sys/socket.h>

#define maxLong 4294967295

using namespace std;
class Database;
class OtherServersHandler;
typedef unsigned char byte;

class MessageBuilder
{
public:
    MessageBuilder(TNtrNr notary, CryptoPP::RSA::PrivateKey *key);
    ~MessageBuilder();
    Type13Entry* signEntry(Type13Entry* entry, Type12Entry* uEntry, CompleteID &notPredecessorID);
    Type13Entry* terminateAndSign(CompleteID &threadId);
    Type13Entry* packKeyAndSign(string &pubKeyStr);
    TNtrNr getTNotaryNr();
    void setPublicKeyId(CompleteID &id);
    void sendHeartBeat(int sock);
    void sendAmBanned(int sock);
    void sendPblcKeyInfo(list<Type13Entry*> &t13eList, int sock);
    void sendCurrOrOblInfo(list<Type13Entry*> &t13eList, int sock);
    void sendIdInfo(CompleteID &id, list<list<Type13Entry*>*> &listOfT13eLists, int sock);
    void sendNextClaim(string paramstr, Type14Entry *t14e, int sock);
    void sendClaims(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock);
    void sendDecThreads(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock);
    void sendTransactions(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock);
    void sendEssentials(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock);
    void sendRefInfo(string &paramstr, RefereeInfo &refInfo, int sock);
    void sendNotaryInfo(string &paramstr, NotaryInfo &notaryInfo, int sock);
    void sendNewerIds(unsigned char listType, CompleteID id1, CompleteID id2, int sock);
    void sendNotarizationEntry(list<Type13Entry*> &t13eList, int sock);
    void sendNotarizationEntry(list<string> &entriesStr, int sock);
    void sendSignature(string *t13eStr, int sock); // used by non-moderating participants and for clarifications
    static void packMessage(string *message);
    string* signString(string &strToSign);
    static bool addToString(list<Type13Entry*> &source, string &target);
    static unsigned long long systemTimeInMs();
    void setDB(Database *d);
    void setServersHandler(OtherServersHandler *s);
    void sendContactInfo(string &contactInfo, int sock);
    void addOwnContactInfoToDB(string &ip, int port, string *ciStr);
    static bool addToString(list<string> &source, string &target);
    bool appendConfirmation(list<Type13Entry*> &signaturesList, Type12Entry* uEntry, CompleteID &confirmationID);
protected:
private:
    TNtrNr notaryNr;
    CryptoPP::RSA::PrivateKey *privateKey;
    CryptoPP::AutoSeededRandomPool *rng;
    CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA3_384>::Signer *signer;
    CompleteID publicKeyID;
    Database *db;
    OtherServersHandler *servers;

    unsigned long long runningID;
    string newCompleteIDStr();

    Type13Entry* signEntry(Type13Entry* entry, Type12Entry* uEntry, CompleteID &notPredecessorID, string &newCIDStr);
    Type13Entry* signEntry(Entry* entry);
    static unsigned long addModulo(unsigned long a, unsigned long b);
    static bool addToString(list<list<Type13Entry*>*> &source, string &target);
};

#endif // MESSAGEBUILDER_H

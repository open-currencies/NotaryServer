#include "MessageBuilder.h"
#include "Database.h"
#include "OtherServersHandler.h"

MessageBuilder::MessageBuilder(TNtrNr notary, CryptoPP::RSA::PrivateKey *key)
    : notaryNr(notary), privateKey(key), publicKeyID(CompleteID()), db(nullptr), servers(nullptr), runningID(1)
{
    if (privateKey==nullptr)
    {
        rng = nullptr;
        signer = nullptr;
    }
    else
    {
        rng = new CryptoPP::AutoSeededRandomPool();
        signer = new CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA3_384>::Signer(*privateKey);
    }
}

MessageBuilder::~MessageBuilder()
{
    if (privateKey!=nullptr)
    {
        delete rng;
        delete signer;
    }
}

void MessageBuilder::setDB(Database *d)
{
    db = d;
}

void MessageBuilder::setServersHandler(OtherServersHandler *s)
{
    servers = s;
}

void MessageBuilder::addOwnContactInfoToDB(string &ip, int port, string *ciStr)
{
    if (!notaryNr.isGood() || privateKey==nullptr) return;
    unsigned long portL = (unsigned long) ((long) port);
    Util u;
    string str;
    str.append(ip);
    str.append(u.UlAsByteSeq(portL));
    unsigned long long validSince = systemTimeInMs();
    str.append(u.UllAsByteSeq(validSince));
    string *signature = signString(str);
    ContactInfo contactInfo(notaryNr, ip, portL, validSince, *signature);
    delete signature;
    db->lock();
    bool result = db->tryToStoreContactInfo(contactInfo);
    db->unlock();
    if (result && ciStr!=nullptr && ciStr->length()==0) ciStr->append(*contactInfo.getByteSeq());
}

string* MessageBuilder::signString(string &strToSign)
{
    size_t length = signer->MaxSignatureLength();
    CryptoPP::SecByteBlock signature(length);
    length = signer->SignMessage(*rng, (const byte*) strToSign.c_str(), strToSign.length(), signature);
    signature.resize(length);
    string* out = new string();
    out->append((const char*) signature.BytePtr(), signature.size());
    return out;
}

bool MessageBuilder::appendConfirmation(list<Type13Entry*> &signaturesList, Type12Entry* uEntry, CompleteID &confirmationID)
{
    Type13Entry* entry = *signaturesList.rbegin();
    if (entry->getCompleteID() == confirmationID) return false;
    CompleteID zeroID;
    string newCIDStr = confirmationID.to20Char();
    Type13Entry *confirmationEntry = signEntry(entry, uEntry, zeroID, newCIDStr);
    if (confirmationEntry == nullptr || !confirmationEntry->isGood())
    {
        puts("MessageBuilder::appendConfirmation: unable to build good confirmationEntry");
        if (confirmationEntry != nullptr) delete confirmationEntry;
        return false;
    }
    signaturesList.push_back(confirmationEntry);
    return true;
}

Type13Entry* MessageBuilder::signEntry(Type13Entry* entry, Type12Entry* uEntry, CompleteID &notPredecessorID)
{
    if (db==nullptr || servers==nullptr || !notaryNr.isGood()) return nullptr;
    unsigned long long wellConnectedSince = servers->getWellConnectedSince();
    db->lock();
    bool dbUpToDate = db->dbUpToDate(wellConnectedSince) && db->amCurrentlyActingWithBuffer();
    bool isConflicting = false;
    if (entry!=nullptr)
    {
        CompleteID firstID = entry->getFirstID();
        isConflicting = db->isConflicting(firstID);
    }
    db->unlock();
    if (!dbUpToDate || isConflicting) return nullptr;

    string cIDStr = newCompleteIDStr();
    return signEntry(entry, uEntry, notPredecessorID, cIDStr);
}

Type13Entry* MessageBuilder::signEntry(Type13Entry* entry, Type12Entry* uEntry, CompleteID &notPredecessorID, string &newCIDStr)
{
    if (!notaryNr.isGood() || privateKey==nullptr)
    {
        return nullptr;
    }
    if (uEntry==nullptr || !uEntry->isGood())
    {
        puts("bad uEntry supplied in MessageBuilder::signEntry");
        return nullptr;
    }

    Util u;
    string type13entryStr;
    type13entryStr.push_back(0x2C);
    if (entry == nullptr)
    {
        // create string to sign
        string strToSign(newCIDStr);
        strToSign.append(newCIDStr);
        strToSign.append(newCIDStr);
        type13entryStr.append(strToSign);
        strToSign.append(*uEntry->getByteSeq());
        // sign string
        size_t length = signer->MaxSignatureLength();
        CryptoPP::SecByteBlock signature(length);
        length = signer->SignMessage(*rng, (const byte*) strToSign.c_str(), strToSign.length(), signature);
        signature.resize(length);
        // finish type13entryStr
        unsigned long long uLength = uEntry->getByteSeq()->length();
        type13entryStr.append(u.UllAsByteSeq(uLength+8+signature.size()));
        type13entryStr.append(u.UllAsByteSeq(uLength));
        type13entryStr.append(*uEntry->getByteSeq());
        type13entryStr.append((const char*) signature.BytePtr(), signature.size());
    }
    else if (!entry->isGood())
    {
        return nullptr;
    }
    else
    {
        // create string to sign
        string strToSign(newCIDStr);
        if (notPredecessorID.isZero()) // this signature not leading
        {
            strToSign.append(entry->getCompleteID().to20Char());
            strToSign.append(entry->getFirstID().to20Char());
        }
        else // creating leading renotarization signature
        {
            strToSign.append(notPredecessorID.to20Char());
            strToSign.append(newCIDStr);
        }
        type13entryStr.append(strToSign); // add three ids to type13entry
        strToSign.append(*uEntry->getByteSeq());
        strToSign.append(*entry->getByteSeq());
        // sign string
        size_t length = signer->MaxSignatureLength();
        CryptoPP::SecByteBlock signature(length);
        length = signer->SignMessage(*rng, (const byte*) strToSign.c_str(), strToSign.length(), signature);
        signature.resize(length);
        // finish type13entryStr
        type13entryStr.append(u.UllAsByteSeq(signature.size()));
        type13entryStr.append((const char*) signature.BytePtr(), signature.size());
    }

    Type13Entry *out = new Type13Entry(type13entryStr);
    if (!out->isGood())
    {
        delete out;
        return nullptr;
    }
    return out;
}

string MessageBuilder::newCompleteIDStr()
{
    Util u;
    string out;
    out.append(u.flip(u.UllAsByteSeq(systemTimeInMs())));
    out.append(u.flip(u.UlAsByteSeq(notaryNr.getNotaryNr())));
    out.append(u.flip(u.UllAsByteSeq(runningID)));
    runningID++;
    return out;
}

unsigned long long MessageBuilder::systemTimeInMs()
{
    using namespace std::chrono;
    milliseconds ms = duration_cast< milliseconds >(
                          system_clock::now().time_since_epoch()
                      );
    return ms.count();
}

Type13Entry* MessageBuilder::signEntry(Entry* entry)
{
    if (!notaryNr.isGood() || privateKey==nullptr)
    {
        return nullptr;
    }
    if (entry==nullptr || !entry->isGood() || entry->getByteSeq()==nullptr)
    {
        puts("bad entry supplied in MessageBuilder::signEntry(Entry* entry)");
        return nullptr;
    }
    Util u;
    string type12entryStr;
    type12entryStr.push_back(0x2B);
    // create string to sign
    string strToSign(*entry->getByteSeq());
    // add length to type12entryStr
    type12entryStr.append(u.UllAsByteSeq(strToSign.length()));
    // continue
    strToSign.append(u.UlAsByteSeq(notaryNr.getNotaryNr()));
    strToSign.append(u.UllAsByteSeq(systemTimeInMs() + 1000 * 30));
    // sign string
    size_t length = signer->MaxSignatureLength();
    CryptoPP::SecByteBlock signature(length);
    length = signer->SignMessage(*rng, (const byte*) strToSign.c_str(), strToSign.length(), signature);
    signature.resize(length);
    // finish type12entryStr
    type12entryStr.append(strToSign);
    type12entryStr.append(u.UllAsByteSeq(signature.size()));
    type12entryStr.append((const char*) signature.BytePtr(), signature.size());
    // sign type 12 entry and return
    Type12Entry *type12entry = new Type12Entry(type12entryStr);
    if (!type12entry->isGood())
    {
        puts("MessageBuilder::signEntry(Entry* entry): bad type12entry");
        delete type12entry;
        return nullptr;
    }
    //return type12entry;
    CompleteID zeroId;
    Type13Entry *type13entry = signEntry(nullptr, type12entry, zeroId);
    delete type12entry;
    return type13entry;
}

Type13Entry* MessageBuilder::terminateAndSign(CompleteID &threadId)
{
    if (publicKeyID.getNotary()<=0 || threadId.getNotary()<=0 || !notaryNr.isGood() || privateKey==nullptr) return nullptr;
    // build type9entry
    Type9Entry type9entry(publicKeyID, threadId);
    // build type12entry
    string type12entryStr;
    type12entryStr.push_back(0x2B);
    // create string to sign
    string strToSign(*type9entry.getByteSeq());
    // add length to type12entryStr
    Util u;
    type12entryStr.append(u.UllAsByteSeq(strToSign.length()));
    // continue
    strToSign.append(publicKeyID.to20Char());
    strToSign.append(u.UlAsByteSeq(notaryNr.getNotaryNr()));
    strToSign.append(u.UllAsByteSeq(systemTimeInMs() + 1000 * 30));
    // sign string
    size_t length = signer->MaxSignatureLength();
    CryptoPP::SecByteBlock signature(length);
    length = signer->SignMessage(*rng, (const byte*) strToSign.c_str(), strToSign.length(), signature);
    signature.resize(length);
    // finish type12entryStr
    type12entryStr.append(strToSign);
    type12entryStr.append(u.UllAsByteSeq(signature.size()));
    type12entryStr.append((const char*) signature.BytePtr(), signature.size());
    // sign type 12 entry and return
    Type12Entry *type12entry = new Type12Entry(type12entryStr);
    if (!type12entry->isGood())
    {
        delete type12entry;
        return nullptr;
    }
    CompleteID zeroId;
    Type13Entry *type13entry = signEntry(nullptr, type12entry, zeroId);
    delete type12entry;
    return type13entry;
}

Type13Entry* MessageBuilder::packKeyAndSign(string &pubKeyStr)
{
    unsigned long long validUntil = 1000*60; // minute
    validUntil *= 60; // hour
    validUntil *= 24; // day
    validUntil *= 356; // year
    validUntil *= 60;
    validUntil += systemTimeInMs();
    Type11Entry type11entry(pubKeyStr, validUntil);
    return signEntry(&type11entry);
}

TNtrNr MessageBuilder::getTNotaryNr()
{
    if (privateKey == nullptr) return TNtrNr(0,0);
    else return notaryNr;
}

void MessageBuilder::setPublicKeyId(CompleteID &id)
{
    publicKeyID=id;
}

void MessageBuilder::sendPblcKeyInfo(list<Type13Entry*> &t13eList, int sock)
{
    string msg;
    byte type = 254;
    msg.push_back((char)type);
    if (!addToString(t13eList, msg)) return;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::PblcKeyInfo sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendPblcKeyInfo unsuccessful");
    }
}

bool MessageBuilder::addToString(list<string> &source, string &target)
{
    Util u;
    list<string>::iterator it;
    for (it=source.begin(); it!=source.end(); ++it)
    {
        target.append(u.UllAsByteSeq(it->length()));
        target.append(*it);
    }
    return true;
}

bool MessageBuilder::addToString(list<Type13Entry*> &source, string &target)
{
    Util u;
    list<Type13Entry*>::iterator it;
    for (it=source.begin(); it!=source.end(); ++it)
    {
        if (*it == nullptr) return false;
        string *str = (*it)->getByteSeq();
        if (str == nullptr) return false;
        target.append(u.UllAsByteSeq(str->length()));
        target.append(*str);
    }
    return true;
}

bool MessageBuilder::addToString(list<list<Type13Entry*>*> &source, string &target)
{
    Util u;
    list<list<Type13Entry*>*>::iterator it;
    for (it=source.begin(); it!=source.end(); ++it)
    {
        list<Type13Entry*> *nextList = *it;
        if (nextList == nullptr || nextList->size()<=0) return false;
        string str;
        addToString(*nextList, str);
        target.append(u.UllAsByteSeq(str.length()));
        target.append(str);
    }
    return true;
}

void MessageBuilder::sendCurrOrOblInfo(list<Type13Entry*> &t13eList, int sock)
{
    string msg;
    byte type = 253;
    msg.push_back((char)type);
    if (!addToString(t13eList, msg)) return;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::CurrOrOblInfo sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendCurrOrOblInfo unsuccessful");
    }
}

void MessageBuilder::sendIdInfo(CompleteID &id, list<list<Type13Entry*>*> &listOfT13eLists, int sock)
{
    string msg;
    byte type = 252;
    msg.push_back((char)type);
    // add initial request string
    type = 3;
    string paramstr(id.to20Char());
    paramstr.insert(0, 1, (char)type);
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add claims
    if (!addToString(listOfT13eLists, msg)) return;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::IdInfo sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendIdInfo unsuccessful");
    }
}

void MessageBuilder::sendContactInfo(string &contactInfo, int sock)
{
    string msg;
    byte type = 18;
    msg.push_back((char)type);
    // add contact info string
    msg.append(contactInfo);
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::contactInfo sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendContactInfo unsuccessful");
    }
}

void MessageBuilder::sendRefInfo(string &paramstr, RefereeInfo &refInfo, int sock)
{
    string msg;
    byte type = 247;
    msg.push_back((char)type);
    // add initial request string
    type = 8;
    paramstr.insert(0, 1, (char)type);
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add ref info as string
    msg.append(*(refInfo.getByteSeq()));
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::RefInfo sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendRefInfo unsuccessful");
    }
}

void MessageBuilder::sendNotaryInfo(string &paramstr, NotaryInfo &notaryInfo, int sock)
{
    string msg;
    byte type = 245;
    msg.push_back((char)type);
    // add initial request string
    type = 11;
    paramstr.insert(0, 1, (char)type);
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add notary info as string
    msg.append(*(notaryInfo.getByteSeq()));
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::NotaryInfo sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendNotaryInfo unsuccessful");
    }
}

void MessageBuilder::sendClaims(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock)
{
    string msg;
    byte type = 251;
    msg.push_back((char)type);
    // add initial request string
    type = 4;
    paramstr.insert(0, 1, (char)type);
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add claims
    if (!addToString(listOfT13eLists, msg)) return;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::Claims sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendClaims unsuccessful");
    }
}

void MessageBuilder::sendDecThreads(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock)
{
    string msg;
    byte type = 248;
    msg.push_back((char)type);
    // add initial request string
    type = 7;
    paramstr.insert(0, 1, (char)type);
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add entries
    if (!addToString(listOfT13eLists, msg)) return;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::DecThreads sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendDecThreads unsuccessful");
    }
}

void MessageBuilder::sendEssentials(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock)
{
    string msg;
    byte type = 246;
    msg.push_back((char)type);
    // add initial request string
    type = 10;
    paramstr.insert(0, 1, (char)type);
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add claims
    if (!addToString(listOfT13eLists, msg)) return;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::Essentials sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendEssentials unsuccessful");
    }
}

void MessageBuilder::sendTransactions(string paramstr, list<list<Type13Entry*>*> &listOfT13eLists, int sock)
{
    string msg;
    byte type = 249;
    msg.push_back((char)type);
    // add initial request string
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add claims
    if (!addToString(listOfT13eLists, msg)) return;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::Transactions sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendTransactions unsuccessful");
    }
}

void MessageBuilder::sendNextClaim(string paramstr, Type14Entry *t14e, int sock)
{
    string msg;
    byte type = 250;
    msg.push_back((char)type);
    // add initial request string
    type = 5;
    paramstr.insert(0, 1, (char)type);
    packMessage(&paramstr);
    Util u;
    msg.append(u.UllAsByteSeq(paramstr.length()));
    msg.append(paramstr);
    // add claims
    msg.append(*(t14e->getByteSeq()));
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::NextClaim sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendNextClaim unsuccessful");
    }
}

void MessageBuilder::sendAmBanned(int sock)
{
    string message;
    byte type = 244;
    message.push_back(type);
    packMessage(&message);
    unsigned long result = send(sock, message.c_str(), message.length(), MSG_NOSIGNAL);
    if (result == message.length())
    {
        //puts("MessageBuilder::AmBanned sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendAmBanned unsuccessful");
    }
}

void MessageBuilder::sendHeartBeat(int sock)
{
    string message;
    byte type = 255;
    message.push_back(type);
    Util u;
    message.append(u.UllAsByteSeq(systemTimeInMs()));
    packMessage(&message);
    unsigned long result = send(sock, message.c_str(), message.length(), MSG_NOSIGNAL);
    if (result == message.length())
    {
        //puts("MessageBuilder::HeartBeat sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendHeartBeat unsuccessful");
    }
}

void MessageBuilder::packMessage(string *message)
{
    unsigned long checkSum = 0;
    for (unsigned long i=0; i<message->length(); i++)
    {
        checkSum = addModulo(checkSum, (byte) message->at(i));
    }
    Util u;
    string lenAsSeq = u.UlAsByteSeq(message->length());
    string lenAsSeqReverse;
    lenAsSeqReverse.push_back(lenAsSeq.at(3));
    lenAsSeqReverse.push_back(lenAsSeq.at(2));
    lenAsSeqReverse.push_back(lenAsSeq.at(1));
    lenAsSeqReverse.push_back(lenAsSeq.at(0));
    message->insert(0, lenAsSeqReverse);
    string checkSumAsSeq = u.UlAsByteSeq(checkSum);
    message->push_back(checkSumAsSeq.at(3));
    message->push_back(checkSumAsSeq.at(2));
    message->push_back(checkSumAsSeq.at(1));
    message->push_back(checkSumAsSeq.at(0));
}

void MessageBuilder::sendSignature(string *t13eStr, int sock)
{
    string msg;
    byte type = 12;
    Util u;
    msg.push_back((char)type);
    msg.append(u.UllAsByteSeq(t13eStr->length()));
    msg.append(*t13eStr);
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::Signature sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendSignature unsuccessful");
    }
}

void MessageBuilder::sendNewerIds(unsigned char listType, CompleteID id1, CompleteID id2, int sock)
{
    if (!getTNotaryNr().isGood() || privateKey==nullptr) return;

    string msg;
    byte type = 16;
    msg.push_back((char)type);
    Util u;
    string sequenceToSign;
    sequenceToSign.append(u.UcAsByteSeq(listType));
    sequenceToSign.append(u.UlAsByteSeq(getTNotaryNr().getNotaryNr()));
    sequenceToSign.append(id1.to20Char());
    sequenceToSign.append(id2.to20Char());
    string *signature = signString(sequenceToSign);
    msg.append(sequenceToSign);
    msg.append(*signature);
    delete signature;
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::NewerIds sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendNewerIds unsuccessful");
    }
}

void MessageBuilder::sendNotarizationEntry(list<string> &entriesStr, int sock)
{
    if (!getTNotaryNr().isGood() || privateKey==nullptr) return;

    string msg;
    byte type = 17;
    msg.push_back((char)type);
    string sequenceToSign;
    if (!addToString(entriesStr, sequenceToSign)) return;
    string *signature = signString(sequenceToSign);
    msg.append(sequenceToSign);
    // add signature and notaryNr and time stamp
    Util u;
    msg.append(u.UllAsByteSeq(signature->length()+12));
    msg.append(*signature);
    delete signature;
    msg.append(u.UlAsByteSeq(getTNotaryNr().getNotaryNr()));
    msg.append(u.UllAsByteSeq(systemTimeInMs()));
    // pack and send
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::NotarizationEntry sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendNotarizationEntry unsuccessful");
    }
}

void MessageBuilder::sendNotarizationEntry(list<Type13Entry*> &t13eList, int sock)
{
    if (!getTNotaryNr().isGood() || privateKey==nullptr) return;

    string msg;
    byte type = 17;
    msg.push_back((char)type);
    string sequenceToSign;
    if (!addToString(t13eList, sequenceToSign)) return;
    string *signature = signString(sequenceToSign);
    msg.append(sequenceToSign);
    // add signature and notaryNr and time stamp
    Util u;
    msg.append(u.UllAsByteSeq(signature->length()+12));
    msg.append(*signature);
    delete signature;
    msg.append(u.UlAsByteSeq(getTNotaryNr().getNotaryNr()));
    msg.append(u.UllAsByteSeq(systemTimeInMs()));
    // pack and send
    packMessage(&msg);
    unsigned long long result = send(sock, msg.c_str(), msg.length(), MSG_NOSIGNAL);
    if (result == msg.length())
    {
        //puts("MessageBuilder::NotarizationEntry sent successfully");
    }
    else
    {
        //puts("MessageBuilder::sendNotarizationEntry unsuccessful");
    }
}

unsigned long MessageBuilder::addModulo(unsigned long a, unsigned long b)
{
    const unsigned long diff = maxLong - a;
    if (b>diff) return b-diff-1;
    else return a+b;
}

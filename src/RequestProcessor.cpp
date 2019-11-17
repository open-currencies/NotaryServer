#include "RequestProcessor.h"
#include "OtherServersHandler.h"

RequestProcessor::RequestProcessor(Database *d, OtherServersHandler *s, MessageBuilder *msgbuilder)
    : db(d), sh(s), msgBuilder(msgbuilder)
{
    //ctor
}

RequestProcessor::~RequestProcessor()
{
    //dtor
}

void RequestProcessor::process(const size_t n, byte *request, const int socket)
{
    if (n<1) return;
    byte type=request[0];
    switch (type)
    {
    case 0:
        notarizationRequest(n, request, socket);
        break;
    case 1:
        pblcKeyInfoRequest(n, request, socket);
        break;
    case 2:
        currOrOblInfoRequest(n, request, socket);
        break;
    case 3:
        idInfoRequest(n, request, socket);
        break;
    case 4:
        claimsInfoRequest(n, request, socket);
        break;
    case 5:
        nextClaimInfoRequest(n, request, socket);
        break;
    case 6:
        transferRqstsInfoRequest(n, request, socket);
        break;
    case 7:
        decThrInfoRequest(n, request, socket);
        break;
    case 8:
        refInfoRequest(n, request, socket);
        break;
    case 9:
        exchangeOffersInfoRequest(n, request, socket);
        break;
    case 10:
        essentialsRequest(n, request, socket);
        break;
    case 11:
        notaryInfoRequest(n, request, socket);
        break;
    case 12:
        newSignatureRequest(n, request, socket); // might be an answer to own request
        break;
    case 13:
        initialType13EntryRequest(n, request, socket);
        break;
    case 14:
        notarizationEntryRequest(n, request, socket);
        break;
    case 15:
        checkNewerEntryRequest(n, request, socket);
        break;
    case 16:
        considerNewerEntriesRequest(n, request); // might be an answer to own request
        break;
    case 17:
        considerNotarizationEntryRequest(n, request); // might be an answer to own request
        break;
    case 18:
        considerContactInfoRequest(n, request); // might be an answer to own request
        break;
    case 19:
        contactsRequest(n, request, socket);
        break;
    case 20:
        closeConnectionRequest(socket);
        break;
    case 21:
        heartBeatRequest(socket);
        break;
    default:
        return;
    }
}

void RequestProcessor::closeConnectionRequest(const int socket)
{
    if (socket==-1) return;
    shutdown(socket,2);
    close(socket);
}

void RequestProcessor::heartBeatRequest(const int socket)
{
    msgBuilder->sendHeartBeat(socket);
}

void RequestProcessor::pblcKeyInfoRequest(const size_t n, byte *request, const int socket)
{
    string pblcKeyStr;
    for (size_t i=1; i<n; i++) pblcKeyStr.push_back((char)request[i]);
    db->lock();
    CompleteID firstId = db->getFirstID(&pblcKeyStr);
    db->unlock();
    if (firstId.getNotary() <= 0) return;
    list<Type13Entry*> t13eList;
    if (loadSupportingType13Entries(firstId, t13eList))
    {
        // send t13eList
        msgBuilder->sendPblcKeyInfo(t13eList, socket);
    }
    else
    {
        puts("pblcKeyInfoRequest: t13eList could not be loaded");
    }
    deleteContent(t13eList);
}

void RequestProcessor::currOrOblInfoRequest(const size_t n, byte *request, const int socket)
{
    string currOrOblStr;
    for (size_t i=1; i<n; i++) currOrOblStr.push_back((char)request[i]);
    // create Type5Or15Entry entry
    if (currOrOblStr.length()<20)
    {
        return;
    }
    Util u;
    const int type = u.getType(currOrOblStr);
    Type5Or15Entry *entry = nullptr;
    if (type == 5)
    {
        Type5Entry *t5e = new Type5Entry(currOrOblStr);
        entry = (Type5Or15Entry*) t5e;
    }
    else if (type == 15)
    {
        Type15Entry *t15e = new Type15Entry(currOrOblStr);
        entry = (Type5Or15Entry*) t15e;
    }
    else
    {
        return;
    }
    if (!entry->isGood())
    {
        puts("currOrOblInfoRequest: good entry could not be loaded");
        delete entry;
        return;
    }
    // get the id
    db->lock();
    CompleteID firstId = db->getCurrencyOrOblId(entry);
    db->unlock();
    delete entry;
    if (firstId.getNotary() <= 0)
    {
        return;
    }
    // get signature entries and send
    list<Type13Entry*> t13eList;
    if (loadSupportingType13Entries(firstId, t13eList))
    {
        msgBuilder->sendCurrOrOblInfo(t13eList, socket);
    }
    else
    {
        puts("currOrOblInfoRequest: t13eList could not be loaded");
    }
    deleteContent(t13eList);
}

void RequestProcessor::refInfoRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    if (str.length()!=40) return;
    string dum = str.substr(0,20);
    CompleteID keyId(dum);
    dum = str.substr(20,20);
    CompleteID liquiId(dum);
    // load info from db
    db->lock();
    RefereeInfo *refInfo = db->getRefereeInfo(keyId, liquiId);
    db->unlock();
    if (refInfo == nullptr) return;
    // send
    msgBuilder->sendRefInfo(str, *refInfo, socket);
    delete refInfo;
}

void RequestProcessor::notaryInfoRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    if (str.length()<24) return;
    string dum = str.substr(0,4);
    Util u;
    unsigned long lenOfKey = u.byteSeqAsUl(dum);
    string publicKey = str.substr(4, lenOfKey);
    dum = str.substr(4+lenOfKey, 20);
    CompleteID liquiId(dum);
    // load info from db
    db->lock();
    NotaryInfo *notaryInfo = db->getNotaryInfo(&publicKey, liquiId);
    db->unlock();
    if (notaryInfo == nullptr) return;
    // send
    msgBuilder->sendNotaryInfo(str, *notaryInfo, socket);
    delete notaryInfo;
}

void RequestProcessor::idInfoRequest(const size_t n, byte *request, const int socket)
{
    string idStr;
    for (size_t i=1; i<n; i++) idStr.push_back((char)request[i]);
    if (idStr.length()!=20) return;
    CompleteID id(idStr);
    // get the ids
    list<CompleteID> idsList;
    db->lock();
    db->getRelatedEntries(id, idsList);
    db->unlock();
    // load signatures
    list<list<Type13Entry*>*> listOfT13eLists;
    list<CompleteID>::iterator it;
    for (it=idsList.begin(); it!=idsList.end(); ++it)
    {
        list<Type13Entry*> *nextList = new list<Type13Entry*>();
        if (!loadSupportingType13Entries(*it, *nextList))
        {
            puts("idInfoRequest: nextList could not be loaded");
            delete nextList;
            deleteContent(listOfT13eLists);
            return;
        }
        listOfT13eLists.push_back(nextList);
    }
    // send
    msgBuilder->sendIdInfo(id, listOfT13eLists, socket);
    deleteContent(listOfT13eLists);
}

void RequestProcessor::claimsInfoRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // extract owner id
    Util u;
    size_t pos = 0;
    string dum;
    if (str.length()<20) return;
    dum = str.substr(pos,20);
    CompleteID id(dum);
    pos+=20;
    // get currency/obl id
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID currencyId(dum);
    pos+=20;
    // get maxClaimId
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID maxClaimId(dum);
    pos+=20;
    // get max claims number
    if (str.length()-pos<2) return;
    dum = str.substr(pos,2);
    unsigned short maxClaimsNum = min((unsigned short) 25, u.byteSeqAsUs(dum));
    pos+=2;
    if (pos!=str.length()) return;
    // get the ids
    list<CompleteID> idsList;
    db->lock();
    db->getClaims(id, currencyId, maxClaimId, maxClaimsNum, idsList);
    db->unlock();
    // load signatures
    list<list<Type13Entry*>*> listOfT13eLists;
    list<CompleteID>::iterator it;
    for (it=idsList.begin(); it!=idsList.end(); ++it)
    {
        list<Type13Entry*> *nextList = new list<Type13Entry*>();
        if (!loadSupportingType13Entries(*it, *nextList))
        {
            puts("claimsInfoRequest: nextList could not be loaded");
            delete nextList;
            deleteContent(listOfT13eLists);
            return;
        }
        listOfT13eLists.push_back(nextList);
    }
    // send
    msgBuilder->sendClaims(str, listOfT13eLists, socket);
    deleteContent(listOfT13eLists);
}

void RequestProcessor::decThrInfoRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    Util u;
    size_t pos = 0;
    string dum;
    // extract spec
    if (str.length()-pos<1) return;
    dum = str.substr(pos,1);
    unsigned char spec = u.byteSeqAsUc(dum);
    pos+=1;
    // extract applicant/referee id
    if (str.length()<20) return;
    dum = str.substr(pos,20);
    CompleteID keyId(dum);
    pos+=20;
    // extract currency id
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID currencyId(dum);
    pos+=20;
    // get minApplId
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID minApplId(dum);
    pos+=20;
    // get max threads number
    if (str.length()-pos<2) return;
    dum = str.substr(pos,2);
    unsigned short maxThreadsNum = min((unsigned short) 25, u.byteSeqAsUs(dum));
    pos+=2;
    if (pos!=str.length()) return;
    // dissect spec
    bool applicantSpecified = true;
    if (spec>=10)
    {
        applicantSpecified = false;
        spec-=10;
    }
    bool isOpPr = true;
    if (spec>=5)
    {
        isOpPr = false;
        spec-=5;
    }
    // get the ids
    list<CompleteID> idsList;
    db->lock();
    if (currencyId.isZero())
    {
        db->getNotaryApplications(keyId, applicantSpecified, spec, minApplId, maxThreadsNum, idsList);
    }
    else
    {
        if (isOpPr) db->getApplications("OP", keyId, applicantSpecified, currencyId, spec, minApplId, maxThreadsNum, idsList);
        else db->getApplications("AR", keyId, applicantSpecified, currencyId, spec, minApplId, maxThreadsNum, idsList);
    }
    db->unlock();
    // load signatures
    list<list<Type13Entry*>*> listOfT13eLists;
    list<CompleteID>::iterator it;
    for (it=idsList.begin(); it!=idsList.end(); ++it)
    {
        list<Type13Entry*> *nextList = new list<Type13Entry*>();
        if (!loadSupportingType13Entries(*it, *nextList))
        {
            puts("decThrInfoRequest: nextList could not be loaded");
            delete nextList;
            deleteContent(listOfT13eLists);
            return;
        }
        listOfT13eLists.push_back(nextList);
    }
    // send
    msgBuilder->sendDecThreads(str, listOfT13eLists, socket);
    deleteContent(listOfT13eLists);
}

void RequestProcessor::exchangeOffersInfoRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // extract key id
    Util u;
    size_t pos = 0;
    string dum;
    if (str.length()<20) return;
    dum = str.substr(pos,20);
    CompleteID id(dum);
    pos+=20;
    // get currency/obl id
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID currencyOId(dum);
    pos+=20;
    // get range
    if (str.length()-pos<2) return;
    dum = str.substr(pos,2);
    unsigned short rangeNum = u.byteSeqAsUs(dum);
    pos+=2;
    // get currency/obl id
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID currencyRId(dum);
    pos+=20;
    // get max transRqsts number
    if (str.length()-pos<2) return;
    dum = str.substr(pos,2);
    unsigned short maxTransRqstsNum = min((unsigned short) 25, u.byteSeqAsUs(dum));
    pos+=2;
    if (pos!=str.length()) return;
    // get the ids
    list<CompleteID> idsList;
    db->lock();
    db->getExchangeOffers(id, currencyOId, currencyRId, rangeNum, maxTransRqstsNum, idsList);
    db->unlock();
    // load signatures
    list<list<Type13Entry*>*> listOfT13eLists;
    list<CompleteID>::iterator it;
    for (it=idsList.begin(); it!=idsList.end(); ++it)
    {
        list<Type13Entry*> *nextList = new list<Type13Entry*>();
        if (!loadSupportingType13Entries(*it, *nextList))
        {
            puts("exchangeOffersInfoRequest: nextList could not be loaded");
            delete nextList;
            deleteContent(listOfT13eLists);
            return;
        }
        listOfT13eLists.push_back(nextList);
    }
    // update exchange offer ratios in db
    db->lock();
    list<list<Type13Entry*>*>::iterator iter;
    for (iter=listOfT13eLists.begin(); iter!=listOfT13eLists.end(); ++iter)
    {
        list<Type13Entry*> *nextList = *iter;
        Type13Entry* entry = nextList->front();
        CompleteID firstId = entry->getFirstID();
        Type12Entry* t12e = db->createT12FromT13Str(*entry->getByteSeq());
        if (t12e!=nullptr)
        {
            db->updateExchangeOfferRatio(firstId, t12e);
            delete t12e;
        }
    }
    db->unlock();
    // send
    byte type = 9;
    str.insert(0, 1, (char)type);
    msgBuilder->sendTransactions(str, listOfT13eLists, socket);
    deleteContent(listOfT13eLists);
}

void RequestProcessor::transferRqstsInfoRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // extract key id
    Util u;
    size_t pos = 0;
    string dum;
    if (str.length()<20) return;
    dum = str.substr(pos,20);
    CompleteID id(dum);
    pos+=20;
    // get currency/obl id
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID currencyId(dum);
    pos+=20;
    // get maxTransId
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID maxTransId(dum);
    pos+=20;
    // get max transRqsts number
    if (str.length()-pos<2) return;
    dum = str.substr(pos,2);
    unsigned short maxTransRqstsNum = min((unsigned short) 25, u.byteSeqAsUs(dum));
    pos+=2;
    if (pos!=str.length()) return;
    // get the ids
    list<CompleteID> idsList;
    db->lock();
    db->getTransferRequests(id, currencyId, maxTransId, maxTransRqstsNum, idsList);
    db->unlock();
    // load signatures
    list<list<Type13Entry*>*> listOfT13eLists;
    list<CompleteID>::iterator it;
    for (it=idsList.begin(); it!=idsList.end(); ++it)
    {
        list<Type13Entry*> *nextList = new list<Type13Entry*>();
        if (!loadSupportingType13Entries(*it, *nextList))
        {
            puts("transferRqstsInfoRequest: nextList could not be loaded");
            delete nextList;
            deleteContent(listOfT13eLists);
            return;
        }
        listOfT13eLists.push_back(nextList);
    }
    // send
    byte type = 6;
    str.insert(0, 1, (char)type);
    msgBuilder->sendTransactions(str, listOfT13eLists, socket);
    deleteContent(listOfT13eLists);
}

void RequestProcessor::nextClaimInfoRequest(const size_t n, byte *request, const int socket)
{
    unsigned long long wellConnectedSince = sh->getWellConnectedSince();
    db->lock();
    bool actingAndNotBanned = db->amCurrentlyActingWithBuffer() && db->dbUpToDate(wellConnectedSince);
    db->unlock();
    if (!actingAndNotBanned) return;
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // extract owner id
    Util u;
    size_t pos = 0;
    string dum;
    if (str.length()<20) return;
    dum = str.substr(pos,20);
    CompleteID id(dum);
    pos+=20;
    // get currency/obl id
    if (str.length()-pos<20) return;
    dum = str.substr(pos,20);
    CompleteID currencyId(dum);
    pos+=20;
    // check length
    if (pos!=str.length()) return;

    // get claim entry
    db->lock();
    Type14Entry* t14e = db->buildNextClaim(id, currencyId);
    db->unlock();
    if (t14e==nullptr)
    {
        puts("nextClaimInfoRequest: t14e==nullptr");
        return;
    }

    // send
    msgBuilder->sendNextClaim(str, t14e, socket);
    delete t14e;
}

bool RequestProcessor::loadSupportingType13Entries(CompleteID &id, list<Type13Entry*> &target)
{
    if (target.size()>0) return false;
    list<Type13Entry*> t13eListInc;
    db->lock();
    CompleteID firstID = db->getFirstID(id);
    CompleteID currentId = db->getLatestID(firstID);
    db->unlock();
    while (currentId.getNotary()>0)
    {
        // load entries for this id
        t13eListInc.clear();
        if (!buildType13Entries(currentId, t13eListInc) || t13eListInc.size()<1)
        {
            deleteContent(target);
            deleteContent(t13eListInc);
            return false;
        }
        // include into main list
        list<Type13Entry*>::iterator it;
        list<Type13Entry*>::iterator it2=target.begin();
        for (it=t13eListInc.begin(); it!=t13eListInc.end(); ++it)
        {
            Type13Entry *t13e = *it;
            if (t13e==nullptr || !t13e->isGood())
            {
                puts("RequestProcessor::loadSupportingType13Entries: bad t13e, unexpected");
                deleteContent(target);
                deleteContent(t13eListInc);
                return false;
            }
            target.insert(it2, t13e);
        }
        // get next id
        Type13Entry *firstInInc = t13eListInc.front();
        currentId = firstInInc->getPredecessorID();
        if (currentId == firstInInc->getCompleteID()) currentId=CompleteID();
    }
    return (target.size()>0);
}

void RequestProcessor::deleteContent(list<Type13Entry*> &entries)
{
    list<Type13Entry*>::iterator it;
    for (it=entries.begin(); it!=entries.end(); ++it)
    {
        if (*it!=nullptr) delete *it;
    }
    entries.clear();
}

void RequestProcessor::deleteContent(list<list<Type13Entry*>*> &listOfLists)
{
    list<list<Type13Entry*>*>::iterator it;
    for (it=listOfLists.begin(); it!=listOfLists.end(); ++it)
    {
        deleteContent(**it);
        if (*it!=nullptr) delete *it;
    }
    listOfLists.clear();
}

void RequestProcessor::considerNotarizationEntryRequest(const size_t n, byte *request)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // extract strings
    list<string> entriesStr;
    string signedSequence;
    string signature;
    Util u;
    unsigned long notaryNr=0;
    unsigned long long timeStamp=0;
    size_t pos = 0;
    while (str.length()-pos > 8)
    {
        // first get length
        string entryStrLengthStr = str.substr(pos,8);
        unsigned long long entryStrLength = u.byteSeqAsUll(entryStrLengthStr);
        pos+=8;
        // get entry or signature
        if (str.length()-pos < entryStrLength)
        {
            return;
        }
        else if (str.length()-pos == entryStrLength)
        {
            if (entryStrLength<13) return;
            // get signature
            signature = str.substr(pos,entryStrLength-12);
            pos+=entryStrLength-12;
            // get notaryNr
            string dum = str.substr(pos,4);
            notaryNr = u.byteSeqAsUl(dum);
            pos+=4;
            // get time stamp
            dum = str.substr(pos,8);
            timeStamp = u.byteSeqAsUll(dum);
            pos+=8;
        }
        else
        {
            string entryStr = str.substr(pos,entryStrLength);
            entriesStr.push_back(entryStr);
            pos+=entryStrLength;

            signedSequence.append(entryStrLengthStr);
            signedSequence.append(entryStr);
        }
    }
    if (pos!=str.length()) return;
    unsigned long long wellConnectedSince = sh->getWellConnectedSince();
    // store to db
    db->lock();
    const bool dbWasUpToDate = db->dbUpToDate(wellConnectedSince);
    // check signature
    int wasAlreadyIntegrated = 0; // undecided
    CompleteID firstID;
    bool renot = false;
    CIDsSet existingEntries;
    CIDsSet addedEntries;
    if (db->verifySignature(signedSequence, signature, notaryNr, timeStamp))
    {
        for (list<string>::iterator it=entriesStr.begin(); it!=entriesStr.end(); ++it)
        {
            Type13Entry t13e(*it);
            // check if entry is already in db
            if (wasAlreadyIntegrated == 0)
            {
                firstID = t13e.getFirstID();
                if (db->isInGeneralList(firstID)) wasAlreadyIntegrated = 1;
                else wasAlreadyIntegrated = -1;

                if (firstID != t13e.getPredecessorID()) renot = true;

                // check if entry is in notarization + determine existingEntries
                if (wasAlreadyIntegrated == -1 && db->getSignatureCount(firstID)>0)
                {
                    list<CompleteID> existingEntriesList;
                    if (db->getExistingEntriesIds(firstID, existingEntriesList, renot)>0)
                    {
                        for (list<CompleteID>::iterator it2=existingEntriesList.begin(); it2!=existingEntriesList.end(); ++it2)
                        {
                            existingEntries.add(*it2);
                        }
                    }
                }
            }
            // exit of already integrated
            if (wasAlreadyIntegrated == 1)
            {
                break;
            }
            // add to db
            if (!db->addType13Entry(&t13e, true))
            {
                if (existingEntries.size()>0) db->deleteTailSignatures(addedEntries, renot);
                else db->deleteSignatures(firstID, renot);
                db->unlock();
                // try do download missing entries
                for (unsigned short counter=0; counter<3; counter++)
                {
                    db->lock();
                    CompleteID entryID = db->getNextEntryToDownload();
                    if (entryID.isZero()) entryID = db->getNextEntryToDownload();
                    db->unlock();

                    if (!entryID.isZero())
                    {
                        sh->requestEntry(entryID, sh->getSomeReachableNotary());
                    }
                }
                return;
            }
            else if (existingEntries.size()>0)
            {
                CompleteID entryID = t13e.getCompleteID();
                if (!existingEntries.contains(entryID)) addedEntries.add(entryID);
            }
        }
    }
    // delete if could not add
    if (wasAlreadyIntegrated == -1 && !db->isInGeneralList(firstID))
    {
        if (existingEntries.size()>0) db->deleteTailSignatures(addedEntries, renot);
        else db->deleteSignatures(firstID, renot);
        db->unlock();
        return;
    }
    db->unlock();

    if (wasAlreadyIntegrated == 0) return;
    else if (wasAlreadyIntegrated == 1)
    {
        return;
    }
    // inform others
    if (dbWasUpToDate)
    {
        sh->sendConsiderNotarizationEntryToAll(firstID);
    }
    else // ask for more entries right away
    {
        wellConnectedSince = sh->getWellConnectedSince();
        db->lock();
        if (db->dbUpToDate(wellConnectedSince))
        {
            db->unlock();
            return;
        }
        db->unlock();

        // check for newer entries
        for (unsigned char listType=0; listType<5; listType++)
        {
            db->lock();
            CompleteID upToDateID = db->getUpToDateID(listType);
            db->unlock();
            sh->checkNewerEntry(listType, upToDateID, 2, false);
        }

        // try to download something
        for (unsigned short counter=0; counter<3; counter++)
        {
            db->lock();
            CompleteID entryID = db->getNextEntryToDownload();
            if (entryID.isZero()) entryID = db->getNextEntryToDownload();
            db->unlock();

            if (!entryID.isZero())
            {
                sh->requestEntry(entryID, sh->getSomeReachableNotary());
            }
        }
    }
}

void RequestProcessor::considerNewerEntriesRequest(const size_t n, byte *request)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    if (str.length()<46) return;
    // extract listType
    Util u;
    size_t pos = 0;
    string dum;
    dum = str.substr(pos,1);
    unsigned char listType = u.byteSeqAsUc(dum);
    pos+=1;
    // extract notaryNr
    dum = str.substr(pos,4);
    unsigned long notaryNr = u.byteSeqAsUl(dum);
    pos+=4;
    // extract id1
    dum = str.substr(pos,20);
    CompleteID id1(dum);
    pos+=20;
    // extract id2
    dum = str.substr(pos,20);
    CompleteID id2(dum);
    pos+=20;
    // extract signature
    string signedSequence = str.substr(0,45);
    string signature = str.substr(pos,str.length()-45);
    // check signature
    db->lock();
    if (!db->verifySignature(signedSequence, signature, notaryNr, db->systemTimeInMs()))
    {
        db->unlock();
        return;
    }
    // report to db
    CompleteID newIndividualUpToDateID = db->newEntriesIdsReport(listType, notaryNr, id1, id2);
    if (newIndividualUpToDateID.getNotary()>0)
    {
        db->unlock();
        sh->checkNewerEntry(listType, notaryNr, newIndividualUpToDateID);
        return;
    }
    else if (id1 == id2 && id1.getNotary() > 0 && !db->isInGeneralList(id1)) // request entry and exit if only one entry
    {
        db->unlock();
        sh->requestEntry(id1, notaryNr);
        return;
    }
    else db->unlock();
    // try do download missing entries
    for (unsigned short counter=0; counter<3; counter++)
    {
        db->lock();
        CompleteID entryID = db->getNextEntryToDownload();
        if (entryID.isZero()) entryID = db->getNextEntryToDownload();
        db->unlock();

        if (!entryID.isZero())
        {
            sh->requestEntry(entryID, sh->getSomeReachableNotary());
        }
    }
}

void RequestProcessor::checkNewerEntryRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    if (str.length()!=21) return;
    // extract listType
    Util u;
    size_t pos = 0;
    string dum;
    dum = str.substr(pos,1);
    unsigned char listType = u.byteSeqAsUc(dum);
    pos+=1;
    // extract entry id
    dum = str.substr(pos,20);
    CompleteID entryId(dum);
    // get ids from db
    CIDsSet newerIds;
    db->lock();
    bool success = db->loadNewerEntriesIds(listType, entryId, newerIds);
    db->unlock();
    // send
    if (!success || newerIds.size()>2 || newerIds.size()<1) return;
    if (newerIds.size()==1)  msgBuilder->sendNewerIds(listType, newerIds.first(), newerIds.first(), socket);
    else msgBuilder->sendNewerIds(listType, newerIds.first(), newerIds.last(), socket);
}

bool RequestProcessor::buildType13Entries(CompleteID &id, list<Type13Entry*> &signaturesList)
{
    db->lock();
    if (!db->loadType13Entries(id, signaturesList))
    {
        db->unlock();
        deleteContent(signaturesList);
        signaturesList.clear();
        return false;
    }
    db->verifySchedules(id);
    // append confirmation entry if necessary
    unsigned short entryLin = db->getType1Entry()->getLineage(id.getTimeStamp());
    Type13Entry* lastT13E = *signaturesList.rbegin();
    unsigned short lastElementLin = db->getType1Entry()->getLineage(lastT13E->getCompleteID().getTimeStamp());
    unsigned short latestLin = db->getType1Entry()->latestLin();
    CompleteID confirmationID = db->getType1Entry()->getConfirmationId();
    db->unlock();
    TNtrNr tNtrNr(latestLin, 1);
    if (entryLin != latestLin && lastT13E->getCompleteID() != confirmationID && msgBuilder->getTNotaryNr() == tNtrNr)
    {
        if (lastElementLin != entryLin) // delete outdated confirmation entry
        {
            puts("new RequestProcessor::buildType13Entries: deleting old confirmation entry");
            signaturesList.pop_back();
            delete lastT13E;
        }
        // build t12e
        db->lock();
        CompleteID firstID = db->getFirstID(id);
        Type12Entry *t12e = db->buildUnderlyingEntry(firstID,0);
        db->unlock();
        // build confirmation entry
        if (msgBuilder->appendConfirmation(signaturesList, t12e, confirmationID))
        {
            // store CE entry in db
            Type13Entry* confEntry = *signaturesList.rbegin();
            db->lock();
            db->saveConfirmationEntry(id, confEntry);
            db->unlock();
        }
        delete t12e;
    }
    return true;
}

void RequestProcessor::notarizationEntryRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    if (str.length()!=20) return;
    // extract entry id
    CompleteID id(str);
    list<Type13Entry*> signaturesList;
    // load from db
    bool success = buildType13Entries(id, signaturesList);
    // send
    if (success) msgBuilder->sendNotarizationEntry(signaturesList, socket);
    deleteContent(signaturesList);
    // if transaction pair send the other entry as well
    db->lock();
    CompleteID firstID = db->getFirstID(id);
    CompleteID zeroID;
    CompleteID otherTransferId = db->getConnectedTransfer(firstID, zeroID);
    db->unlock();
    if (otherTransferId.getNotary()<=0 || otherTransferId==firstID) return;
    success = buildType13Entries(otherTransferId, signaturesList);
    // send
    if (success) msgBuilder->sendNotarizationEntry(signaturesList, socket);
    deleteContent(signaturesList);
}

void RequestProcessor::initialType13EntryRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    if (str.length()!=20) return;
    // extract entry id
    CompleteID id(str);
    string t13eStr;
    db->lock();
    bool success = db->loadType13EntryStr(id, 1, t13eStr);
    if (!success) success = db->loadType13EntryStr(id, 2, t13eStr);
    db->unlock();
    if (success)
    {
        Type13Entry t13e(t13eStr);
        if (t13e.isGood() && t13e.getFirstID() == t13e.getCompleteID())
        {
            msgBuilder->sendSignature(t13e.getByteSeq(), socket);
        }
    }
}

// could actually be an answer to own request (only if moderator) or could be a new request
void RequestProcessor::newSignatureRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // reconstruct type 13 entry
    Util u;
    if (str.length()<8) return;
    string dum = str.substr(0,8);
    unsigned long long t13eLength = u.byteSeqAsUll(dum);
    if (str.length()-8 < t13eLength)
    {
        return;
    }
    string type13entryStr = str.substr(8, t13eLength);
    Type13Entry *type13entry = new Type13Entry(type13entryStr);
    if (!type13entry->isGood())
    {
        puts("RequestProcessor::newSignatureRequest: bad type13entry");
        delete type13entry;
        return;
    }
    CompleteID firstID = type13entry->getFirstID();
    CompleteID predecessorID = type13entry->getPredecessorID();
    CompleteID entryID = type13entry->getCompleteID();
    db->lock();
    if (db->getType1Entry()->getLineage(entryID.getTimeStamp()) != db->getType1Entry()->latestLin())
    {
        db->unlock();
        puts("RequestProcessor::newSignatureRequest: wrong lineage");
        delete type13entry;
        return;
    }
    unsigned long long wellConnectedSince = sh->getWellConnectedSince();
    // check if db is up-to-date
    if (!db->dbUpToDate(wellConnectedSince))
    {
        db->unlock();
        delete type13entry;
        return;
    }
    // check if not yet notarized
    if (db->isInGeneralList(firstID))
    {
        db->unlock();
        delete type13entry;
        return;
    }
    // check if entry is already known (for initial entries only)
    unsigned long existingSignatures = db->getSignatureCount(firstID);
    if (firstID == entryID && existingSignatures > 0)
    {
        db->unlock();
        delete type13entry;
        return;
    }
    // check if predecessor is last in block (for known firstID only)
    if (firstID != entryID && existingSignatures > 0 && !db->isLastInBlock(predecessorID))
    {
        db->unlock();
        delete type13entry;
        return;
    }
    bool amModerator = db->amModerator(firstID);
    if (t13eLength+8 == n-1) // have merely a notification (no EOF flag)
    {
        if (firstID == entryID) // new notarization
        {
            // add to db
            if (existingSignatures==0 && !amModerator && !db->addType13Entry(type13entry, true))
            {
                db->registerConflicts(firstID);
            }
            db->unlock();
            delete type13entry;
            return;
        }
        if (existingSignatures <= 0) // db possibly incomplete
        {
            bool isActing = db->isActingNotary(firstID.getNotary(), firstID.getTimeStamp());
            db->unlock();
            // request initial type 13 entry
            if (isActing)
            {
                sh->askForInitialType13Entry(firstID, entryID.getNotary());
            }
            delete type13entry;
            return;
        }
        // must be moderator from now on
        if (!amModerator)
        {
            db->unlock();
            delete type13entry;
            return;
        }
        // store signature and moderate for next
        list<CompleteID> existingEntriesList;
        if (db->addType13Entry(type13entry, false)
                && (db->getExistingEntriesIds(firstID, existingEntriesList, false)>0
                    || db->getExistingEntriesIds(firstID, existingEntriesList, true)>0))
        {
            unsigned short neededSignatures = db->getType1Entry()->getNeededSignatures(db->getType1Entry()->latestLin());
            if (existingEntriesList.size() < neededSignatures)
            {
                set<unsigned long>* actingNotaries = db->getActingNotaries(db->systemTimeInMs());
                db->unlock();
                // delete those who already signed
                for (list<CompleteID>::iterator it=existingEntriesList.begin(); it!=existingEntriesList.end(); ++it)
                {
                    actingNotaries->erase(it->getNotary());
                }
                // generate relevant notaries list
                set<unsigned long>* notaries = sh->genNotariesList(*actingNotaries);
                actingNotaries->clear();
                delete actingNotaries;
                // send and clean up
                sh->sendNewSignature(type13entry, *notaries);
                notaries->clear();
                delete notaries;
            }
            else
            {
                set<unsigned long>* actingNotaries = db->getActingNotaries(db->systemTimeInMs());
                db->unlock();
                // generate relevant notaries list
                set<unsigned long>* notaries = sh->genNotariesList(*actingNotaries);
                actingNotaries->clear();
                delete actingNotaries;
                // send and clean up
                sh->sendNewSignature(type13entry, *notaries);
                notaries->clear();
                delete notaries;
            }
        }
        else if (db->isInGeneralList(firstID))
        {
            // inform everyone if notarization is complete
            list<Type13Entry*> signaturesList;
            if (db->loadType13Entries(firstID, signaturesList))
            {
                db->unlock();
                sh->sendNotarizationEntryToAll(signaturesList);
                deleteContent(signaturesList);
            }
            else
            {
                db->unlock();
                deleteContent(signaturesList);
            }
        }
        delete type13entry;
        return;
    }
    db->unlock();
    // must have message from moderator from now on
    if (amModerator)
    {
        delete type13entry;
        return;
    }
    size_t pos = type13entryStr.length()+9;
    // extract rank
    if (str.length()-pos<2)
    {
        delete type13entry;
        return;
    }
    dum = str.substr(pos,2);
    unsigned short participationRank = u.byteSeqAsUs(dum);
    pos+=2;
    // get signedSeq
    string signedSeq(type13entryStr);
    signedSeq.append(dum);
    // extract moderator signature length
    if (str.length()-pos<4)
    {
        delete type13entry;
        return;
    }
    dum = str.substr(pos,4);
    unsigned long signatureLength = u.byteSeqAsUl(dum);
    pos+=4;
    // extract moderator signature
    if (str.length()-pos<signatureLength)
    {
        delete type13entry;
        return;
    }
    string signature = str.substr(pos,signatureLength);
    // verify signature contained in dum
    db->lock();
    bool correct = db->verifySignature(signedSeq, signature, firstID.getNotary(), firstID.getTimeStamp());
    if (!correct)
    {
        db->unlock();
        delete type13entry;
        return;
    }
    // add signature to db
    if (!db->addType13Entry(type13entry, true))
    {
        if (firstID == entryID) db->registerConflicts(firstID);
        db->unlock();
        delete type13entry;
        return;
    }
    // inform everyone if notarization complete
    if (db->isInGeneralList(firstID))
    {
        db->unlock();
        sh->sendConsiderNotarizationEntryToAll(firstID);
        delete type13entry;
        return;
    }
    // schedule own signature based on rank
    db->addToEntriesToSign(entryID, participationRank+1);
    db->unlock();
    delete type13entry;
}

void RequestProcessor::notarizationRequest(const size_t n, byte *request, const int socket)
{
    unsigned long long wellConnectedSince = sh->getWellConnectedSince();
    db->lock();
    bool actingAndNotBanned = db->amCurrentlyActingWithBuffer() && db->dbUpToDate(wellConnectedSince);
    db->unlock();
    if (!actingAndNotBanned) return;
    // reconstruct type 12 entry
    stringstream ss;
    for (size_t i=1; i<n; i++) ss << ((char)request[i]);
    string type12entryStr = ss.str();
    Type12Entry *type12entry = new Type12Entry(type12entryStr);
    if (!type12entry->isGood())
    {
        delete type12entry;
        return;
    }
    // check connection
    if (!sh->wellConnected())
    {
        delete type12entry;
        return;
    }
    // get acting notaries
    db->lock();
    set<unsigned long>* actingNotaries = db->getActingNotaries(db->systemTimeInMs());
    db->unlock();
    actingNotaries->erase(msgBuilder->getTNotaryNr().getNotaryNr());
    // generate relevant notaries list
    set<unsigned long>* notaries = sh->genNotariesList(*actingNotaries);
    actingNotaries->clear();
    delete actingNotaries;
    // sign type 12 entry and send
    CompleteID zeroId;
    Type13Entry* signedEntry = msgBuilder->signEntry(nullptr, type12entry, zeroId);
    if (signedEntry != nullptr)
    {
        db->lock();
        // try to add to db
        if (!db->addType13Entry(signedEntry, false))
        {
            // check if key or liqui that is already registered
            CompleteID existingKeyId;
            if (type12entry->underlyingType()==11)
            {
                Type11Entry *t11e = (Type11Entry*) type12entry->underlyingEntry();
                existingKeyId = db->getFirstID(t11e->getPublicKey());
            }
            else if (type12entry->underlyingType()==5 || type12entry->underlyingType()==15)
            {
                Type5Or15Entry *t5Or15e = (Type5Or15Entry*) type12entry->underlyingEntry();
                existingKeyId = db->getCurrencyOrOblId(t5Or15e);
            }
            // clean up and return
            db->unlock();
            if (existingKeyId.getNotary()>0)
            {
                msgBuilder->sendNewerIds(1, existingKeyId, existingKeyId, socket);
            }
            delete signedEntry;
            delete type12entry;
            notaries->clear();
            delete notaries;
            return;
        }
        db->unlock();
        sh->sendNewSignature(signedEntry, *notaries);
        delete signedEntry;
    }
    delete type12entry;
    notaries->clear();
    delete notaries;
}

void RequestProcessor::considerContactInfoRequest(const size_t n, byte *request)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // create ContactInfo
    ContactInfo contactInfo(str);
    db->lock();
    db->tryToStoreContactInfo(contactInfo);
    db->unlock();
}

void RequestProcessor::contactsRequest(const size_t n, byte *request, const int socket)
{
    list<string> contacts;
    db->lock();
    unsigned long long currentTime = db->systemTimeInMs();
    set<unsigned long> *notaries = db->getActingNotaries(currentTime);
    bool result = db->loadContactsList(notaries, contacts);
    db->unlock();
    if (notaries!=nullptr) delete notaries;
    if (result)
    {
        list<string>::iterator it;
        for (it=contacts.begin(); it!=contacts.end(); ++it) msgBuilder->sendContactInfo(*it, socket);
    }
}

void RequestProcessor::essentialsRequest(const size_t n, byte *request, const int socket)
{
    string str;
    for (size_t i=1; i<n; i++) str.push_back((char)request[i]);
    // extract minEntryId
    Util u;
    size_t pos = 0;
    string dum;
    if (str.length()<20) return;
    dum = str.substr(pos,20);
    CompleteID minEntryId(dum);
    pos+=20;
    if (pos!=str.length()) return;
    // get the ids
    list<CompleteID> idsList;
    db->lock();
    db->getEssentialEntries(minEntryId, idsList);
    db->unlock();
    // load signatures
    list<list<Type13Entry*>*> listOfT13eLists;
    list<CompleteID>::iterator it;
    for (it=idsList.begin(); it!=idsList.end(); ++it)
    {
        list<Type13Entry*> *nextList = new list<Type13Entry*>();
        if (!loadSupportingType13Entries(*it, *nextList))
        {
            puts("essentialsRequest: nextList could not be loaded");
            deleteContent(*nextList);
            delete nextList;
            deleteContent(listOfT13eLists);
            return;
        }
        listOfT13eLists.push_back(nextList);
    }
    // send
    msgBuilder->sendEssentials(str, listOfT13eLists, socket);
    deleteContent(listOfT13eLists);
}

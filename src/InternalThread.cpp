#include "InternalThread.h"

#define routineSleepTimeInMcrS 10000

#define checkNewEntriesInterval 400 // in ms

#define signEntriesInterval 75 // in ms
#define downloadNewEntriesInterval 600 // in ms
#define terminateThreadsInterval 700 // in ms
#define startRenotarizationsInterval 1200 // in ms

#define checkUpToDateStatusInterval 1500 // in ms
#define reportUpToDateStatusInterval 2000 // in ms
#define updateNotariesListInterval 2000 // in ms
#define registerKeyInterval 2000 // in ms
#define updateServersInterval 5000 // in ms
#define reportContactsInterval 20000 // in ms
#define updateRenotarizationAttemptsInterval 15000 // in ms

#define maxLoopRepetitionsAtOnce 1000000

InternalThread::InternalThread(Database *d, OtherServersHandler *s, MessageBuilder* m)
    : db(d), servers(s), msgBuilder(m)
{
    running=false;
    stopped=true;
}

void InternalThread::start()
{
    running=true;
    if(pthread_create(&thread, NULL, InternalThread::routine, (void*) this) < 0)
    {
        puts("InternalThread::routine: could not create internal thread");
        running=false;
        return;
    }
    stopped=false;
    pthread_detach(thread);
}

InternalThread::~InternalThread()
{

}

void* InternalThread::routine(void *internalThread)
{
    InternalThread* internal=(InternalThread*) internalThread;
    internal->stopped=false;
    unsigned long long checkNewEntriesNext = 0;
    unsigned long long downloadNewEntriesNext = 0;
    unsigned long long updateServersNext = 0;
    unsigned long long signEntriesNext = 0;
    unsigned long long terminateThreadsNext = 0;
    unsigned long long registerKeyNext = 0;
    unsigned long long updateNotariesListNext = 0;
    unsigned long long reportUpToDateStatusNext = 0;
    unsigned long long checkUpToDateStatusNext = 0;
    unsigned long long startRenotarizationsNext = 0;
    unsigned long long reportContactsNext = 0;
    unsigned long long updateRenotarizationAttemptsNext = 0;
    bool upToDate = false;
    TNtrNr tNotaryNr = internal->msgBuilder->getTNotaryNr(); // own number
    CompleteID pubKeyId;
    bool pubKeyIsRegistered = false;
    bool amActing = false;
    set<unsigned long>* notaries = nullptr;
    unsigned long long currentTime;
    do
    {
        usleep(routineSleepTimeInMcrS);
        currentTime = internal->db->systemTimeInMs();

        // update ip, port etc. for servers
        if (currentTime >= updateServersNext)
        {
            internal->db->lock();
            internal->db->addContactsToServers(internal->servers, tNotaryNr.getNotaryNr());
            internal->db->unlock();

            updateServersNext=currentTime+updateServersInterval;
        }

        // check for missing entries
        if (currentTime >= checkNewEntriesNext)
        {
            for (unsigned char listType=0; listType<5; listType++)
            {
                internal->db->lock();
                CompleteID upToDateID = internal->db->getUpToDateID(listType);
                internal->db->unlock();

                internal->servers->checkNewerEntry(listType, upToDateID, 1);
            }
            checkNewEntriesNext=currentTime+checkNewEntriesInterval;
        }

        // download missing entries
        if (currentTime >= downloadNewEntriesNext)
        {
            // try to download something
            internal->db->lock();
            CompleteID entryID = internal->db->getNextEntryToDownload();
            if (entryID.isZero()) entryID = internal->db->getNextEntryToDownload();
            internal->db->unlock();

            if (!entryID.isZero())
            {
                internal->servers->requestEntry(entryID, internal->servers->getSomeReachableNotary());
            }

            downloadNewEntriesNext=currentTime+downloadNewEntriesInterval;
        }

        // check db up-to-date status
        if (currentTime >= checkUpToDateStatusNext)
        {
            unsigned long long wellConnectedSince = internal->servers->getWellConnectedSince();
            internal->db->lock();
            bool upToDateNew = internal->db->dbUpToDate(wellConnectedSince);
            internal->db->unlock();

            // report db up-to-date status
            if (currentTime >= reportUpToDateStatusNext || (upToDateNew && !upToDate))
            {
                if (!upToDateNew)
                {
                    puts("Database not yet up-to-date. Current status:");

                    Util u;
                    string out;

                    if (wellConnectedSince > 0)
                    {
                        out = "wellConnectedSince: ";
                        out.append(u.epochToStr(wellConnectedSince));
                        puts(out.c_str());
                    }
                    else
                    {
                        puts("not well connected");
                    }

                    // report which notaries this notary is connected to
                    list<unsigned long> notariesList;
                    internal->servers->loadContactsReachable(notariesList);
                    if (notariesList.size()<=0)
                    {
                        puts("not connected to any notary");
                    }
                    else
                    {
                        string txt("currently connected to notaries: ");
                        list<unsigned long>::iterator it;
                        for (it=notariesList.begin(); it!=notariesList.end(); ++it)
                        {
                            if (it != notariesList.begin()) txt.append(", ");
                            txt.append(to_string(*it));
                        }
                        puts(txt.c_str());
                    }

                    // report up-to-date-timestamps
                    internal->db->lock();
                    CompleteID upToDateID1 = internal->db->getUpToDateID(0);
                    CompleteID upToDateID2 = internal->db->getUpToDateID(1);
                    CompleteID upToDateID3 = internal->db->getUpToDateID(2);
                    CompleteID upToDateID4 = internal->db->getUpToDateID(3);
                    CompleteID upToDateID5 = internal->db->getUpToDateID(4);
                    internal->db->unlock();

                    out = "listEssentials up-to-date-timestamp: ";
                    out.append(u.epochToStr(upToDateID1.getTimeStamp()));
                    puts(out.c_str());

                    out = "listGeneral up-to-date-timestamp: ";
                    out.append(u.epochToStr(upToDateID2.getTimeStamp()));
                    puts(out.c_str());

                    out = "listTerminations up-to-date-timestamp: ";
                    out.append(u.epochToStr(upToDateID3.getTimeStamp()));
                    puts(out.c_str());

                    out = "listPerpetuals up-to-date-timestamp: ";
                    out.append(u.epochToStr(upToDateID4.getTimeStamp()));
                    puts(out.c_str());

                    out = "listTransfers up-to-date-timestamp: ";
                    out.append(u.epochToStr(upToDateID5.getTimeStamp()));
                    puts(out.c_str());

                    out = "Downloading ";
                    internal->db->lock();
                    size_t nrToDownload = internal->db->getEntriesInDownload();
                    out.append(to_string(nrToDownload));
                    internal->db->unlock();
                    if (nrToDownload == 1) out.append(" entry.");
                    else out.append(" entries.");
                    puts(out.c_str());

                    puts("Please wait ...");
                }
                else if (!upToDate) puts("Database now up-to-date.");

                reportUpToDateStatusNext=currentTime+reportUpToDateStatusInterval;
            }

            upToDate=upToDateNew;
            checkUpToDateStatusNext=currentTime+checkUpToDateStatusInterval;
        }

        // rebuild corresponding notaries list
        currentTime = internal->db->systemTimeInMs();
        if (currentTime >= updateNotariesListNext)
        {
            // delete notaries list
            if (notaries!=nullptr)
            {
                notaries->clear();
                delete notaries;
            }

            // get acting notaries
            internal->db->lock();
            amActing = internal->db->isActingNotary(tNotaryNr, currentTime);
            set<unsigned long>* actingNotaries = internal->db->getActingNotaries(currentTime);
            internal->db->unlock();

            // generate relevant notaries list
            if (amActing) actingNotaries->erase(tNotaryNr.getNotaryNr());
            notaries = internal->servers->genNotariesList(*actingNotaries);

            actingNotaries->clear();
            delete actingNotaries;

            updateNotariesListNext = currentTime + updateNotariesListInterval;
        }

        // report contacts
        if (currentTime >= reportContactsNext)
        {
            // load contacts from db
            list<string> contacts;
            internal->db->lock();
            bool result = internal->db->loadContactsList(notaries, contacts);
            ContactInfo* contactInfo = internal->db->getContactInfo(tNotaryNr);
            internal->db->unlock();

            // add own contact info
            if (result && contactInfo!=nullptr)
            {
                contacts.push_back(*contactInfo->getByteSeq());
            }
            if (contactInfo!=nullptr) delete contactInfo;

            // send
            if (result)
            {
                internal->servers->sendContactsList(contacts, *notaries);
            }

            // request contact infos for yourself
            internal->servers->sendContactsRqst();

            reportContactsNext=currentTime+reportContactsInterval;
        }

        // check that server is well-connected and up-to-date
        if (!internal->servers->wellConnected(notaries->size()) || !upToDate) continue;

        // check if amActing and if any threads terminated
        internal->db->lock();
        amActing = internal->db->isActingNotaryWithBuffer(tNotaryNr, currentTime);
        if (amActing) internal->db->checkThreadTerminations();
        internal->db->unlock();

        if (!amActing) continue;

        // sign outstanding entries
        if (currentTime >= signEntriesNext)
        {
            string type13entryStr;
            string type12entryStr;
            CompleteID zeroId;
            unsigned long c = 0;
            while (internal->servers->wellConnected() && c<maxLoopRepetitionsAtOnce)
            {
                internal->db->lock();
                if (!internal->db->loadNextEntryToSign(type13entryStr, type12entryStr))
                {
                    internal->db->unlock();
                    break;
                }
                else c++;
                internal->db->unlock();

                Type13Entry *entry = new Type13Entry(type13entryStr);
                Type12Entry *uEntry = new Type12Entry(type12entryStr);
                Type13Entry *signedEntry = internal->msgBuilder->signEntry(entry, uEntry, zeroId);
                if (signedEntry!=nullptr)
                {
                    internal->servers->sendSignatureToAll(signedEntry->getByteSeq());
                    // clean up
                    delete signedEntry;
                }
                delete entry;
                delete uEntry;
                type13entryStr = "";
                type12entryStr = "";
            }
            signEntriesNext=currentTime+signEntriesInterval;
        }

        // update renotarization attempts
        if (currentTime >= updateRenotarizationAttemptsNext)
        {
            internal->db->lock();
            internal->db->updateRenotarizationAttempts();
            internal->db->unlock();
            updateRenotarizationAttemptsNext=currentTime+updateRenotarizationAttemptsInterval;
        }

        // start renotarizations
        if (currentTime >= startRenotarizationsNext)
        {
            CompleteID entryId;
            unsigned long c = 0;
            while (internal->servers->wellConnected() && c<maxLoopRepetitionsAtOnce)
            {
                internal->db->lock();

                int attemptStatus = internal->db->loadNextEntryToRenotarize(entryId);

                if (attemptStatus == -1)
                {
                    internal->db->unlock();
                    break;
                }
                else c++;

                if (attemptStatus == 0)
                {
                    internal->db->unlock();
                    continue;
                }

                string t13eStr;
                if (!internal->db->loadType13EntryStr(entryId, 0, t13eStr))
                {
                    internal->db->unlock();
                    continue;
                }
                Type13Entry t13e(t13eStr);
                CompleteID entryIdFirst = internal->db->getFirstID(entryId);
                string t13eFirstStr;
                if (!internal->db->loadType13EntryStr(entryIdFirst, 0, t13eFirstStr))
                {
                    internal->db->unlock();
                    continue;
                }
                Type12Entry *t12e = internal->db->createT12FromT13Str(t13eFirstStr);

                internal->db->unlock();

                if (t12e == nullptr)
                {
                    continue;
                }

                // create signature
                Type13Entry* signedEntry = internal->msgBuilder->signEntry(&t13e, t12e, entryId);
                if (signedEntry == nullptr)
                {
                    delete t12e;
                    continue;
                }

                // store and send
                internal->db->lock();
                bool result = internal->db->addType13Entry(signedEntry, false);
                internal->db->unlock();

                if (result)
                {
                    internal->servers->sendNewSignature(signedEntry, *notaries);
                }

                delete signedEntry;
                delete t12e;
            }
            startRenotarizationsNext=currentTime+startRenotarizationsInterval;
        }

        pubKeyIsRegistered = pubKeyIsRegistered && internal->db->isFreshNow(pubKeyId);
        internal->db->lock();
        pubKeyIsRegistered = pubKeyIsRegistered && (pubKeyId == internal->db->getLatestNotaryId(tNotaryNr));
        internal->db->unlock();

        // check for new type 9 entries to be created
        if (pubKeyIsRegistered && currentTime >= terminateThreadsNext)
        {
            CompleteID threadId;
            unsigned long c = 0;
            while (internal->servers->wellConnected() && c<maxLoopRepetitionsAtOnce)
            {
                internal->db->lock();
                if (!internal->db->loadNextTerminatingThread(threadId))
                {
                    internal->db->unlock();
                    break;
                }
                else c++;
                internal->db->unlock();

                Type13Entry* signedEntry = internal->msgBuilder->terminateAndSign(threadId);
                if (signedEntry == nullptr)
                {
                    continue;
                }

                internal->db->lock();
                bool result = internal->db->addType13Entry(signedEntry, false);
                internal->db->unlock();

                if (result)
                {
                    internal->servers->sendNewSignature(signedEntry, *notaries);
                }
                delete signedEntry;
            }
            terminateThreadsNext=currentTime+terminateThreadsInterval;
        }

        // register own public key
        if (tNotaryNr.isGood() && !pubKeyIsRegistered)
        {
            if (currentTime >= registerKeyNext)
            {
                internal->db->lock();
                pubKeyId = internal->db->getLatestNotaryId(tNotaryNr);
                internal->db->unlock();

                if (pubKeyId.getNotary() > 0)
                {
                    if (internal->db->isFreshNow(pubKeyId))
                    {
                        internal->msgBuilder->setPublicKeyId(pubKeyId);
                        pubKeyIsRegistered=true;
                    }
                    else
                    {
                        puts("InternalThread::routine: own notary public registered but not fresh");
                    }
                }
                else
                {
                    puts("InternalThread::routine: attempting to register own notary public key in the database");
                    string str;
                    internal->db->lock();
                    const bool loaded = internal->db->loadNotaryPubKey(tNotaryNr, str);
                    internal->db->unlock();
                    if (loaded)
                    {
                        Type13Entry* signedEntry = internal->msgBuilder->packKeyAndSign(str);
                        if (signedEntry != nullptr)
                        {
                            internal->db->lock();
                            if (!internal->db->addType13Entry(signedEntry, false))
                            {
                                internal->db->unlock();
                                puts("InternalThread::routine: (own key registration) could not add signed entry to db");
                            }
                            else
                            {
                                internal->db->unlock();
                                internal->servers->sendNewSignature(signedEntry, *notaries);
                            }
                            delete signedEntry;
                        }
                        else
                        {
                            puts("InternalThread::routine: could not create signedEntry");
                        }
                    }
                    else
                    {
                        puts("InternalThread::routine: could not load own notary public key from db");
                    }
                }
                registerKeyNext=currentTime+registerKeyInterval;
            }
        }
    }
    while(internal->running);

    internal->stopped=true;
    puts("InternalThread::routine stopped");
    return NULL;
}

void InternalThread::stopSafely()
{
    running=false;
    while (!stopped) sleep(1);
}

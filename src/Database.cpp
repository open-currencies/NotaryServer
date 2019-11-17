#include "Database.h"

#define discountTimeBufferInMs 60000
#define maxEntriesToDownload 1000
#define maxDownloadAttempts 5
#define minUpToDateTimeBufferInMs 7000
#define participationTimeBufferInMs 250
#define minTimeBetweenDownloadAttemptsInMs 3000
#define blockCacheSizeInMb 150
#define writeBufferSizeInMb 16

Database::Database(const string& dbDir) : ownNumber(0)
{
    // loading type 1 entry
    type1entry=new Type1Entry(dbDir+"/type1entry");
    if (!type1entry->isGood())
    {
        puts("Type 1 entry could not be loaded.");
        return;
    }

    // options for rocksdb
    table_options.block_cache = rocksdb::NewLRUCache(blockCacheSizeInMb * 1024 * 1024LL);
    table_options.cache_index_and_filter_blocks = true;
    table_options.pin_l0_filter_and_index_blocks_in_cache = true;
    rocksdb::Options options;
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
    options.optimize_filters_for_hits = true;
    options.write_buffer_size = writeBufferSizeInMb * 1024 * 1024LL;
    options.db_write_buffer_size = writeBufferSizeInMb * 1024 * 1024LL;
    options.max_write_buffer_number = 3;
    options.create_if_missing = true;

    rocksdb::Status status;

    // loading notaries list
    status = rocksdb::DB::Open(options, dbDir+"/notaries", &notaries);
    if (!status.ok())
    {
        puts("List of notaries could not be loaded.");
        return;
    }

    // loading entriesInNotarization
    status = rocksdb::DB::Open(options, dbDir+"/entriesInNotarization", &entriesInNotarization);
    if (!status.ok())
    {
        puts("List of entries in notarization could not be loaded.");
        return;
    }

    // loading notarizationEntries
    status = rocksdb::DB::Open(options, dbDir+"/notarizationEntries", &notarizationEntries);
    if (!status.ok())
    {
        puts("General list of notarization entries could not be loaded.");
        return;
    }

    // loading publicKeys
    status = rocksdb::DB::Open(options, dbDir+"/publicKeys", &publicKeys);
    if (!status.ok())
    {
        puts("List of public Kkeys could not be loaded.");
        return;
    }

    // loading subjectToRenotarization
    status = rocksdb::DB::Open(options, dbDir+"/subjectToRenotarization", &subjectToRenotarization);
    if (!status.ok())
    {
        puts("List of entries subject to renotarization could not be loaded.");
        return;
    }

    // loading notaryApplications
    status = rocksdb::DB::Open(options, dbDir+"/notaryApplications", &notaryApplications);
    if (!status.ok())
    {
        puts("List of notary applications could not be loaded.");
        return;
    }

    // loading currenciesAndObligations
    status = rocksdb::DB::Open(options, dbDir+"/currenciesAndObligations", &currenciesAndObligations);
    if (!status.ok())
    {
        puts("List of currencies and obligations could not be loaded.");
        return;
    }

    // loading scheduledActions
    status = rocksdb::DB::Open(options, dbDir+"/scheduledActions", &scheduledActions);
    if (!status.ok())
    {
        puts("List of scheduled Actions could not be loaded.");
        return;
    }

    // loading essentialEntries
    status = rocksdb::DB::Open(options, dbDir+"/essentialEntries", &essentialEntries);
    if (!status.ok())
    {
        puts("List of essential Entries could not be loaded.");
        return;
    }

    // loading conflicts
    status = rocksdb::DB::Open(options, dbDir+"/conflicts", &conflicts);
    if (!status.ok())
    {
        puts("List of conflicts could not be loaded.");
        return;
    }

    // loading perpetuals
    status = rocksdb::DB::Open(options, dbDir+"/perpetualEntries", &perpetualEntries);
    if (!status.ok())
    {
        puts("List of perpetual Entries could not be loaded.");
        return;
    }

    // loading transfers
    status = rocksdb::DB::Open(options, dbDir+"/transfersWithFees", &transfersWithFees);
    if (!status.ok())
    {
        puts("List of transfers With Fees could not be loaded.");
        return;
    }

    puts("Database loaded successfully.");
}

void Database::upToDateReport()
{
    lock();
    string msg;
    msg.append("entriesToDownload: ");
    msg.append(to_string(((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload)->size()));

    msg.append("\nentriesInDownload: ");
    msg.append(to_string(((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload)->size()));

    msg.append("\nmissingPredecessors: ");
    msg.append(to_string(missingPredecessors.size()));

    msg.append("\nmissingNotaries: ");
    msg.append(to_string(missingNotaries.size()));

    msg.append("\nsize of lastDownloadAttempt: ");
    msg.append(to_string(lastDownloadAttempt.size()));

    msg.append("\nsize of entriesToSign: ");
    msg.append(to_string(entriesToSign.size()));

    msg.append("\nsize of listEssentials->individualUpToDatesByID: ");
    msg.append(to_string(listEssentials->individualUpToDatesByID.size()));

    msg.append("\nsize of listGeneral->individualUpToDatesByID: ");
    msg.append(to_string(listGeneral->individualUpToDatesByID.size()));

    msg.append("\nsize of listTerminations->individualUpToDatesByID: ");
    msg.append(to_string(listTerminations->individualUpToDatesByID.size()));

    msg.append("\nsize of listPerpetuals->individualUpToDatesByID: ");
    msg.append(to_string(listPerpetuals->individualUpToDatesByID.size()));

    msg.append("\nsize of listTransfers->individualUpToDatesByID: ");
    msg.append(to_string(listTransfers->individualUpToDatesByID.size()));

    puts(msg.c_str());
    unlock();
}

void Database::rocksdbReport()
{
    lock();
    string msg;
    msg.append("Block cache usage: ");
    msg.append(to_string(table_options.block_cache->GetUsage()));

    msg.append("\n  Pinned usage: ");
    msg.append(to_string(table_options.block_cache->GetPinnedUsage()));

    msg.append("\nIndex and filter blocks (notaries): ");
    string out;
    notaries->GetProperty("rocksdb.estimate-table-readers-mem", &out);
    msg.append(out);

    msg.append("\nMemtable (notaries): ");
    out.clear();
    notaries->GetProperty("rocksdb.cur-size-all-mem-tables", &out);
    msg.append(out);

    msg.append("\nIndex and filter blocks (entriesInNotarization): ");
    out.clear();
    entriesInNotarization->GetProperty("rocksdb.estimate-table-readers-mem", &out);
    msg.append(out);

    msg.append("\nMemtable (entriesInNotarization): ");
    out.clear();
    entriesInNotarization->GetProperty("rocksdb.cur-size-all-mem-tables", &out);
    msg.append(out);

    msg.append("\nIndex and filter blocks (notarizationEntries): ");
    out.clear();
    notarizationEntries->GetProperty("rocksdb.estimate-table-readers-mem", &out);
    msg.append(out);

    msg.append("\nMemtable (notarizationEntries): ");
    out.clear();
    notarizationEntries->GetProperty("rocksdb.cur-size-all-mem-tables", &out);
    msg.append(out);

    msg.append("\nIndex and filter blocks (publicKeys): ");
    out.clear();
    publicKeys->GetProperty("rocksdb.estimate-table-readers-mem", &out);
    msg.append(out);

    msg.append("\nMemtable (publicKeys): ");
    out.clear();
    publicKeys->GetProperty("rocksdb.cur-size-all-mem-tables", &out);
    msg.append(out);

    msg.append("\nIndex and filter blocks (perpetualEntries): ");
    out.clear();
    perpetualEntries->GetProperty("rocksdb.estimate-table-readers-mem", &out);
    msg.append(out);

    msg.append("\nMemtable (perpetualEntries): ");
    out.clear();
    perpetualEntries->GetProperty("rocksdb.cur-size-all-mem-tables", &out);
    msg.append(out);

    puts(msg.c_str());
    unlock();
}

bool Database::init(unsigned long ownNr, TNtrNr corrNotary, CryptoPP::RSA::PublicKey* corrNotaryPublicKey)
{
    lock();

    if (!type1entry->isGood())
    {
        unlock();
        return false;
    }

    ownNumber=ownNr;
    correspondingNotary=corrNotary;
    correspondingNotaryPublicKey=corrNotaryPublicKey;

    // initialize notaries mentioned in the type1entry
    unsigned short latestLin = type1entry->latestLin();
    for (unsigned short i=1; i<=latestLin; i++)
    {
        unsigned long N = type1entry->getMaxActing(i);
        unsigned long nrOfNotInLin = getNrOfNotariesInLineage(i);
        if (nrOfNotInLin<N)
        {
            puts("adding initial notary keys to database");
            for (unsigned long j=nrOfNotInLin + 1; j<=N; j++)
            {
                string tNtrNrStr = TNtrNr(i,j).toString();
                string *publicKey = type1entry->getPublicKey(i,j);
                string key;
                key.push_back('B'); // prefix for body
                key.append(tNtrNrStr);
                key.append("PK"); // prefix for public key
                notaries->Put(rocksdb::WriteOptions(), key, *publicKey);

                key = "";
                key.push_back('K'); // prefix for identification by public key
                key.append(util.UlAsByteSeq(publicKey->length()));
                key.append(*publicKey);
                notaries->Put(rocksdb::WriteOptions(), key, tNtrNrStr);

                key = "";
                key.push_back('B'); // prefix for body
                key.append(tNtrNrStr);
                key.append("SK"); // prefix for share to keep
                notaries->Put(rocksdb::WriteOptions(), key,  util.DblAsByteSeq(1));

                // check if id already assigned
                CompleteID id = getFirstID(publicKey);
                if (id.getNotary() > 0)
                {
                    // save id to notaries
                    key = "";
                    key.push_back('B'); // prefix for body
                    key.append(tNtrNrStr);
                    key.append("ID"); // prefix for id
                    notaries->Put(rocksdb::WriteOptions(), key,  id.to20Char());

                    // save total notary number to public keys
                    key = "";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(id.to20Char());
                    key.append("TN"); // suffix for total notary number
                    publicKeys->Put(rocksdb::WriteOptions(), key, tNtrNrStr);
                }
            }
            // update notaries list count
            string key;
            key.push_back('H'); // prefix for list head (nr of entries)
            key.append(util.UsAsByteSeq(i));
            notaries->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(N));
        }
    }

    // initialize UpToDateTimeInfo
    puts("initializing UpToDateTimeInfo");
    listEssentials = new UpToDateTimeInfo(loadUpToDateID(0));
    listGeneral = new UpToDateTimeInfo(loadUpToDateID(1));
    listTerminations = new UpToDateTimeInfo(loadUpToDateID(2));
    listPerpetuals = new UpToDateTimeInfo(loadUpToDateID(3));
    listTransfers = new UpToDateTimeInfo(loadUpToDateID(4));

    set<unsigned long>* notariesSet = getFutureActingNotaries(systemTimeInMs());
    if (notariesSet->count(ownNumber)>0) notariesSet->erase(ownNumber);
    set<unsigned long>::iterator it;
    for (it = notariesSet->begin(); it != notariesSet->end(); ++it)
    {
        listEssentials->individualUpToDates.insert(pair<unsigned long, CompleteID>(*it, *listEssentials->getUpToDateIDOverall()));
        listEssentials->individualUpToDatesByID.insert(pair<CompleteID, unsigned long>(*listEssentials->getUpToDateIDOverall(), *it));

        listGeneral->individualUpToDates.insert(pair<unsigned long, CompleteID>(*it, *listGeneral->getUpToDateIDOverall()));
        listGeneral->individualUpToDatesByID.insert(pair<CompleteID, unsigned long>(*listGeneral->getUpToDateIDOverall(), *it));

        listTerminations->individualUpToDates.insert(pair<unsigned long, CompleteID>(*it, *listTerminations->getUpToDateIDOverall()));
        listTerminations->individualUpToDatesByID.insert(pair<CompleteID, unsigned long>(*listTerminations->getUpToDateIDOverall(), *it));

        listPerpetuals->individualUpToDates.insert(pair<unsigned long, CompleteID>(*it, *listPerpetuals->getUpToDateIDOverall()));
        listPerpetuals->individualUpToDatesByID.insert(pair<CompleteID, unsigned long>(*listPerpetuals->getUpToDateIDOverall(), *it));

        listTransfers->individualUpToDates.insert(pair<unsigned long, CompleteID>(*it, *listTransfers->getUpToDateIDOverall()));
        listTransfers->individualUpToDatesByID.insert(pair<CompleteID, unsigned long>(*listTransfers->getUpToDateIDOverall(), *it));
    }
    notariesSet->clear();
    delete notariesSet;

    entriesToDownload = new map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>();
    entriesInDownload = new map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>();

    unlock();

    return true;
}

// db must be locked for this
bool Database::dbUpToDate(unsigned long long wellConnectedSince)
{
    if (wellConnectedSince<=0) return false;

    if (listEssentials->individualUpToDates.size()<=0
            && listGeneral->individualUpToDates.size()<=0
            && listTerminations->individualUpToDates.size()<=0
            && listPerpetuals->individualUpToDates.size()<=0
            && listTransfers->individualUpToDates.size()<=0) return true;

    unsigned long long overallUpToDate = min(getUpToDateID(0).getTimeStamp(), getUpToDateID(1).getTimeStamp());
    overallUpToDate = min(overallUpToDate, getUpToDateID(2).getTimeStamp());
    overallUpToDate = min(overallUpToDate, getUpToDateID(3).getTimeStamp());
    overallUpToDate = min(overallUpToDate, getUpToDateID(4).getTimeStamp());

    if (overallUpToDate <= wellConnectedSince + minUpToDateTimeBufferInMs) return false;

    unsigned long long currentTime = systemTimeInMs();
    if (type1entry->getLineage(currentTime) != type1entry->latestLin()) return false;

    unsigned long long upToDateTimeBuffer = type1entry->getLatestMaxNotarizationTime() * 2;
    upToDateTimeBuffer += minUpToDateTimeBufferInMs;
    if (overallUpToDate + upToDateTimeBuffer <= currentTime) return false;

    return true;
}

// db must be locked for this
CompleteID Database::getUpToDateID(unsigned char listType)
{
    UpToDateTimeInfo* info = getInfoFromType(listType);
    if (info==nullptr) return CompleteID();
    // check if UpToDateTimeInfo follows the right notaries
    unsigned long long currentTime = systemTimeInMs();
    // first delete outdated notaries
    if (info->individualUpToDates.size()>0)
    {
        unsigned long firstNotaryNr = info->individualUpToDates.begin()->first;
        while (!type1entry->isActingNotary(firstNotaryNr, currentTime))
        {
            // delete this notary from UpToDateTimeInfo
            CompleteID individualUpToDateId = info->individualUpToDates[firstNotaryNr];
            info->individualUpToDatesByID.erase(pair<CompleteID,unsigned long>(individualUpToDateId,firstNotaryNr));
            info->individualUpToDates.erase(firstNotaryNr);
            if (info->conditionalUpToDates.count(firstNotaryNr)>0)
            {
                delete info->conditionalUpToDates[firstNotaryNr];
                info->conditionalUpToDates.erase(firstNotaryNr);
            }
            if (info->individualUpToDates.size()<=0) break;
            else firstNotaryNr = info->individualUpToDates.begin()->first;
        }
    }
    // add new acting notaries
    // first determine latest acting notary
    unsigned short latestLin = type1entry->latestLin();
    unsigned long long linStart = type1entry->getNotaryTenureStartFormal(latestLin,1);
    unsigned long tenure = type1entry->getNotaryTenure(latestLin);
    double delta = ((double) tenure) / type1entry->getMaxActing(latestLin) * 1000;
    unsigned long latestActingNr = 1 + (unsigned long)((double)(currentTime - linStart) / delta);
    unsigned long notariesCount = getNrOfNotariesInLineage(latestLin);
    if (notariesCount < latestActingNr) latestActingNr = notariesCount;
    // add missing notaries beginning with latest
    unsigned long notaryCandidate = latestActingNr;
    while (notaryCandidate>0 && info->individualUpToDates.count(notaryCandidate)<=0)
    {
        if (isActingNotary(notaryCandidate, currentTime) && notaryCandidate != ownNumber)
        {
            info->individualUpToDates.insert(pair<unsigned long, CompleteID>(notaryCandidate, *info->getUpToDateIDOverall()));
            info->individualUpToDatesByID.insert(pair<CompleteID, unsigned long>(*info->getUpToDateIDOverall(), notaryCandidate));
        }
        notaryCandidate--;
    }
    return *info->getUpToDateIDOverall();
}

Database::~Database()
{
    delete type1entry;
    delete notaries;
    delete entriesInNotarization;
    delete notarizationEntries;
    delete publicKeys;
    delete subjectToRenotarization;
    delete notaryApplications;
    delete currenciesAndObligations;
    delete scheduledActions;
    delete essentialEntries;
    delete conflicts;
    delete perpetualEntries;
    delete transfersWithFees;

    // destroy entriesToDownload, entriesInDownload
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>::iterator it;
    for (it = ((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload)->begin();
            it != ((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload)->end(); ++it)
    {
        DownloadStatus* status = it->second;
        delete status;
    }
    ((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload)->clear();
    delete entriesToDownload;
    for (it = ((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload)->begin();
            it != ((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload)->end(); ++it)
    {
        DownloadStatus* status = it->second;
        delete status;
    }
    ((map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload)->clear();
    delete entriesInDownload;

    // destroy UpToDateTimeInfo
    delete listEssentials;
    delete listGeneral;
    delete listTerminations;
    delete listPerpetuals;
    delete listTransfers;

    // destroy entriesToSign
    entriesToSign.clear();
}

// db must be locked for this
bool Database::loadNewerEntriesIds(unsigned char listType, CompleteID &benchmarkId, CIDsSet &newerIDs)
{
    if (!newerIDs.isEmpty()) return false;

    string keyPref;
    rocksdb::Iterator* it;
    string keyStr;

    if (listType==0)
    {
        keyPref.push_back('B'); // prefix for body
        it = essentialEntries->NewIterator(rocksdb::ReadOptions());
    }
    else if (listType==1)
    {
        keyPref.push_back('B'); // prefix for body
        it = notarizationEntries->NewIterator(rocksdb::ReadOptions());
    }
    else if (listType==2)
    {
        keyPref.push_back('B'); // prefix for body
        keyPref.append(util.UcAsByteSeq(5));
        it = notaryApplications->NewIterator(rocksdb::ReadOptions());
    }
    else if (listType==3)
    {
        keyPref.push_back('B'); // prefix for body
        it = perpetualEntries->NewIterator(rocksdb::ReadOptions());
    }
    else if (listType==4)
    {
        keyPref.push_back('B'); // prefix for body
        it = transfersWithFees->NewIterator(rocksdb::ReadOptions());
    }
    else return false;

    // define upper limit
    unsigned long long currentTime = systemTimeInMs();
    unsigned long long upToDateBuffer = type1entry->getLatestMaxNotarizationTime();
    upToDateBuffer += (upToDateBuffer/2);
    if (currentTime <= upToDateBuffer) return false;
    CompleteID upperLimit(0, 0, currentTime-upToDateBuffer);

    // search forward
    unsigned long prefLength = keyPref.length();
    string keyMin;
    keyMin.append(keyPref);
    keyMin.append(benchmarkId.to20Char());
    for (it->Seek(keyMin); it->Valid() && newerIDs.size()<2; it->Next())
    {
        // check the prefix
        keyStr = it->key().ToString();
        if (keyStr.length() < prefLength+20) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // extract id
        string idStr = keyStr.substr(prefLength, 20);
        CompleteID id = CompleteID(idStr);
        // check whether upper limit is reached
        if (id >= upperLimit) break;
        // add if new
        if (id.getNotary()<=0 || id<=benchmarkId || newerIDs.contains(id) || !isInGeneralList(id)) continue;
        else newerIDs.add(id);
    }
    delete it;

    // report if enough found
    if (newerIDs.size()>1) return true;

    // report
    newerIDs.add(upperLimit);
    return true;
}

// db must be locked for this
CompleteID Database::loadUpToDateID(unsigned char listType)
{
    string key;
    string value;
    unsigned long long minTime;
    unsigned long long currentTime = systemTimeInMs();
    rocksdb::Status s;
    if (listType==0)
    {
        minTime = type1entry->getNotaryTenureStartCorrected(1,1) - 1;
        key.push_back('H'); // prefix for head
        s = essentialEntries->Get(rocksdb::ReadOptions(), key, &value);
    }
    else if (listType==1)
    {
        minTime = type1entry->getOldestPossibleFreshEntryTime(currentTime) - 1;
        key.push_back('H'); // prefix for head
        s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    }
    else if (listType==2)
    {
        minTime = type1entry->getNotaryTenureStartCorrected(1,1) - 1;
        key.push_back('H'); // prefix for head
        s = notaryApplications->Get(rocksdb::ReadOptions(), key, &value);
    }
    else if (listType==3)
    {
        minTime = type1entry->getNotaryTenureStartCorrected(1,1) - 1;
        key.push_back('H'); // prefix for head
        s = perpetualEntries->Get(rocksdb::ReadOptions(), key, &value);
    }
    else if (listType==4)
    {
        minTime = type1entry->getNotaryTenureStartCorrected(1,1) - 1;
        key.push_back('H'); // prefix for head
        s = transfersWithFees->Get(rocksdb::ReadOptions(), key, &value);
    }
    const bool success = (s.ok() && value.length()>2);
    CompleteID minID(1,1,minTime);
    if (!success) return minID;
    CompleteID id = CompleteID(value);
    if (minID < id) return id;
    return minID;
}

// for this the db object must be locked first
void Database::saveUpToDateTime(unsigned char listType)
{
    string key;
    if (listType==0)
    {
        key.push_back('H'); // prefix for head
        essentialEntries->Put(rocksdb::WriteOptions(), key, listEssentials->getUpToDateIDOverall()->to20Char());
    }
    else if (listType==1)
    {
        key.push_back('H'); // prefix for head
        notarizationEntries->Put(rocksdb::WriteOptions(), key, listGeneral->getUpToDateIDOverall()->to20Char());
    }
    else if (listType==2)
    {
        key.push_back('H'); // prefix for head
        notaryApplications->Put(rocksdb::WriteOptions(), key, listTerminations->getUpToDateIDOverall()->to20Char());
    }
    else if (listType==3)
    {
        key.push_back('H'); // prefix for head
        perpetualEntries->Put(rocksdb::WriteOptions(), key, listPerpetuals->getUpToDateIDOverall()->to20Char());
    }
    else if (listType==4)
    {
        key.push_back('H'); // prefix for head
        transfersWithFees->Put(rocksdb::WriteOptions(), key, listTransfers->getUpToDateIDOverall()->to20Char());
    }
}

// for this the db object must be locked first
CompleteID Database::getOldestEntryToDownload()
{
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesInD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload;
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesToD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload;
    if (entriesInD->empty() && entriesToD->empty()) return CompleteID(0,0,0);
    else if (entriesInD->empty()) return entriesToD->begin()->first;
    else if (entriesToD->empty()) return entriesInD->begin()->first;
    CompleteID id1 = entriesToD->begin()->first;
    CompleteID id2 = entriesInD->begin()->first;
    if (id1<id2) return id1;
    return id2;
}

// for this the db object must be locked first
size_t Database::getEntriesInDownload()
{
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesInD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload;
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesToD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload;
    return entriesToD->size() + entriesInD->size();
}

// for this the db object must be locked first
CompleteID Database::getNextEntryToDownload()
{
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesInD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload;
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesToD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload;
    unsigned long long currentTime = systemTimeInMs();
    CompleteID zeroID;
    while (!entriesToD->empty())
    {
        CompleteID id = entriesToD->begin()->first;
        DownloadStatus *status = entriesToD->begin()->second;
        entriesToD->erase(id);

        if ((isInGeneralList(id) && getConnectedTransfer(id,zeroID)!=id)
                || status->attempts >= maxDownloadAttempts)
        {
            delete status;
            if (missingPredecessors.count(id)>0)
            {
                delete missingPredecessors[id];
                missingPredecessors.erase(id);
            }
            if (missingNotaries.count(id)>0)
            {
                delete missingNotaries[id];
                missingNotaries.erase(id);
            }
            if (lastDownloadAttempt.count(id)>0)
            {
                lastDownloadAttempt.erase(id);
            }
        }
        else
        {
            entriesInD->insert(pair<CompleteID, DownloadStatus*>(id,status));
            // check if any predecessors still missing
            bool predecessorsMissing = true;
            if (missingPredecessors.count(id) > 0)
            {
                CIDsSet* listMissing = missingPredecessors[id];
                while (!listMissing->isEmpty())
                {
                    CompleteID firstMissing = listMissing->first();
                    if (isInGeneralList(firstMissing)
                            || (entriesToD->count(firstMissing) <= 0
                                && entriesInD->count(firstMissing) <= 0))
                    {
                        listMissing->deleteFirst();
                    }
                    else break;
                }
                if (listMissing->isEmpty())
                {
                    delete listMissing;
                    missingPredecessors.erase(id);
                    predecessorsMissing = false;
                }
            }
            else predecessorsMissing = false;
            // check if any notaries still missing
            bool notariesMissing = true;
            if (missingNotaries.count(id) > 0)
            {
                set<unsigned long>* listMissing = missingNotaries[id];
                while (listMissing->size() > 0)
                {
                    unsigned long firstMissing = *listMissing->begin();
                    if (isActingNotary(firstMissing, id.getTimeStamp()))
                    {
                        listMissing->erase(listMissing->begin());
                    }
                    else break;
                }
                if (listMissing->size() <= 0)
                {
                    delete listMissing;
                    missingNotaries.erase(id);
                    notariesMissing = false;
                }
            }
            else notariesMissing = false;
            // attempt download
            if (!predecessorsMissing &&
                    (lastDownloadAttempt.count(id) <= 0
                     || currentTime >= lastDownloadAttempt[id] + minTimeBetweenDownloadAttemptsInMs))
            {
                if (lastDownloadAttempt.count(id) <= 0)
                {
                    lastDownloadAttempt.insert(pair<CompleteID, unsigned long long>(id,currentTime));
                }
                else lastDownloadAttempt[id] = currentTime;
                status->attempts++;
                if (!notariesMissing)
                {
                    return id;
                }
            }
        }
    }
    volatile map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *tmp=entriesInDownload;
    entriesInDownload=entriesToDownload;
    entriesToDownload=tmp;
    return CompleteID(0,0,0);
}

unsigned long long Database::systemTimeInMs()
{
    using namespace std::chrono;
    milliseconds ms = duration_cast< milliseconds >(
                          system_clock::now().time_since_epoch()
                      );
    return ms.count();
}

// db must be locked for this
bool Database::addToEntriesToSign(CompleteID &signatureId, unsigned short participationRank)
{
    if (participationRank<=0) return false;
    unsigned int lineage = type1entry->getLineage(signatureId.getTimeStamp());
    if (lineage != type1entry->latestLin()) return false;
    if (ownNumber <= 0) return false;
    // calculate signature time
    unsigned long long time = signatureId.getTimeStamp();
    time += (participationTimeBufferInMs * participationRank);
    pair<unsigned long long,CompleteID> LLC(time, signatureId);
    if (entriesToSign.count(LLC)<=0)
    {
        entriesToSign.insert(LLC);
    }
    return true;
}

// db must be locked for this
bool Database::isLastInBlock(CompleteID &signatureID)
{
    // find corresponding block
    bool renot = false;
    CompleteID firstSignId = getFirstNotSignId(signatureID, renot);
    if (firstSignId.getNotary()<=0)
    {
        renot=true;
        firstSignId = getFirstNotSignId(signatureID, renot);
    }
    if (firstSignId.getNotary()<=0)
    {
        return false;
    }
    // try to find successor in block
    string key;
    rocksdb::DB* dbList = entriesInNotarization;
    if (renot)
    {
        key.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;
    }
    key.append(signatureID.to20Char());
    key.push_back('N'); // suffix for next type 13 entry
    string value;
    rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (success)
    {
        return false;
    }
    return true;
}

// db must be locked for this
bool Database::loadNextEntryToSign(string &type13entryStr, string &type12entryStr)
{
    if (entriesToSign.empty())
    {
        return false;
    }
    CompleteID id;
    while (id.isZero() && !entriesToSign.empty())
    {
        pair<unsigned long long,CompleteID> item = *entriesToSign.begin();
        if (item.first > systemTimeInMs()) return false;
        id = item.second;
        entriesToSign.erase(item);
        if (type1entry->getLineage(item.first) != type1entry->latestLin()) id=CompleteID();
        else if (!isLastInBlock(id)) id=CompleteID();
    }
    if (id.isZero()) return false;
    // load strings
    unsigned char l = 1;
    if (!loadType13EntryStr(id, l, type13entryStr))
    {
        l = 2;
        if (!loadType13EntryStr(id, l, type13entryStr)) return false;
    }
    if (!loadUnderlyingType12EntryStr(id, l, type12entryStr)) return false;
    return true;
}

// db must be locked for this
bool Database::amCurrentlyActing()
{
    if (ownNumber<=0) return false;
    return isActingNotary(ownNumber, systemTimeInMs());
}

// db must be locked for this
bool Database::amCurrentlyActingWithBuffer()
{
    if (ownNumber<=0) return false;
    unsigned long long currentTime = systemTimeInMs();
    unsigned short lineageNr = type1entry->getLineage(currentTime);
    TNtrNr tNtrNr(lineageNr, ownNumber);
    if (!tNtrNr.isGood()) return false;
    return isActingNotaryWithBuffer(tNtrNr, currentTime);
}

// db must be locked for this
bool Database::isActingNotaryWithBuffer(TNtrNr tNtrNr, unsigned long long currentTime)
{
    bool isActing = isActingNotary(tNtrNr, currentTime);
    if (isActing)
    {
        unsigned long long upToDateTimeBuffer = type1entry->getLatestMaxNotarizationTime();
        upToDateTimeBuffer += (upToDateTimeBuffer * 2 / 3);
        if (currentTime <= upToDateTimeBuffer) isActing = false;
        else isActing = isActingNotary(tNtrNr, currentTime - upToDateTimeBuffer);
    }
    return isActing;
}

// db must be locked for this
bool Database::isActingNotary(TNtrNr tNtrNr, unsigned long long currentTime)
{
    if (type1entry->getLineage(currentTime) != tNtrNr.getLineage()) return false;
    if (!isActingNotary(tNtrNr.getNotaryNr(), currentTime)) return false;
    return true;
}

// db must be locked for this
bool Database::isActingNotary(unsigned long notaryNr, unsigned long long currentTime)
{
    if (!type1entry->isActingNotary(notaryNr, currentTime)) return false;
    unsigned short lineageNr = type1entry->getLineage(currentTime);
    if (getNotaryTenureStart(TNtrNr(lineageNr, notaryNr)) > currentTime) return false;
    return true;
}

// db must be locked for this
set<unsigned long>* Database::getFutureActingNotaries(unsigned long long futureTime)
{
    // calculate theoretical numbers (interval)
    unsigned short lineageNr = type1entry->getLineage(futureTime);
    if (lineageNr == 0)
    {
        set<unsigned long>* notariesList = new set<unsigned long>();
        return notariesList;
    }
    unsigned long N = type1entry->getMaxActing(lineageNr);
    unsigned long long linStart = type1entry->getNotaryTenureStartFormal(lineageNr,1);
    if (futureTime<linStart)
    {
        set<unsigned long>* notariesList = new set<unsigned long>();
        return notariesList;
    }
    unsigned long tenure = type1entry->getNotaryTenure(lineageNr);
    double delta = ((double) tenure) / N * 1000;
    unsigned long latestActingNr = 1 + (unsigned long)((double)(futureTime - linStart) / delta);
    unsigned long earliestActingNr = 1;
    if (latestActingNr >= N) earliestActingNr = 1 + latestActingNr - N;

    // create actual list
    set<unsigned long>* notariesList = new set<unsigned long>();
    for (unsigned long i=earliestActingNr; i<=latestActingNr; i++) notariesList->insert(i);
    return notariesList;
}

// db must be locked for this
set<unsigned long>* Database::getActingNotaries(unsigned long long currentTime)
{
    // calculate theoretical numbers (interval)
    unsigned short lineageNr = type1entry->getLineage(currentTime);
    if (lineageNr == 0)
    {
        set<unsigned long>* notariesList = new set<unsigned long>();
        return notariesList;
    }
    unsigned long N = type1entry->getMaxActing(lineageNr);
    unsigned long long linStart = type1entry->getNotaryTenureStartFormal(lineageNr,1);
    if (currentTime<linStart)
    {
        set<unsigned long>* notariesList = new set<unsigned long>();
        return notariesList;
    }
    unsigned long tenure = type1entry->getNotaryTenure(lineageNr);
    double delta = ((double) tenure) / N * 1000;
    unsigned long latestActingNr = 1 + (unsigned long)((double)(currentTime - linStart) / delta);
    unsigned long earliestActingNr = 1;
    if (latestActingNr >= N) earliestActingNr = 1 + latestActingNr - N;

    // create actual list
    set<unsigned long>* notariesList = new set<unsigned long>();
    unsigned long notariesCount = getNrOfNotariesInLineage(lineageNr);
    if (notariesCount < earliestActingNr) return notariesList;
    if (notariesCount < latestActingNr) latestActingNr = notariesCount;
    for (unsigned long i=earliestActingNr; i<=latestActingNr; i++)
    {
        if (getNotaryTenureStart(TNtrNr(lineageNr, i)) <= currentTime) notariesList->insert(i);
    }
    return notariesList;
}

// db must be locked for this
bool Database::tryToStoreContactInfo(ContactInfo &contactInfo)
{
    // conduct checks
    TNtrNr totalNotaryNr = contactInfo.getTotalNotaryNr();
    if (!totalNotaryNr.isGood())
    {
        return false;
    }
    unsigned long long timeStamp = contactInfo.getValidSince();
    unsigned long long currentTime = systemTimeInMs();
    if (timeStamp > currentTime)
    {
        return false;
    }
    if (type1entry->getLineage(timeStamp) != totalNotaryNr.getLineage())
    {
        return false;
    }
    ContactInfo* oldInfo = getContactInfo(totalNotaryNr);
    if (oldInfo!=nullptr)
    {
        if (oldInfo->getValidSince() <= currentTime && oldInfo->getValidSince() >= timeStamp)
        {
            delete oldInfo;
            return false;
        }
        delete oldInfo;
    }
    // check signature
    string signedStr;
    signedStr.append(contactInfo.getIP());
    signedStr.append(util.UlAsByteSeq(contactInfo.getPort()));
    signedStr.append(util.UllAsByteSeq(contactInfo.getValidSince()));
    string *signature = contactInfo.getSignature();
    bool result = verifySignature(signedStr, *signature, totalNotaryNr.getNotaryNr(), contactInfo.getValidSince());
    // store
    if (!result)
    {
        return false;
    }
    string key;
    key.push_back('B'); // prefix for body
    key.append(totalNotaryNr.toString());
    key.append("CI"); // prefix for contact info
    notaries->Put(rocksdb::WriteOptions(), key, *contactInfo.getByteSeq());
    return true;
}

// db must be locked for this
bool Database::loadContactsList(set<unsigned long>* notaries, list<string> &contacts)
{
    if (notaries==nullptr || contacts.size()!=0) return false;
    unsigned long long currentTime = systemTimeInMs();
    unsigned short lineageNr = type1entry->getLineage(currentTime);
    set<unsigned long>::iterator it;
    for (it=notaries->begin(); it!=notaries->end(); ++it)
    {
        TNtrNr totalNotaryNr(lineageNr, *it);
        ContactInfo* contactInfo = getContactInfo(totalNotaryNr);
        if (contactInfo!=nullptr)
        {
            contacts.push_back(*contactInfo->getByteSeq());
            delete contactInfo;
        }
    }
    return true;
}

// db must be locked for this
ContactInfo* Database::getContactInfo(TNtrNr &totalNotaryNr)
{
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(totalNotaryNr.toString());
    key.append("CI"); // prefix for contact info
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return nullptr;
    ContactInfo* out = new ContactInfo(value);
    return out;
}

// db must be locked for this
unsigned long long Database::actingUntil(TNtrNr tNtrNr)
{
    unsigned long notaryNr = tNtrNr.getNotaryNr();
    unsigned short lineageNr = tNtrNr.getLineage();
    if (lineageNr != type1entry->latestLin()) return 0;
    return type1entry->getNotaryTenureEndCorrected(lineageNr, notaryNr);
}

// db must be locked for this
void Database::addContactsToServers(OtherServersHandler *servers, unsigned long ownNr)
{
    unsigned long long currentTime = systemTimeInMs();
    unsigned short lineageNr = type1entry->getLineage(currentTime);
    if (lineageNr != type1entry->latestLin()) return;
    set<unsigned long>* notariesSet = getActingNotaries(currentTime);
    if (notariesSet->count(ownNr)>0) notariesSet->erase(ownNr);
    if (notariesSet->empty())
    {
        delete notariesSet;
        return;
    }
    set<unsigned long>::iterator it;
    for (it = notariesSet->begin(); it != notariesSet->end(); ++it)
    {
        TNtrNr totalNotaryNr(lineageNr, *it);
        unsigned long long activeUntil = type1entry->getNotaryTenureEndCorrected(lineageNr, *it);
        ContactInfo* contactInfo = getContactInfo(totalNotaryNr);
        if (contactInfo == nullptr) servers->addContact(*it, "", 0, 0, activeUntil);
        else
        {
            int port = (int) ((long) contactInfo->getPort());
            servers->addContact(*it, contactInfo->getIP(), port, contactInfo->getValidSince(), activeUntil);
        }
        delete contactInfo;
    }
    notariesSet->clear();
    delete notariesSet;
}

// db must be locked for this
bool Database::storeT9eCreationParameters(CompleteID &threadId, unsigned long long &terminationTime)
{
    if (terminationTime < threadId.getTimeStamp()) return false;
    if (type1entry->getLineage(threadId.getTimeStamp()) != type1entry->latestLin())
    {
        string key;
        key="P"; // prefix for parameters
        key.append(threadId.to20Char());
        key.append("L"); // suffix for lineage
        scheduledActions->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(type1entry->latestLin()));
        key="P"; // prefix for parameters
        key.append(threadId.to20Char());
        key.append("T"); // suffix for termination time
        scheduledActions->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(terminationTime));
        key="P"; // prefix for parameters
        key.append(threadId.to20Char());
        key.append("H"); // suffix for number of notaries
        scheduledActions->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(1));
        key="P"; // prefix for parameters
        key.append(threadId.to20Char());
        key.append("B"); // suffix for the body of the notaries list
        key.append(util.UlAsByteSeq(0));
        scheduledActions->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(1));
        if (ownNumber == 1)
        {
            key="A"; // prefix for attempt
            key.append(util.flip(util.UllAsByteSeq(terminationTime)));
            key.append(threadId.to20Char());
            scheduledActions->Put(rocksdb::WriteOptions(), key, threadId.to20Char());
        }
        return true;
    }
    set<unsigned long>* actingNotaries = getFutureActingNotaries(terminationTime);
    list<unsigned long> signingNotaries;
    if (!loadSigningNotaries(threadId, signingNotaries))
    {
        delete actingNotaries;
        return false;
    }
    list<unsigned long> targetList;
    set<unsigned long> addedNotaries;
    // add signing notaries which are also acting
    list<unsigned long>::iterator it;
    for (it=signingNotaries.begin(); it!=signingNotaries.end(); ++it)
    {
        if (actingNotaries->count(*it)==1)
        {
            targetList.push_back(*it);
            addedNotaries.insert(*it);
        }
    }
    // add remaining acting notaries
    set<unsigned long>::iterator it2;
    for (it2=actingNotaries->begin(); it2!=actingNotaries->end(); ++it2)
    {
        if (addedNotaries.count(*it2)!=1)
        {
            targetList.push_back(*it2);
            addedNotaries.insert(*it2);
        }
    }
    delete actingNotaries;
    // store data
    string key;
    key="P"; // prefix for parameters
    key.append(threadId.to20Char());
    key.append("L"); // suffix for lineage
    scheduledActions->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(type1entry->latestLin()));
    key="P"; // prefix for parameters
    key.append(threadId.to20Char());
    key.append("T"); // suffix for termination time
    scheduledActions->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(terminationTime));
    key="P"; // prefix for parameters
    key.append(threadId.to20Char());
    key.append("H"); // suffix for number of notaries
    scheduledActions->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(targetList.size()));
    unsigned long c=0;
    list<unsigned long>::iterator it3;
    for (it3=targetList.begin(); it3!=targetList.end(); ++it3)
    {
        key="P"; // prefix for parameters
        key.append(threadId.to20Char());
        key.append("B"); // suffix for the body of the notaries list
        key.append(util.UlAsByteSeq(c));
        scheduledActions->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(*it3));
        c++;
    }
    // get own number in list
    unsigned long p=0;
    for (it3=targetList.begin(); it3!=targetList.end(); ++it3)
    {
        if (*it3==ownNumber) break;
        p++;
    }
    // schedule own attempt
    if (p<targetList.size())
    {
        unsigned long long ownTime = terminationTime + type1entry->getLatestMaxNotarizationTime() * p;
        key="A"; // prefix for attempt
        key.append(util.flip(util.UllAsByteSeq(ownTime)));
        key.append(threadId.to20Char());
        scheduledActions->Put(rocksdb::WriteOptions(), key, threadId.to20Char());
    }
    return true;
}

// db must be locked for this
unsigned long Database::getNotariesListLength(CompleteID &entryId, bool renot)
{
    string key;
    string value;
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("H"); // suffix for number of notaries
    rocksdb::Status s;
    if (!renot) s = scheduledActions->Get(rocksdb::ReadOptions(), key, &value);
    else s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUl(value);
}

// db must be locked for this
unsigned long long Database::getTerminationTime(CompleteID &threadId)
{
    string key;
    string value;
    key="P"; // prefix for parameters
    key.append(threadId.to20Char());
    key.append("T"); // suffix for termination time
    rocksdb::Status s = scheduledActions->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUll(value);
}

// db must be locked for this
unsigned short Database::getScheduledLineage(CompleteID &entryId, bool renot)
{
    string key;
    string value;
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("L"); // suffix for lineage
    rocksdb::Status s;
    if (!renot) s = scheduledActions->Get(rocksdb::ReadOptions(), key, &value);
    else s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>1);
    if (!success) return 0;
    return util.byteSeqAsUs(value);
}

// db must be locked for this
unsigned long Database::getNotaryInSchedule(CompleteID &entryId, unsigned long c, bool renot)
{
    string key;
    string value;
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("B"); // suffix for the body of the notaries list
    key.append(util.UlAsByteSeq(c));
    rocksdb::Status s;
    if (!renot) s = scheduledActions->Get(rocksdb::ReadOptions(), key, &value);
    else s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUl(value);
}

// db must be locked for this
void Database::deleteT9eCreationParameters(CompleteID &threadId)
{
    unsigned long listLength = getNotariesListLength(threadId, false);
    list<string> toDeleteList;
    string key;
    key="P"; // prefix for parameters
    key.append(threadId.to20Char());
    key.append("L"); // suffix for lineage
    toDeleteList.push_back(key);
    key="P"; // prefix for parameters
    key.append(threadId.to20Char());
    key.append("T"); // suffix for termination time
    toDeleteList.push_back(key);
    key="P"; // prefix for parameters
    key.append(threadId.to20Char());
    key.append("H"); // suffix for number of notaries
    toDeleteList.push_back(key);
    for (unsigned long c=0; c<listLength; c++)
    {
        key="P"; // prefix for parameters
        key.append(threadId.to20Char());
        key.append("B"); // suffix for the body of the notaries list
        key.append(util.UlAsByteSeq(c));
        toDeleteList.push_back(key);
    }
    // clean up
    list<string>::iterator it;
    for (it=toDeleteList.begin(); it!=toDeleteList.end(); ++it)
        scheduledActions->Delete(rocksdb::WriteOptions(), *it);
}

// db must be locked for this
void Database::verifySchedules(CompleteID &entryID)
{
    // for thread terminations
    unsigned short lineage = getScheduledLineage(entryID, false);
    if (lineage > 0 && lineage != type1entry->latestLin())
    {
        unsigned long long terminationTime = getTerminationTime(entryID);
        deleteT9eCreationParameters(entryID);
        storeT9eCreationParameters(entryID, terminationTime);
    }
    // for renotarizations
    lineage = getScheduledLineage(entryID, true);
    if (lineage > 0 && lineage != type1entry->latestLin())
    {
        deleteRenotarizationParameters(entryID);
        storeRenotarizationParameters(entryID);
    }
}

// db must be locked for this
void Database::checkThreadTerminations()
{
    string keyPref;
    keyPref.push_back('E'); // prefix for expected terminations
    size_t prefLength=keyPref.length();
    rocksdb::Iterator* it = scheduledActions->NewIterator(rocksdb::ReadOptions());
    list<string> toDeleteList;
    unsigned long long currentTime = systemTimeInMs();
    for (it->Seek(keyPref); it->Valid(); it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0)
        {
            break;
        }
        if (keyStr.length()!=prefLength+28)
        {
            toDeleteList.push_back(keyStr);
            continue;
        }
        // get termination time and threadId
        string timeStr = util.flip(keyStr.substr(prefLength, 8));
        string idStr =  keyStr.substr(prefLength+8, 20);
        unsigned long long terminationTime = util.byteSeqAsUll(timeStr);
        CompleteID threadId(idStr);
        if (terminationTime>currentTime) break;
        else
        {
            CompleteID zeroId;
            if (!hasThreadSuccessor(threadId, zeroId)) storeT9eCreationParameters(threadId, terminationTime);
            toDeleteList.push_back(keyStr);
        }
    }
    delete it;
    // clean up
    list<string>::iterator it2;
    for (it2=toDeleteList.begin(); it2!=toDeleteList.end(); ++it2)
    {
        scheduledActions->Delete(rocksdb::WriteOptions(), *it2);
    }
    return;
}

// db must be locked for this
bool Database::loadNextTerminatingThread(CompleteID &lastEntryId)
{
    string keyPref;
    keyPref.push_back('A'); // prefix for termination attempts
    size_t prefLength=keyPref.length();
    rocksdb::Iterator* it = scheduledActions->NewIterator(rocksdb::ReadOptions());
    list<string> toDeleteList;
    list<string> toReparametrizeList;
    CompleteID outId;
    unsigned long long attemptTime=0;
    unsigned long long currentTime = systemTimeInMs();
    for (it->Seek(keyPref); it->Valid(); it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0)
        {
            break;
        }
        if (keyStr.length()!=prefLength+28)
        {
            toDeleteList.push_back(keyStr);
            continue;
        }
        // get termination time and threadId
        string timeStr = util.flip(keyStr.substr(prefLength, 8));
        attemptTime = util.byteSeqAsUll(timeStr);
        string idStr = keyStr.substr(prefLength+8, 20);
        CompleteID threadId(idStr);
        // check if successor exists
        CompleteID zeroId;
        if (hasThreadSuccessor(threadId, zeroId))
        {
            toDeleteList.push_back(keyStr);
            deleteT9eCreationParameters(threadId);
            continue;
        }
        // check lineage
        unsigned short lineage = getScheduledLineage(threadId, false);
        if (lineage != type1entry->latestLin())
        {
            toReparametrizeList.push_back(keyStr);
            continue;
        }
        // check if not too early
        if (currentTime<attemptTime) break;
        // break and report
        outId = threadId;
        toDeleteList.push_back(keyStr);
        break;
    }
    delete it;
    // clean up
    list<string>::iterator it2;
    for (it2=toDeleteList.begin(); it2!=toDeleteList.end(); ++it2)
    {
        scheduledActions->Delete(rocksdb::WriteOptions(), *it2);
    }
    for (it2=toReparametrizeList.begin(); it2!=toReparametrizeList.end(); ++it2)
    {
        string keyStr = *it2;
        scheduledActions->Delete(rocksdb::WriteOptions(), keyStr);
        string idStr = keyStr.substr(prefLength+8, 20);
        CompleteID threadId(idStr);
        unsigned long long terminationTime = getTerminationTime(threadId);
        deleteT9eCreationParameters(threadId);
        storeT9eCreationParameters(threadId, terminationTime);
    }
    // if nothing found
    if (outId.getNotary()<=0) return false;
    // reschedule outId
    unsigned long numOfNotaries = getNotariesListLength(outId, false);
    unsigned long long step = type1entry->getLatestMaxNotarizationTime() * numOfNotaries;
    unsigned long long factor = max((currentTime - attemptTime) / step, (unsigned long long) 1);
    unsigned long long ownTime = attemptTime + step * factor;
    if (ownTime <= currentTime) ownTime += step;
    string key("A"); // prefix for attempt
    key.append(util.flip(util.UllAsByteSeq(ownTime)));
    key.append(outId.to20Char());
    scheduledActions->Put(rocksdb::WriteOptions(), key, outId.to20Char());
    lastEntryId = getLatestID(outId);
    return true;
}

// db must be locked for this
bool Database::eligibleForRenotarization(CompleteID &entryId)
{
    CompleteID firstID = getFirstID(entryId);
    if (firstID.getNotary()<=0) return false;
    // check if this is the latest id
    CompleteID latestID = getLatestID(firstID);
    if (latestID != entryId)
    {
        return false;
    }
    unsigned long long entryTime = latestID.getTimeStamp();
    // content based checks:
    string entryStr;
    if (!loadInitialT13EntryStr(firstID, entryStr)) return false;
    Type12Entry *type12entry = createT12FromT13Str(entryStr);
    if (type12entry == nullptr) return false;
    int type = type12entry->underlyingType();
    CompleteID zeroId;
    if (type == 2)
    {
        delete type12entry;
        if (firstID != getLastEssentialEntryId(zeroId)) return false;
        return true;
    }
    else if (type == 3 || type == 4 || type == 6 || type == 7 || type == 8)
    {
        delete type12entry;
        if (hasThreadSuccessor(firstID, zeroId)) return false;
        return true;
    }
    else if (type == 9)
    {
        delete type12entry;
        CompleteID applicationID = getApplicationId(firstID);
        CompleteID applicantId = getThreadParticipantId(applicationID);
        if (getRefereeTenureEnd(applicantId, firstID) > entryTime) return true;
        // based on possible liquidity claims:
        unsigned short lineage = type1entry->getLineage(firstID.getTimeStamp());
        if (lineage == 0) return false;
        unsigned long long relevantUntil = (unsigned long long) type1entry->getNotaryTenure(lineage);
        relevantUntil *= 1000;
        relevantUntil += firstID.getTimeStamp();
        if (relevantUntil < entryTime) return false;
        return true;
    }
    else if (type == 10)
    {
        // do no renotarize if liquidity already collected
        if (getTransferCollectingClaim(firstID, zeroId).getNotary()>0)
        {
            delete type12entry;
            return false;
        }
        // check if just a satisfied request offer
        Type10Entry *t10e = (Type10Entry*) type12entry->underlyingEntry();
        if (t10e->getTransferAmount()==0 && getConnectedTransfer(firstID, zeroId)!=firstID)
        {
            delete type12entry;
            return false;
        }
        // renotarize otherwise
        delete type12entry;
        return true;
    }
    else if (type == 14)
    {
        CompleteID pubKeyId = type12entry->pubKeyID();
        CompleteID pubKeyIdFirst = getFirstID(pubKeyId);
        Type14Entry *type14entry = (Type14Entry*) type12entry->underlyingEntry();
        CompleteID currencyOrOblId = type14entry->getCurrencyOrObligation();
        CompleteID currencyOrOblIdFirst = getFirstID(currencyOrOblId);
        unsigned long long claimCount = getClaimCount(pubKeyIdFirst, currencyOrOblIdFirst);
        if (claimCount <= 0 || firstID != getClaim(pubKeyIdFirst, currencyOrOblIdFirst, claimCount-1, zeroId))
        {
            delete type12entry;
            return false;
        }
        delete type12entry;
        return true;
    }
    delete type12entry;
    return false;
}

// db must be locked for this
void Database::updateRenotarizationAttempts()
{
    // set prefix
    string keyPref("P");
    size_t prefLength=keyPref.length();
    // build minKey
    string maxKey(keyPref);
    unsigned short lineage = type1entry->latestLin();
    unsigned long long maxTime = type1entry->getFreshnessTime(lineage);
    maxTime *= 650;
    maxTime = systemTimeInMs() - maxTime;
    CompleteID maxId(0, 0, maxTime);
    maxKey.append(maxId.to20Char());
    // iterate
    rocksdb::Iterator* it = subjectToRenotarization->NewIterator(rocksdb::ReadOptions());
    list<string> toDeleteList;
    list<string> toReparametrizeList;
    string oldIdStr("");
    for (it->SeekForPrev(maxKey); it->Valid(); it->Prev())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length() <= prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // get id
        string idStr = keyStr.substr(prefLength, 20);
        if (idStr.compare(oldIdStr) == 0) continue;
        oldIdStr = idStr;
        CompleteID id(idStr);
        if (id >= maxId) continue;
        // check eligibility
        if (!eligibleForRenotarization(id))
        {
            toDeleteList.push_back(idStr);
        }
        else
        {
            toReparametrizeList.push_back(idStr);
        }
    }
    delete it;
    // clean up
    list<string>::iterator it2;
    for (it2=toDeleteList.begin(); it2!=toDeleteList.end(); ++it2)
    {
        CompleteID id(*it2);
        deleteRenotarizationParameters(id);
    }
    for (it2=toReparametrizeList.begin(); it2!=toReparametrizeList.end(); ++it2)
    {
        CompleteID id(*it2);
        deleteRenotarizationParameters(id);
        storeRenotarizationParameters(id);
    }
    // report
    if (toDeleteList.size()>0) puts("Database::updateRenotarizationAttempts: toDeleteList not empty");
    if (toReparametrizeList.size()>0) puts("Database::updateRenotarizationAttempts: toReparametrizeList not empty");
}

// db must be locked for this
bool Database::storeRenotarizationParameters(CompleteID &entryId)
{
    // check if renotarization already scheduled
    if (getEarliestRenotTime(entryId) != ULLONG_MAX) return false;
    // check if entry is subject to renotarization at all
    if (!eligibleForRenotarization(entryId)) return false;
    // check lineage
    unsigned short lineage = type1entry->getLineage(entryId.getTimeStamp());
    if (lineage != type1entry->latestLin()) // only first notary can renotarize
    {
        string key;
        key="P"; // prefix for parameters
        key.append(entryId.to20Char());
        key.append("L"); // suffix for lineage
        subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(type1entry->latestLin()));
        key="P"; // prefix for parameters
        key.append(entryId.to20Char());
        key.append("E"); // suffix for earliest time
        unsigned long long earliestTime = type1entry->getNotaryTenureStartCorrected(type1entry->latestLin(), 1);
        subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(earliestTime));
        key="P"; // prefix for parameters
        key.append(entryId.to20Char());
        key.append("H"); // suffix for number of notaries
        subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(1));
        key="P"; // prefix for parameters
        key.append(entryId.to20Char());
        key.append("B"); // suffix for the body of the notaries list
        key.append(util.UlAsByteSeq(0));
        subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(1));
        if (ownNumber == 1)
        {
            key="A"; // prefix for attempt
            key.append(util.flip(util.UllAsByteSeq(earliestTime)));
            key.append(entryId.to20Char());
            subjectToRenotarization->Put(rocksdb::WriteOptions(), key, entryId.to20Char());
        }
        return true;
    }
    // get earliest renotarization start time
    unsigned long long earliestTime = type1entry->getFreshnessTime(lineage);
    earliestTime *= 500;
    earliestTime += entryId.getTimeStamp();

    // get future acting notaries
    set<unsigned long>* actingNotaries = getFutureActingNotaries(earliestTime);
    // store data
    string key;
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("L"); // suffix for lineage
    subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(lineage));
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("E"); // suffix for earliest time
    subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(earliestTime));
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("H"); // suffix for number of notaries
    subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(actingNotaries->size()));
    unsigned long c=0;
    set<unsigned long>::iterator it;
    for (it=actingNotaries->begin(); it!=actingNotaries->end(); ++it)
    {
        key="P"; // prefix for parameters
        key.append(entryId.to20Char());
        key.append("B"); // suffix for the body of the notaries list
        key.append(util.UlAsByteSeq(c));
        subjectToRenotarization->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(*it));
        c++;
    }
    // get own number in list
    unsigned long p=0;
    for (it=actingNotaries->begin(); it!=actingNotaries->end(); ++it)
    {
        if (*it==ownNumber) break;
        p++;
    }
    // schedule own attempt
    if (p < actingNotaries->size())
    {
        unsigned long long ownTime = earliestTime + type1entry->getLatestMaxNotarizationTime() * p;
        key="A"; // prefix for attempt
        key.append(util.flip(util.UllAsByteSeq(ownTime)));
        key.append(entryId.to20Char());
        subjectToRenotarization->Put(rocksdb::WriteOptions(), key, entryId.to20Char());
    }
    delete actingNotaries;
    return true;
}

// db must be locked for this
void Database::deleteRenotarizationParameters(CompleteID &entryId)
{
    unsigned long listLength = getNotariesListLength(entryId, true);
    list<string> toDeleteList;
    string key;
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("L"); // suffix for lineage
    toDeleteList.push_back(key);
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("E"); // suffix for earliest time
    toDeleteList.push_back(key);
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("H"); // suffix for number of notaries
    toDeleteList.push_back(key);
    for (unsigned long c=0; c<listLength; c++)
    {
        key="P"; // prefix for parameters
        key.append(entryId.to20Char());
        key.append("B"); // suffix for the body of the notaries list
        key.append(util.UlAsByteSeq(c));
        toDeleteList.push_back(key);
    }
    // clean up
    list<string>::iterator it;
    for (it=toDeleteList.begin(); it!=toDeleteList.end(); ++it)
        subjectToRenotarization->Delete(rocksdb::WriteOptions(), *it);
}

// db must be locked for this
unsigned long long Database::getEarliestRenotTime(CompleteID &entryId)
{
    string key;
    string value;
    key="P"; // prefix for parameters
    key.append(entryId.to20Char());
    key.append("E"); // suffix for earliest time
    rocksdb::Status s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return ULLONG_MAX;
    return util.byteSeqAsUll(value);
}

// db must be locked for this
int Database::loadNextEntryToRenotarize(CompleteID &entryId)
{
    string keyPref("A");
    size_t prefLength=keyPref.length();
    rocksdb::Iterator* it = subjectToRenotarization->NewIterator(rocksdb::ReadOptions());
    list<string> toDeleteList;
    list<string> toReparametrizeList;
    CompleteID outId;
    unsigned long long attemptTime=0;
    unsigned long long currentTime = systemTimeInMs();
    for (it->Seek(keyPref); it->Valid(); it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0)
        {
            break;
        }
        if (keyStr.length()!=prefLength+28)
        {
            toDeleteList.push_back(keyStr);
            continue;
        }
        // get latest time and entryId
        string timeStr = util.flip(keyStr.substr(prefLength, 8));
        attemptTime = util.byteSeqAsUll(timeStr);
        string idStr = keyStr.substr(prefLength+8, 20);
        CompleteID id(idStr);
        // check eligibility
        if (!eligibleForRenotarization(id))
        {
            toDeleteList.push_back(keyStr);
            continue;
        }
        // check lineage
        if (getScheduledLineage(id, true) != type1entry->latestLin())
        {
            toReparametrizeList.push_back(keyStr);
            continue;
        }
        // check if not too early
        if (currentTime<attemptTime) break;
        // break and report
        outId = id;
        subjectToRenotarization->Delete(rocksdb::WriteOptions(), keyStr);
        break;
    }
    delete it;
    // clean up
    list<string>::iterator it2;
    for (it2=toDeleteList.begin(); it2!=toDeleteList.end(); ++it2)
    {
        subjectToRenotarization->Delete(rocksdb::WriteOptions(), *it2);
        string idStr = (*it2).substr(prefLength+8, 20);
        CompleteID id(idStr);
        deleteRenotarizationParameters(id);
    }
    for (it2=toReparametrizeList.begin(); it2!=toReparametrizeList.end(); ++it2)
    {
        subjectToRenotarization->Delete(rocksdb::WriteOptions(), *it2);
        string idStr = (*it2).substr(prefLength+8, 20);
        CompleteID id(idStr);
        deleteRenotarizationParameters(id);
        storeRenotarizationParameters(id);
    }
    // if nothing found
    if (outId.getNotary()<=0) return -1;
    unsigned long long earliestTime = getEarliestRenotTime(outId);
    if (earliestTime == ULLONG_MAX) return -1;
    entryId = outId;
    // reschedule entryId:
    unsigned long numOfNotaries = getNotariesListLength(entryId, true);
    unsigned long long step = type1entry->getLatestMaxNotarizationTime() * numOfNotaries;
    unsigned long long factor = max((currentTime - attemptTime) / step, (unsigned long long) 1);
    unsigned long long ownTime = attemptTime + step * factor;
    if (ownTime <= currentTime) ownTime += step;
    string key("A"); // prefix for attempt
    key.append(util.flip(util.UllAsByteSeq(ownTime)));
    key.append(entryId.to20Char());
    subjectToRenotarization->Put(rocksdb::WriteOptions(), key, entryId.to20Char());
    // return
    if (currentTime > attemptTime + type1entry->getLatestMaxNotarizationTime()) return 0;
    return 1;
}

// db must be locked for this
CompleteID Database::getFirstID(CompleteID &id)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(id.to20Char());
    key.append("FN"); // suffix for id of first notarization entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID firstNotID(value);
    return firstNotID;
}

// db must be locked for this
CompleteID Database::getLatestID(CompleteID &firstID)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(firstID.to20Char());
    key.append("LN"); // suffix for id of latest known notarization entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID latestNotId(value);
    return latestNotId;
}

// db must be locked for this
CompleteID Database::getFirstID(string* pubKey)
{
    string key;
    string value;
    key.push_back('B'); // suffix for key identification by byte sequence
    key.append(util.UlAsByteSeq(pubKey->length()));
    key.append(*pubKey);
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID id(value);
    return id;
}

// db must be locked for this
bool Database::loadInitialT13EntryStr(CompleteID &idFirst, string &str)
{
    // obtain the underlying entry from the general list of
    // notarization entries using this complete id
    string key;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(idFirst.to20Char());
    key.append("SL");  // suffix for signatures list (type13entries)
    key.push_back('B'); // suffix for body of signatures list
    key.append(util.UlAsByteSeq(0)); // suffix for first type 13 entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &str);
    return (s.ok() && str.length()>2);
}

// db must be locked for this
bool Database::loadPubKey(CompleteID &idFirst, string &str)
{
    string key;
    key.push_back('I'); // suffix for key identification by id
    key.append(idFirst.to20Char());
    key.append("PK"); // suffix for public key
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &str);
    return (s.ok() && str.length()>2);
}

bool Database::isFreshNow(CompleteID &id)
{
    bool result;
    lock();
    result = type1entry->isFresh(id, systemTimeInMs()) || freshByDefi(id);
    unlock();
    return result;
}

// db must be locked for this
bool Database::freshByDefi(CompleteID &id)
{
    return isInGeneralList(id)
           && (getOblOwner(id).getNotary()>0        // if type 15 entry
               || getRefereeTenure(id)>0            // if type 5 entry
               || getValidityDate(id)>0);           // if type 11 entry
}

// db must be locked for this
TNtrNr Database::getTotalNotaryNr(string* pubKey)
{
    // get from list of notaries (not public Keys)
    string key;
    string value;
    key.push_back('K'); // prefix for identification by public key
    key.append(util.UlAsByteSeq(pubKey->length()));
    key.append(*pubKey);
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return TNtrNr(0,0);
    TNtrNr out(value);
    return out;
}

// db must be locked for this
TNtrNr Database::getTotalNotaryNr(CompleteID &pubKeyId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyId.to20Char());
    key.append("TN"); // suffix for total notary number
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return TNtrNr(0,0);
    TNtrNr out(value);
    return out;
}

// db must be locked for this
bool Database::loadNotaryPubKey(TNtrNr &totalNotaryNr, string &str)
{
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(totalNotaryNr.toString());
    key.append("PK"); // prefix for public key
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return false;
    str.append(value);
    return true;
}

// db must be locked for this
CompleteID Database::getLatestNotaryId(TNtrNr &totalNotaryNr)
{
    CompleteID out = getNotaryId(totalNotaryNr);
    return getLatestID(out);
}

// db must be locked for this
CompleteID Database::getNotaryId(TNtrNr &totalNotaryNr)
{
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(totalNotaryNr.toString());
    key.append("ID"); // prefix for id
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID id(value);
    return id;
}

// db must be locked for this
bool Database::recentlyApprovedNotary(CompleteID &pubKeyId, CompleteID &terminationID, unsigned long long currentTime)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyId.to20Char());
    key.append("NA"); // suffix for notary appointment to be finalized
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return false;
    CompleteID id(value);
    if (terminationID != id) return false;
    unsigned short lineage = type1entry->getLineage(currentTime);
    if (lineage == 0) return false;
    unsigned long long recentUntil = (unsigned long long) type1entry->getNotaryTenure(lineage);
    recentUntil *= 1000;
    recentUntil += id.getTimeStamp();
    return (currentTime <= recentUntil);
}

// db must be locked for this
Type12Entry* Database::buildUnderlyingEntry(CompleteID &firstId, unsigned char l)
{
    string str;
    if (l==0)
    {
        if (!loadInitialT13EntryStr(firstId, str)) return nullptr;
    }
    else if (l==1)
    {
        if (!loadType13EntryStr(firstId, l, str)) return nullptr;
    }
    else
    {
        CompleteID notEntryId = getEntryInRenotarization(firstId);
        CompleteID notEntryIdFirst = getFirstID(notEntryId);
        if (!loadInitialT13EntryStr(notEntryIdFirst, str)) return nullptr;
    }
    return createT12FromT13Str(str);
}

// db must be locked for this
unsigned short Database::getEntryType(CompleteID &firstID)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(firstID.to20Char());
    key.append("UET"); // suffix for type of underlying entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>1);
    if (!success) return 0;
    return util.byteSeqAsUs(value);
}

// db must be locked for this
Type5Entry* Database::getTruncatedEntry(CompleteID &currencyIdFirst)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(currencyIdFirst.to20Char());
    key.append("TB"); // suffix for truncated byte seq
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return nullptr;
    Type5Entry *t5e = new Type5Entry(value);
    if (!t5e->isGood())
    {
        delete t5e;
        return nullptr;
    }
    return t5e;
}

// db must be locked for this
unsigned long Database::getRefereeTenure(CompleteID &currencyIdFirst)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(currencyIdFirst.to20Char());
    key.append("RT"); // suffix for referee tenure
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUl(value);
}

Type12Entry* Database::createT12FromT13Str(string &str)
{
    const unsigned long long l = str.length();
    if (l < 77 || str.at(0) != 0x2C) return nullptr;
    if (str.substr(1, 20).compare(str.substr(21, 20)) != 0) return nullptr;
    if (str.substr(21, 20).compare(str.substr(41, 20)) != 0) return nullptr;
    string t12eStrWithSigLenStr = str.substr(61, 8);
    string t12eStrLenStr = str.substr(69, 8);
    Util u;
    unsigned long long t12eStrWithSigLen = u.byteSeqAsUll(t12eStrWithSigLenStr);
    unsigned long long t12eStrLen = u.byteSeqAsUll(t12eStrLenStr);
    if (t12eStrWithSigLen <= t12eStrLen+8 || l != t12eStrWithSigLen+69) return nullptr;
    string t12eStr(str.substr(77, t12eStrLen));
    Type12Entry *type12entry = new Type12Entry(t12eStr);
    if (!type12entry->isGood())
    {
        delete type12entry;
        return nullptr;
    }
    return type12entry;
}

// db must be locked for this
CompleteID Database::getRefereeCurrency(CompleteID &pubKeyId, CompleteID &terminationID)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyId.to20Char());
    key.push_back('R'); // suffix for referee information
    key.push_back('T'); // suffix for termination entry
    key.append(terminationID.to20Char());
    key.push_back('C'); // suffix for respective currency
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    return CompleteID(value);
}

// db must be locked for this
unsigned long long Database::getRefereeTenureEnd(CompleteID &pubKeyId, CompleteID &terminationID)
{
    unsigned long long tenureStart = getRefereeTenureStart(pubKeyId, terminationID);
    if (tenureStart == ULLONG_MAX) return 0;
    CompleteID currencyID = getRefereeCurrency(pubKeyId, terminationID);
    unsigned long duration = getRefereeTenure(currencyID);
    unsigned long long endOfTenure = (unsigned long long) duration;
    endOfTenure *= 1000;
    endOfTenure += tenureStart;
    return endOfTenure;
}

// db must be locked for this
unsigned long long Database::getRefereeTenureStart(CompleteID &pubKeyId, CompleteID &terminationID)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyId.to20Char());
    key.push_back('R'); // suffix for referee information
    key.push_back('T'); // suffix for termination entry
    key.append(terminationID.to20Char());
    key.push_back('T'); // suffix for tenure start
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return ULLONG_MAX;
    unsigned long long tenureStart = util.byteSeqAsUll(value);
    return tenureStart;
}

// db must be locked for this
bool Database::isActingReferee(CompleteID &pubKeyId, CompleteID &terminationID, unsigned long long currentTime)
{
    unsigned long long tenureStart = getRefereeTenureStart(pubKeyId, terminationID);
    CompleteID currencyID = getRefereeCurrency(pubKeyId, terminationID);
    unsigned long duration = getRefereeTenure(currencyID);
    unsigned long long endOfTenure = (unsigned long long) duration;
    endOfTenure *= 1000;
    endOfTenure += tenureStart;
    return (tenureStart<=currentTime && currentTime<endOfTenure);
}

// db must be locked for this
unsigned long Database::getNrOfNotariesInLineage(unsigned short lineageNr)
{
    string key;
    string value;
    key.push_back('H'); // prefix for list head (nr of entries)
    key.append(util.UsAsByteSeq(lineageNr));
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUl(value);
}

// db must be locked for this
bool Database::possibleToAppointNewNotaries(unsigned long long currentTime)
{
    unsigned short lineageNr = type1entry->getLineage(currentTime);
    if (lineageNr == 0) return false;
    unsigned long N = type1entry->getMaxActing(lineageNr);
    unsigned long long linStart = type1entry->getNotaryTenureStartFormal(lineageNr,1);
    unsigned long tenure = type1entry->getNotaryTenure(lineageNr);
    double delta = ((double) tenure) / N * 1000;
    unsigned long nrOfTenureStartsAfterFirst = (unsigned long)((double)(currentTime - linStart) / delta);
    unsigned long nrOfNotariesInLineage = getNrOfNotariesInLineage(lineageNr);
    if (nrOfNotariesInLineage < nrOfTenureStartsAfterFirst) return true;
    else
    {
        unsigned long waitingNotaries = nrOfNotariesInLineage - nrOfTenureStartsAfterFirst;
        return (waitingNotaries < type1entry->getMaxWaiting(lineageNr));
    }
}

// db must be locked for this
unsigned long long Database::getValidityDate(CompleteID &pubKeyId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyId.to20Char());
    key.append("VD"); // suffix for validity date
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUll(value);
}

// db must be locked for this
CompleteID Database::getValidityDateEntry(CompleteID &pubKeyId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyId.to20Char());
    key.append("VE"); // suffix for validity date
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID out(value);
    return out;
}

// db must be locked for this
int Database::verifyConflict(CompleteID &prelimEntryID, CompleteID &currentRefId)
{
    if (currentRefId.isZero() || currentRefId==prelimEntryID) return 0; // unclear status, but is ignored for now
    bool renot = false;
    unsigned long long timeLimit = getNotTimeLimit(prelimEntryID, renot);
    if (timeLimit<=0)
    {
        renot = true;
        timeLimit = getNotTimeLimit(prelimEntryID, renot);
        if (timeLimit<=0)
        {
            puts("Database::verifyConflict: old entry is already deleted...");
            return -1; // was deleted
        }
    }
    if (timeLimit <= systemTimeInMs())
    {
        puts("Database::verifyConflict: old entry cannot be notarized and integrated, deleting...");
        deleteSignatures(prelimEntryID, renot);
        return -1; // cannot be notarized, so it was deleted
    }
    puts("Database::verifyConflict: time limit not yet reached...");
    puts(to_string(timeLimit).c_str());

    conflictingEntries.add(prelimEntryID);
    return 0;
}

// db must be locked for this
bool Database::isInGeneralList(CompleteID &entryID)
{
    if (entryID.getNotary() <= 0) return false;
    return (getFirstID(entryID).getNotary() > 0);
}

// db must be locked for this
bool Database::hasTakenPartInThread(CompleteID &applicationId, CompleteID &pubKeyId, CompleteID &currentRefId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("TP"); // suffix for thread participants list. Output: ID of respective thread entry
    key.append(pubKeyId.to20Char());
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return false;
    CompleteID threadEntryID(value);
    if (isInGeneralList(threadEntryID)) return true;
    int notStatus = verifyConflict(threadEntryID, currentRefId);
    if (notStatus == -1) notarizationEntries->Delete(rocksdb::WriteOptions(), key);
    return false;
}

// db must be locked for this
CompleteID Database::getApplicationId(CompleteID &threadEntryId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(threadEntryId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('F'); // suffix for application entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID applicationID(value);
    return applicationID;
}

// db must be locked for this
CompleteID Database::getThreadParticipantId(CompleteID &threadEntryId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(threadEntryId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('P'); // suffix for participant id
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID keyID(value);
    return keyID;
}

// db must be locked for this
CompleteID Database::getLiquidityClaimId(CompleteID &threadEntryId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(threadEntryId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('C'); // suffix for claim id
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID claimId(value);
    return claimId;

}

// db must be locked for this
CompleteID Database::getRefTerminationEntryId(CompleteID &threadEntryId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(threadEntryId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('T'); // suffix for termination entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID id(value);
    return id;
}

// db must be locked for this
double Database::getNonTransferPartInStake(CompleteID &threadEntryId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(threadEntryId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('N'); // suffix for non transfer part
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
CompleteID Database::getFirstNoteId(CompleteID &threadEntryId)
{
    CompleteID zeroId;
    CompleteID id = getApplicationId(threadEntryId);
    if (id.getNotary()<=0) return zeroId;
    id = getThreadSuccessor(id, zeroId);
    return id;
}

// db must be locked for this
CompleteID Database::getThreadSuccessor(CompleteID &threadEntryId, CompleteID &currentRefId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(threadEntryId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.append("SE");  // suffix for successor entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID successorID(value);
    if (isInGeneralList(successorID)) return successorID;
    int notStatus = verifyConflict(successorID, currentRefId);
    if (notStatus == -1) notarizationEntries->Delete(rocksdb::WriteOptions(), key);
    return CompleteID();
}

// db must be locked for this
bool Database::hasThreadSuccessor(CompleteID &threadEntryId, CompleteID &currentRefId)
{
    return getThreadSuccessor(threadEntryId, currentRefId).getNotary()>0;
}

// db must be locked for this
CompleteID Database::getOutflowEntry(CompleteID &claimEntryId, CompleteID &currentRefId)
// returns claimEntryId if no outflow yet, returns zero if not even claim entry
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(claimEntryId.to20Char());
    key.push_back('C'); // suffix for claim entry
    key.append("OE"); // suffix for outflow entry
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID transferId(value);
    if (isInGeneralList(transferId)) return transferId;
    int notStatus = verifyConflict(transferId, currentRefId);
    if (notStatus == -1)
    {
        notarizationEntries->Put(rocksdb::WriteOptions(), key, claimEntryId.to20Char());
    }
    return claimEntryId;
}

// db must be locked for this
CompleteID Database::getConnectedTransfer(CompleteID &transferId, CompleteID &currentRefId)
// returns transferId if no connected transfer yet, returns zero if not even transfer entry
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(transferId.to20Char());
    key.append("LT"); // suffix for liquidity transfer entry information
    key.append("CT"); // suffix for the connected transfer
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID otherTransferId(value);
    if (isInGeneralList(otherTransferId)) return otherTransferId;
    int notStatus = verifyConflict(otherTransferId, currentRefId);
    if (notStatus == -1)
    {
        notarizationEntries->Put(rocksdb::WriteOptions(), key, transferId.to20Char());
    }
    return transferId;
}

// db must be locked for this
CompleteID Database::getTransferCollectingClaim(CompleteID &transferId, CompleteID &currentRefId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(transferId.to20Char());
    key.append("LT"); // suffix for liquidity transfer entry information
    key.append("UC"); // suffix for using claim
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID claimId(value);
    if (isInGeneralList(claimId)) return claimId;
    int notStatus = verifyConflict(claimId, currentRefId);
    if (notStatus == -1) notarizationEntries->Delete(rocksdb::WriteOptions(), key);
    return CompleteID();
}

// db must be locked for this
bool Database::isFreeClaim(CompleteID &claimEntryId, CompleteID &pubKeyID, CompleteID &liquiId, CompleteID &currentRefId)
{
    if (claimEntryId.getNotary()<=0) return false;
    // return if outflow exists
    if (getOutflowEntry(claimEntryId, currentRefId) != claimEntryId) return false;
    // check if is last claim entry in sequence
    unsigned long long claimCount = getClaimCount(pubKeyID, liquiId);
    if (claimCount <= 0) return false;
    if (getClaim(pubKeyID, liquiId, claimCount-1, currentRefId) != claimEntryId) return false;
    // verify that there is no later claim (e.g. in notarization)
    if (getClaim(pubKeyID, liquiId, claimCount, currentRefId).getNotary()>0)
    {
        puts("Database::isFreeClaim: laterClaim exists");
        return false;
        // the above should not trigger, but checking is important for conflicts
    }
    return true;
}

// db must be locked for this
double Database::getRefereeStake(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("RS"); // suffix for referee stake
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return DBL_MAX;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
unsigned char Database::getThreadStatus(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("TS"); // suffix for thread status
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>0);
    if (!success) return 255;
    return util.byteSeqAsUc(value);
}

// db must be locked for this
unsigned long Database::getProcessingTime(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("PT"); // suffix for processing time
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUl(value);
}

// db must be locked for this
CompleteID Database::getTerminationId(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("TI"); // suffix for termination entry id
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID id (value);
    return id;
}

// db must be locked for this
CompleteID Database::getCurrencyId(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("CI"); // suffix for currency id
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID id (value);
    return id;
}

// db must be locked for this
double Database::getRequestedLiquidity(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("RL"); // suffix for requested liquidity
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
unsigned long long Database::getTenureStart(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("BT"); // suffix for begin of tenure
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUll(value);
}

// db must be locked for this
double Database::getForfeit(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("FL"); // suffix for forfeited liquidity
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
double Database::getProcessingFee(CompleteID &applicationId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(applicationId.to20Char());
    key.push_back('T'); // suffix for thread entry
    key.push_back('A'); // suffix for application entry information
    key.append("PF"); // suffix for processing fee
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
double Database::getLiquidityLimitPerRef(CompleteID &currencyID)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(currencyID.to20Char());
    key.append("LL"); // suffix for liquidity limit
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
double Database::getInitialLiquidity(CompleteID &currencyID)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(currencyID.to20Char());
    key.append("IL"); // suffix for initial liquidity
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
double Database::getParentShare(CompleteID &currencyID)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(currencyID.to20Char());
    key.append("PS"); // suffix for parent share
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
double Database::getLiquidityCreatedSoFar(CompleteID &pubKeyID, CompleteID &terminationId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyID.to20Char());
    key.push_back('R'); // suffix for referee information
    key.push_back('T'); // suffix for termination entry
    key.append(terminationId.to20Char());
    key.push_back('L'); // suffix for liquidity created
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
bool Database::latestApprovalInNotarization(CompleteID &pubKeyID, CompleteID &terminationId, CompleteID &currentRefId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyID.to20Char());
    key.push_back('R'); // suffix for referee information
    key.push_back('T'); // suffix for termination entry
    key.append(terminationId.to20Char());
    key.push_back('A'); // suffix for last approval (type 8 entry id)
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return false;
    CompleteID noteId(value);
    if (isInGeneralList(noteId))
    {
        publicKeys->Delete(rocksdb::WriteOptions(), key);
        return false;
    }
    int notStatus = verifyConflict(noteId, currentRefId);
    if (notStatus == -1)
    {
        publicKeys->Delete(rocksdb::WriteOptions(), key);
        return false;
    }
    if (noteId == currentRefId) return false;
    return true;
}

// db must be locked for this
CompleteID Database::getOblOwner(CompleteID &oblId)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(oblId.to20Char());
    key.append("OO"); // suffix for obligation owner
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID out(value);
    return out;
}

// db must be locked for this
double Database::getLowerTransferLimit(CompleteID &oblId)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(oblId.to20Char());
    key.append("MT"); // suffix minimal transfer
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
unsigned short Database::getAppointmentLimitPerRef(CompleteID &currencyID)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(currencyID.to20Char());
    key.append("RL"); // suffix for referee limit
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>1);
    if (!success) return 0;
    return util.byteSeqAsUs(value);
}

// db must be locked for this
unsigned short Database::getRefsAppointedSoFar(CompleteID &pubKeyID, CompleteID &terminationId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyID.to20Char());
    key.push_back('R'); // suffix for referee information
    key.push_back('T'); // suffix for termination entry
    key.append(terminationId.to20Char());
    key.push_back('R'); // suffix for referees appointed
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>1);
    if (!success) return 0;
    return util.byteSeqAsUs(value);
}

// db must be locked for this
Type14Entry* Database::buildNextClaim(CompleteID &pubKeyID, CompleteID &currencyId)
{
    pubKeyID = getFirstID(pubKeyID);
    CompleteID pubKeyIDLatest = getLatestID(pubKeyID);
    unsigned long long currentTime = systemTimeInMs();
    if (!type1entry->isFresh(pubKeyIDLatest, currentTime) && !freshByDefi(pubKeyIDLatest))
    {
        puts("Database::buildNextClaim: pubKeyIDLatest not fresh:");
        puts(pubKeyIDLatest.to27Char().c_str());
        return nullptr;
    }
    // vectors for scenarios and ids etc.
    vector<CompleteID> predecessorsF; // first ids
    vector<CompleteID> predecessorsL; // latest ids
    vector<unsigned char> scenarios;
    vector<double> impacts;
    double totalAmount=0;
    double nonTransferAmount=0;
    double discountRate = getDiscountRate(currencyId);
    // get id of latest claim and its outflow (if applicable)
    CompleteID zeroId;
    unsigned long long k = getClaimCount(pubKeyID, currencyId);
    if (k>0)
    {
        CompleteID lastClaimId = getClaim(pubKeyID, currencyId, k-1, zeroId);
        predecessorsF.push_back(lastClaimId);
        CompleteID lastClaimIdLatest = getLatestID(lastClaimId);
        predecessorsL.push_back(lastClaimIdLatest);

        if (!type1entry->isFresh(lastClaimIdLatest, currentTime) && !freshByDefi(lastClaimIdLatest))
        {
            puts("Database::buildNextClaim: lastClaimIdLatest not fresh:");
            puts(lastClaimIdLatest.to27Char().c_str());
            return nullptr;
        }

        scenarios.push_back(0);
        CompleteID outFlowEntryId = getOutflowEntry(lastClaimId, zeroId);
        if (outFlowEntryId.getNotary()<=0)
        {
            puts("Database::buildNextClaim: bad outFlowEntryId");
            return nullptr;
        }
        else if (outFlowEntryId != lastClaimId)
        {
            predecessorsF.push_back(outFlowEntryId);
            CompleteID outFlowEntryIdLatest = getLatestID(outFlowEntryId);
            predecessorsL.push_back(outFlowEntryIdLatest);
            // get outflow entry type
            unsigned short entryType = getEntryType(outFlowEntryId);
            // determine scenario
            if (entryType == 10) scenarios.push_back(2);
            else if (entryType == 7) scenarios.push_back(5);
            else if (entryType == 8) scenarios.push_back(6);
            else
            {
                puts("Database::buildNextClaim: unexpected outflow entryType");
                return nullptr;
            }
        }
    }
    // calculate impacts so far
    unsigned short n = predecessorsF.size();
    for (unsigned short i=0; i<n; i++)
    {
        double amount = getCollectableLiquidity(predecessorsF.at(i), pubKeyID, currencyId,
                                                discountRate, scenarios.at(i), zeroId, currentTime);
        double nonTransfer = getCollectableNonTransferLiqui(predecessorsF.at(i), pubKeyID, currencyId,
                             discountRate, scenarios.at(i), zeroId, currentTime);
        impacts.push_back(amount);
        totalAmount+=amount;
        nonTransferAmount+=nonTransfer;
    }
    // get incoming transfers
    string keyPref; // prefix
    keyPref.push_back('I'); // suffix for key identification by id
    keyPref.append(pubKeyID.to20Char());
    keyPref.append("TC"); // suffix for transfers to collect
    keyPref.append(currencyId.to20Char());
    unsigned long prefLength = keyPref.length();
    rocksdb::Iterator* it = publicKeys->NewIterator(rocksdb::ReadOptions());
    int added=0;
    for (it->Seek(keyPref); it->Valid() && added<=10; it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to lists
        string idAndScenStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        string idStr = idAndScenStr.substr(0, idAndScenStr.length()-1);
        CompleteID id(idStr);
        unsigned char scenario = util.byteSeqAsUc(idAndScenStr.substr(idAndScenStr.length()-1, 1));
        // get amount etc.
        double amount = getCollectableLiquidity(id, pubKeyID, currencyId, discountRate, scenario, zeroId, currentTime);
        if (amount<=0)
        {
            publicKeys->Delete(rocksdb::WriteOptions(), keyStr);
            continue;
        }
        CompleteID idLatest = getLatestID(id);
        // add if fresh
        if (type1entry->isFresh(idLatest, currentTime) || freshByDefi(idLatest))
        {
            predecessorsF.push_back(id);
            predecessorsL.push_back(idLatest);
            scenarios.push_back(scenario);
            impacts.push_back(amount);
            totalAmount+=amount;
            double nonTransfer = getCollectableNonTransferLiqui(id, pubKeyID, currencyId, discountRate, scenario, zeroId, currentTime);
            nonTransferAmount+=nonTransfer;
            added++;
        }
        else
        {
            puts("Database::buildNextClaim: transfer not fresh");
        }
    }
    delete it;
    // get incoming notarization fees if notary
    TNtrNr tNtrNr = getTotalNotaryNr(pubKeyID);
    if (!tNtrNr.isZero())
    {
        CompleteID id = pubKeyID;
        CompleteID idLatest = pubKeyIDLatest;
        double amount = getCollectableLiquidity(pubKeyID, pubKeyID, currencyId, discountRate, 3, zeroId, currentTime);
        // check if can collect own fees
        unsigned long long tenureEnd = type1entry->getNotaryTenureEndCorrected(tNtrNr.getLineage(), tNtrNr.getNotaryNr());
        if (currentTime >= tenureEnd && amount > 0
                && getCollectingClaim(pubKeyID, pubKeyID, currencyId, 3, zeroId).getNotary() <= 0)
        {
            //puts("Database::buildNextClaim: notary's own fees to collect");
        }
        else // search for other eligible fee sources
        {
            amount = 0;

            keyPref = "";
            keyPref.push_back('B'); // prefix for body
            keyPref.append(tNtrNr.toString());
            keyPref.append("ISF"); // prefix for to collect from
            prefLength = keyPref.length();

            string keyMin(keyPref);
            keyMin.append(util.UlAsByteSeq(0));
            // search forward
            it = notaries->NewIterator(rocksdb::ReadOptions());
            for (it->Seek(keyMin); it->Valid(); it->Next())
            {
                // check the prefix
                string keyStr = it->key().ToString();
                if (keyStr.length()<=prefLength) break;
                if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
                // extract notary
                string notary2NumStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
                unsigned long notary2Num = util.byteSeqAsUl(notary2NumStr);
                TNtrNr tNtrNr2(tNtrNr.getLineage(), notary2Num);
                id = getNotaryId(tNtrNr2);
                idLatest = getLatestID(id);
                // check if already collected etc.
                if ((type1entry->isFresh(idLatest, currentTime) || freshByDefi(idLatest))
                        && getCollectingClaim(id, pubKeyID, currencyId, 3, zeroId).getNotary()<=0)
                {
                    tenureEnd = type1entry->getNotaryTenureEndCorrected(tNtrNr.getLineage(), notary2Num);
                    if (currentTime >= tenureEnd)
                    {
                        amount = getCollectableLiquidity(id, pubKeyID, currencyId, discountRate, 3, zeroId, currentTime);
                        if (amount>0) break;
                    }
                }

                amount = 0;
            }
            delete it;
        }

        if (amount>0)
        {
            predecessorsF.push_back(id);
            predecessorsL.push_back(idLatest);
            scenarios.push_back(3);
            impacts.push_back(amount);
            totalAmount+=amount;
        }
    }
    // build entry from data and return
    CompleteID latestKeyId = getLatestID(pubKeyID);
    if (nonTransferAmount>totalAmount) nonTransferAmount = totalAmount;
    Type14Entry* out = new Type14Entry(currencyId, latestKeyId, totalAmount, nonTransferAmount,
                                       impacts, predecessorsL, scenarios);
    if (!out->isGood())
    {
        puts("Database::buildNextClaim: bad Type14Entry");
        delete out;
        return nullptr;
    }
    return out;
}

// db must be locked for this
size_t Database::getExchangeOffers(CompleteID &pubKeyID, CompleteID &currencyOId, CompleteID &currencyRId, unsigned short &rangeNum, unsigned short &maxNum, list<CompleteID> &idsList)
{
    if (!idsList.empty()) return 0;
    pubKeyID = getFirstID(pubKeyID);
    // define prefix
    string keyPref;
    keyPref.push_back('I'); // suffix for key identification by id
    unsigned long prefLength;
    rocksdb::Iterator* it;
    if (pubKeyID.getNotary()>0) // go to publicKeys
    {
        keyPref.append(pubKeyID.to20Char());
        keyPref.append("EO"); // suffix for exchange offers
        keyPref.append(currencyOId.to20Char());
        keyPref.append(currencyRId.to20Char());
        keyPref.append(util.UsAsByteSeq(rangeNum));
        prefLength = keyPref.length();
        it = publicKeys->NewIterator(rocksdb::ReadOptions());
    }
    else // go to currenciesAndObligations
    {
        keyPref.append(currencyOId.to20Char());
        keyPref.append("EO"); // suffix for exchange offers
        keyPref.append(currencyRId.to20Char());
        keyPref.append(util.UsAsByteSeq(rangeNum));
        prefLength = keyPref.length();
        it = currenciesAndObligations->NewIterator(rocksdb::ReadOptions());
    }
    // construct list
    for (it->Seek(keyPref); it->Valid() && idsList.size()<maxNum; it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to list
        string ratioAndIdStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        if (ratioAndIdStr.length()!=28) break;
        string idStr = ratioAndIdStr.substr(8, 20);
        CompleteID id(idStr);
        idsList.push_back(id);
    }
    delete it;
    return idsList.size();
}

// db must be locked for this
void Database::updateExchangeOfferRatio(CompleteID &offerId, Type12Entry* offerEntry)
{
    if (offerEntry == nullptr || offerEntry->underlyingType()!=10) return;
    Type10Entry *t10e = (Type10Entry*) offerEntry->underlyingEntry();
    CompleteID ownerId = offerEntry->pubKeyID();
    ownerId = getFirstID(ownerId);
    CompleteID currencyOId = t10e->getCurrencyOrObl();
    CompleteID currencyRId = t10e->getTarget();
    double amountR = t10e->getRequestedAmount();
    if (removeFromExchangeOffersList(offerId, ownerId, currencyOId, currencyRId))
        addToExchangeOffersList(offerId, ownerId, currencyOId, currencyRId, amountR);
}

// db must be locked for this
size_t Database::getTransferRequests(CompleteID &pubKeyID, CompleteID &currencyId, CompleteID &maxId, unsigned short &maxNum, list<CompleteID> &idsList)
{
    if (!idsList.empty()) return 0;
    pubKeyID = getFirstID(pubKeyID);
    // define prefix
    string keyPref;
    keyPref.push_back('I'); // suffix for key identification by id
    keyPref.append(pubKeyID.to20Char());
    keyPref.append("TR"); // suffix for transfers requested
    keyPref.append(currencyId.to20Char());
    unsigned long prefLength = keyPref.length();
    string keyMax(keyPref);
    keyMax.append(maxId.to20Char());
    // construct list
    rocksdb::Iterator* it = publicKeys->NewIterator(rocksdb::ReadOptions());
    for (it->SeekForPrev(keyMax); it->Valid() && idsList.size()<maxNum; it->Prev())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to list
        string idStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        CompleteID id(idStr);
        idsList.push_back(id);
    }
    delete it;
    return idsList.size();
}

// db must be locked for this
size_t Database::loadOutgoingShares(TNtrNr &totalNotaryNr, map<unsigned long, double> &sharesMap)
{
    if (!sharesMap.empty()) return 0;
    string keyPref;
    keyPref.push_back('B'); // prefix for body
    keyPref.append(totalNotaryNr.toString());
    keyPref.append("OSM"); // prefix for outgoing share multiplier
    unsigned long prefLength = keyPref.length();
    // construct list
    double multipliersSum = getMultipliersSum(totalNotaryNr);
    double shareToHandOver = 1.0;
    shareToHandOver -= getShareToKeep(totalNotaryNr);
    rocksdb::Iterator* it = notaries->NewIterator(rocksdb::ReadOptions());
    for (it->Seek(keyPref); it->Valid(); it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to list
        string notNumStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        unsigned long notNum = util.byteSeqAsUl(notNumStr);

        string shareMultiplierStr = it->value().ToString();
        double share = shareToHandOver * util.byteSeqAsDbl(shareMultiplierStr) / multipliersSum;

        sharesMap.insert(pair<unsigned long, double>(notNum, share));
    }
    delete it;
    return sharesMap.size();
}

// db must be locked for this
size_t Database::loadIncomingShares(TNtrNr &totalNotaryNr, map<unsigned long, double> &sharesMap)
{
    if (!sharesMap.empty()) return 0;
    string keyPref;
    keyPref.push_back('B'); // prefix for body
    keyPref.append(totalNotaryNr.toString());
    keyPref.append("ISF"); // prefix for incoming shares
    unsigned long prefLength = keyPref.length();
    // construct list
    rocksdb::Iterator* it = notaries->NewIterator(rocksdb::ReadOptions());
    for (it->Seek(keyPref); it->Valid(); it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to list
        string notNumStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        unsigned long notNum = util.byteSeqAsUl(notNumStr);
        TNtrNr tNtrNr(totalNotaryNr.getLineage(), notNum);

        double multipliersSum = getMultipliersSum(tNtrNr);
        double shareToHandOver = 1.0;
        shareToHandOver -= getShareToKeep(tNtrNr);
        double share = shareToHandOver * getRedistributionMultiplier(tNtrNr, totalNotaryNr) / multipliersSum;

        sharesMap.insert(pair<unsigned long, double>(notNum, share));
    }
    delete it;
    return sharesMap.size();
}

// db must be locked for this
double Database::getShareToKeep(TNtrNr &totalNotaryNr)
{
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(totalNotaryNr.toString());
    key.append("SK"); // prefix for share to keep
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
void Database::decreaseShareToKeep(TNtrNr &totalNotaryNr, double penaltyFactor)
{
    double share = getShareToKeep(totalNotaryNr) * penaltyFactor;
    string key;
    key.push_back('B'); // prefix for body
    key.append(totalNotaryNr.toString());
    key.append("SK"); // prefix for share to keep
    notaries->Put(rocksdb::WriteOptions(), key,  util.DblAsByteSeq(share));
}

// db must be locked for this
double Database::getCollectedFees(TNtrNr &totalNotaryNr, CompleteID &currencyID)
{
    string keyPref;
    keyPref.push_back('B'); // prefix for body
    keyPref.append(totalNotaryNr.toString());
    keyPref.append("CF"); // prefix for collected fees
    keyPref.append(currencyID.to20Char());
    unsigned long prefLength = keyPref.length();
    string keyMin(keyPref);
    keyMin.append(util.flip(util.UsAsByteSeq(0)));
    // search forward
    double sum = 0;
    rocksdb::Iterator* it = notaries->NewIterator(rocksdb::ReadOptions());
    for (it->Seek(keyMin); it->Valid(); it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // extract value
        string valueStr = it->value().ToString();
        sum += util.byteSeqAsDbl(valueStr);
    }
    delete it;
    return sum;
}

// db must be locked for this
void Database::addToCollectedFees(TNtrNr &totalNotaryNr, CompleteID &currencyID, double amount)
{
    if (amount <= 0) return;

    // get lowest value
    double minFee = getLowerTransferLimit(currencyID);
    minFee *= type1entry->getFee(totalNotaryNr.getLineage());
    minFee *= 0.01;

    if (minFee <= 0) return;

    // get range and range limit
    unsigned short range = 0;
    double relativeAmount = amount/minFee;
    if (relativeAmount>=2)
    {
        range = (unsigned short) log2(relativeAmount);
    }
    double rangeLimit = minFee * exp2(range+1);

    // sum with existing range value
    double sum = amount;
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(totalNotaryNr.toString());
    key.append("CF"); // prefix for collected fees
    key.append(currencyID.to20Char());
    key.append(util.flip(util.UsAsByteSeq(range)));
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (success) sum += util.byteSeqAsDbl(value);

    // update range value
    if (!success || sum<rangeLimit)
    {
        notaries->Put(rocksdb::WriteOptions(), key,  util.DblAsByteSeq(sum));
        return;
    }
    else
    {
        notaries->Delete(rocksdb::WriteOptions(), key);
        addToCollectedFees(totalNotaryNr, currencyID, sum);
    }
}

// db must be locked for this
double Database::getRedistributionMultiplier(TNtrNr &from, TNtrNr &to)
{
    if (from.getLineage()!=to.getLineage()) return 0;
    if (from.getNotaryNr()==to.getNotaryNr()) return 0;
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(from.toString());
    key.append("OSM"); // prefix for outgoing share multiplier
    key.append(util.UlAsByteSeq(to.getNotaryNr()));
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
double Database::getMultipliersSum(TNtrNr &from)
{
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(from.toString());
    key.append("MS"); // prefix for outgoing share multiplier
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
void Database::addToRedistributionMultiplier(TNtrNr &from, TNtrNr &to, double penaltyFactor)
{
    if (from.getLineage()!=to.getLineage()) return;
    if (from.getNotaryNr()==to.getNotaryNr()) return;
    // add to outgoing
    double multiplier = 1.0;
    multiplier -= penaltyFactor;
    multiplier += getRedistributionMultiplier(from, to);
    string key;
    key.push_back('B'); // prefix for body
    key.append(from.toString());
    key.append("OSM"); // prefix for outgoing shares
    key.append(util.UlAsByteSeq(to.getNotaryNr()));
    notaries->Put(rocksdb::WriteOptions(), key,  util.DblAsByteSeq(multiplier));
    // add to overall multipliers sum
    double multipliersSum = 1.0;
    multipliersSum -= penaltyFactor;
    key = "";
    string value;
    key.push_back('B'); // prefix for body
    key.append(from.toString());
    key.append("MS"); // prefix for multipliers sum
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (success) multipliersSum += util.byteSeqAsDbl(value);
    notaries->Put(rocksdb::WriteOptions(), key,  util.DblAsByteSeq(multipliersSum));
    // add flag to incoming
    key="";
    key.push_back('B'); // prefix for body
    key.append(to.toString());
    key.append("ISF"); // prefix for incoming shares from
    key.append(util.UlAsByteSeq(from.getNotaryNr()));
    notaries->Put(rocksdb::WriteOptions(), key,  from.toString());
}

// db must be locked for this
unsigned short Database::getInitiatedThreadsCount(TNtrNr &tNtrNr)
{
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(tNtrNr.toString());
    key.append("ITC"); // prefix for initiated threads count
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>1);
    if (!success) return 0;
    return util.byteSeqAsUs(value);
}

// db must be locked for this
void Database::checkForInitiatedThreadConflict(TNtrNr &tNtrNr, CompleteID &currentRefId)
{
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(tNtrNr.toString());
    key.append("ITN"); // prefix for initiated thread in notarization
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return;
    CompleteID threadId(value);
    if (isInGeneralList(threadId))
    {
        notaries->Delete(rocksdb::WriteOptions(), key);
        return;
    }
    int notStatus = verifyConflict(threadId, currentRefId);
    if (notStatus == -1)
    {
        notaries->Delete(rocksdb::WriteOptions(), key);
        return;
    }
}

// db must be locked for this
void Database::setInitiatedThreadsCount(TNtrNr &tNtrNr, unsigned short newCount)
{
    string key;
    key.push_back('B'); // prefix for body
    key.append(tNtrNr.toString());
    key.append("ITC"); // prefix for initiated threads count
    notaries->Put(rocksdb::WriteOptions(), key,  util.UsAsByteSeq(newCount));
}

// db must be locked for this
NotaryInfo* Database::getNotaryInfo(string *publicKeyStr, CompleteID &currencyId)
{
    CompleteID pubKeyID = getFirstID(publicKeyStr);
    pubKeyID = getFirstID(pubKeyID);
    currencyId = getFirstID(currencyId);
    // check if there is a pending application
    CompleteID pendingApplicationId;
    list<CompleteID> idsList;
    CompleteID minApplId;
    if (getNotaryApplications(pubKeyID, true, 0, minApplId, 2, idsList)>0) pendingApplicationId = *idsList.begin();
    else if (getNotaryApplications(pubKeyID, true, 1, minApplId, 2, idsList)>0) pendingApplicationId = *idsList.begin();
    else if (getNotaryApplications(pubKeyID, true, 2, minApplId, 2, idsList)>0) pendingApplicationId = *idsList.begin();
    idsList.clear();
    if (pendingApplicationId.getNotary()>0)
    {
        map<unsigned long, double> emptyMap1;
        map<unsigned long, double> emptyMap2;
        NotaryInfo *out = new NotaryInfo(pendingApplicationId, 0, emptyMap1, emptyMap2, pubKeyID, currencyId, 0);
        return out;
    }
    // check if notary
    TNtrNr tNtrNr = getTotalNotaryNr(publicKeyStr);
    if (tNtrNr.isZero())
    {
        map<unsigned long, double> emptyMap1;
        map<unsigned long, double> emptyMap2;
        CompleteID zeroId;
        NotaryInfo *out = new NotaryInfo(zeroId, 0, emptyMap1, emptyMap2, pubKeyID, currencyId, 0);
        return out;
    }
    // get application
    CompleteID applicationId;
    if (getNotaryApplications(pubKeyID, true, 3, minApplId, 2, idsList)>0) applicationId = *idsList.begin();
    idsList.clear();
    // load outgoing shares
    map<unsigned long, double> outgoingShares;
    loadOutgoingShares(tNtrNr, outgoingShares);
    double shareToKeep = getShareToKeep(tNtrNr);
    outgoingShares.insert(pair<unsigned long, double>(tNtrNr.getNotaryNr(), shareToKeep));
    // load incoming shares
    map<unsigned long, double> incomingShares;
    loadIncomingShares(tNtrNr, incomingShares);
    // load collected liquidity
    double collectedFees = getCollectedFees(tNtrNr, currencyId);
    unsigned short initiatedThreads = getInitiatedThreadsCount(tNtrNr);
    // return
    NotaryInfo *out = new NotaryInfo(applicationId, initiatedThreads, outgoingShares, incomingShares, pubKeyID, currencyId, collectedFees);
    return out;
}

// db must be locked for this
RefereeInfo* Database::getRefereeInfo(CompleteID &pubKeyID, CompleteID &currencyId)
{
    pubKeyID = getFirstID(pubKeyID);
    currencyId = getFirstID(currencyId);
    // check if there is a pending application
    CompleteID pendingApplicationId;
    list<CompleteID> idsList;
    CompleteID minApplId;
    if (getApplications("AR", pubKeyID, true, currencyId, 0, minApplId, 2, idsList)>0) pendingApplicationId = *idsList.begin();
    else if (getApplications("AR", pubKeyID, true, currencyId, 1, minApplId, 2, idsList)>0) pendingApplicationId = *idsList.begin();
    else if (getApplications("AR", pubKeyID, true, currencyId, 2, minApplId, 2, idsList)>0) pendingApplicationId = *idsList.begin();
    idsList.clear();
    // check if ref is acting ref at the moment
    CompleteID currentAppointmentId;
    CompleteID lastSuccessfulApplicationId;
    unsigned long long currentTime = systemTimeInMs();
    unsigned long long minApplTime = (unsigned long long) getRefereeTenure(currencyId);
    minApplTime *= 3000;
    if (minApplTime<currentTime) minApplTime = currentTime - minApplTime;
    else minApplTime=0;
    minApplId = CompleteID(0,0,minApplTime);
    if (getApplications("AR", pubKeyID, true, currencyId, 3, minApplId, 6, idsList)>0)
    {
        list<CompleteID>::iterator it;
        for (it=idsList.begin(); it!=idsList.end(); ++it)
        {
            if (lastSuccessfulApplicationId < *it) lastSuccessfulApplicationId = *it;
            CompleteID terminationID = getTerminationId(*it);
            if (getRefereeTenureStart(pubKeyID, terminationID) <= currentTime
                    && currentTime < getRefereeTenureEnd(pubKeyID, terminationID))
            {
                currentAppointmentId = terminationID;
            }
        }
    }
    // correct currentAppointmentId and pendingApplicationId if necessary
    if (isActingReferee(pubKeyID, currencyId, currentTime))
    {
        currentAppointmentId = currencyId;
    }
    if (pendingApplicationId.getNotary()<=0)
    {
        CompleteID terminationID = getTerminationId(lastSuccessfulApplicationId);
        if (currentTime < getRefereeTenureStart(pubKeyID, terminationID))
            pendingApplicationId = lastSuccessfulApplicationId;
    }
    // report
    if (currentAppointmentId.getNotary()<=0)
    {
        CompleteID zeroId;
        RefereeInfo *out = new RefereeInfo(zeroId, 0, 0, 0, pendingApplicationId);
        return out;
    }
    unsigned long long tenureEnd = getRefereeTenureEnd(pubKeyID, currentAppointmentId);
    double liquidityCreated = getLiquidityCreatedSoFar(pubKeyID, currentAppointmentId);
    unsigned short refsAppointed = getRefsAppointedSoFar(pubKeyID, currentAppointmentId);
    currentAppointmentId = getLatestID(currentAppointmentId);
    RefereeInfo *out = new RefereeInfo(currentAppointmentId, tenureEnd, liquidityCreated, refsAppointed, pendingApplicationId);
    return out;
}

// db must be locked for this
unsigned long long Database::getNotaryTenureStart(TNtrNr tNtrNr)
{
    // check that notary exists already
    unsigned long maxNotaryNum = getNrOfNotariesInLineage(tNtrNr.getLineage());
    if (tNtrNr.getNotaryNr()>maxNotaryNum) return ULLONG_MAX;
    // check if appointed by type1entry
    if (tNtrNr.getNotaryNr()<=type1entry->getMaxActing(tNtrNr.getLineage()))
        return type1entry->getNotaryTenureStartCorrected(tNtrNr.getLineage(), tNtrNr.getNotaryNr());
    // calculate based on appointment
    string key;
    string value;
    key.push_back('B'); // prefix for body
    key.append(tNtrNr.toString());
    key.append("T2E"); // prefix for type 2 entry
    rocksdb::Status s = notaries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return ULLONG_MAX;
    CompleteID t2eId(value);
    return max(t2eId.getTimeStamp(),
               type1entry->getNotaryTenureStartCorrected(tNtrNr.getLineage(), tNtrNr.getNotaryNr()));
}

// db must be locked for this
CompleteID Database::getLastEssentialEntryId(CompleteID &currentRefId)
{
    string keyPref;
    keyPref.push_back('B'); // prefix for body
    unsigned long prefLength = keyPref.length();
    CompleteID maxID(ULONG_MAX, ULLONG_MAX, ULLONG_MAX);
    string keyMax(keyPref);
    keyMax.append(maxID.to20Char());
    // search backwards
    string keyStr;
    CompleteID out;
    rocksdb::Iterator* it = essentialEntries->NewIterator(rocksdb::ReadOptions());
    list<string> toDeleteList;
    for (it->SeekForPrev(keyMax); it->Valid(); it->Prev())
    {
        // check the prefix
        keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // extract id
        string idStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        out = CompleteID(idStr);
        // check if notarized
        if (isInGeneralList(out)) break;
        else
        {
            // check for conflict
            if (verifyConflict(out, currentRefId) == -1) toDeleteList.push_back(keyStr);
            out = CompleteID();
        }
    }
    delete it;

    // clean up
    list<string>::iterator it2;
    for (it2=toDeleteList.begin(); it2!=toDeleteList.end(); ++it2)
    {
        essentialEntries->Delete(rocksdb::WriteOptions(), *it2);
    }

    return out;
}

// db must be locked for this
size_t Database::getEssentialEntries(CompleteID &minEntryId, list<CompleteID> &idsList)
{
    if (!idsList.empty()) return 0;
    string keyPref;
    keyPref.push_back('B'); // prefix for body
    unsigned long prefLength = keyPref.length();
    // define min key
    string keyMin(keyPref);
    keyMin.append(minEntryId.to20Char());
    // construct list
    rocksdb::Iterator* it = essentialEntries->NewIterator(rocksdb::ReadOptions());
    for (it->Seek(keyMin); it->Valid(); it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to list
        string idStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        CompleteID id(idStr);
        if (isInGeneralList(id)) idsList.push_back(id);
    }
    delete it;
    return idsList.size();
}

// db must be locked for this
size_t Database::getNotaryApplications(CompleteID &pubKeyID, bool isApplicant, unsigned char status, CompleteID &minApplId, unsigned short maxNum, list<CompleteID> &idsList)
{
    if (!idsList.empty()) return 0;
    pubKeyID = getFirstID(pubKeyID);
    // define prefix and dbPart
    string keyPref;
    rocksdb::DB* dbPart=nullptr;
    if (pubKeyID.getNotary()<=0)
    {
        keyPref.push_back('B'); // prefix for body
        keyPref.append(util.UcAsByteSeq(status));
        dbPart=notaryApplications;
    }
    else if (isApplicant)
    {
        keyPref.push_back('I'); // suffix for key identification by id
        keyPref.append(pubKeyID.to20Char());
        keyPref.append("AN"); // suffix for notary applications
        keyPref.append(util.UcAsByteSeq(status));
        dbPart=publicKeys;
    }
    else
    {
        TNtrNr totalNotaryNr = getTotalNotaryNr(pubKeyID);
        if (!totalNotaryNr.isZero())
        {
            keyPref.push_back('B'); // prefix for body
            keyPref.append(totalNotaryNr.toString());
            keyPref.append("AN"); // suffix for notary applications
            keyPref.append(util.UcAsByteSeq(status));
            dbPart=notaries;
        }
        else return 0;
    }
    unsigned long prefLength = keyPref.length();
    // define min key
    string keyMin(keyPref);
    keyMin.append(minApplId.to20Char());
    // construct list
    rocksdb::Iterator* it = dbPart->NewIterator(rocksdb::ReadOptions());
    for (it->Seek(keyMin); it->Valid() && idsList.size()<maxNum; it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to list
        string idStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        CompleteID id(idStr);
        idsList.push_back(id);
    }
    delete it;
    return idsList.size();
}

// db must be locked for this
size_t Database::getApplications(string type, CompleteID &pubKeyID, bool isApplicant, CompleteID &currencyId, unsigned char status, CompleteID &minApplId,
                                 unsigned short maxNum, list<CompleteID> &idsList)
{
    if (!idsList.empty()) return 0;
    if (type.compare("OP")!=0 && type.compare("AR")!=0) return 0;
    pubKeyID = getFirstID(pubKeyID);
    // define prefix and dbPart
    string keyPref;
    rocksdb::DB* dbPart=nullptr;
    if (pubKeyID.getNotary()<=0)
    {
        keyPref.push_back('I'); // prefix to identification by Id
        keyPref.append(currencyId.to20Char());
        keyPref.append(type); // prefix for operation proposals/ref applications
        keyPref.append(util.UcAsByteSeq(status));
        dbPart=currenciesAndObligations;
    }
    else if (isApplicant)
    {
        keyPref.push_back('I'); // suffix for key identification by id
        keyPref.append(pubKeyID.to20Char());
        keyPref.append(type); // suffix for operation proposals/ref applications
        keyPref.append(util.UcAsByteSeq(status));
        keyPref.append(currencyId.to20Char());
        dbPart=publicKeys;
    }
    else
    {
        keyPref.push_back('I'); // suffix for key identification by id
        keyPref.append(pubKeyID.to20Char());
        keyPref.push_back('R'); // suffix for referee information
        keyPref.push_back('C'); // suffix for currency choice
        keyPref.append(currencyId.to20Char());
        keyPref.append(type); // suffix for operation proposals/ref applications
        keyPref.append(util.UcAsByteSeq(status));
        dbPart=publicKeys;
    }
    unsigned long prefLength = keyPref.length();
    // define min key
    string keyMin(keyPref);
    keyMin.append(minApplId.to20Char());
    // construct list
    rocksdb::Iterator* it = dbPart->NewIterator(rocksdb::ReadOptions());
    for (it->Seek(keyMin); it->Valid() && idsList.size()<maxNum; it->Next())
    {
        // check the prefix
        string keyStr = it->key().ToString();
        if (keyStr.length()<=prefLength) break;
        if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
        // add to list
        string idStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
        CompleteID id(idStr);
        idsList.push_back(id);
    }
    delete it;
    return idsList.size();
}

// db must be locked for this
unsigned long long Database::getClaimCount(CompleteID &pubKeyID, CompleteID &currencyId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyID.to20Char());
    key.push_back('C'); // suffix for liquidity claims
    key.append(currencyId.to20Char());
    key.push_back('H'); // suffix for head (count)
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUll(value);
}

// db must be locked for this
CompleteID Database::getClaim(CompleteID &pubKeyID, CompleteID &currencyId, unsigned long long k, CompleteID &currentRefId)
{
    string key;
    string value;
    key.push_back('I'); // suffix for key identification by id
    key.append(pubKeyID.to20Char());
    key.push_back('C'); // suffix for liquidity claims
    key.append(currencyId.to20Char());
    key.push_back('B'); // suffix for body
    key.append(util.UllAsByteSeq(k));
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success)
    {
        return CompleteID();
    }
    CompleteID claimId(value);
    if (isInGeneralList(claimId)) return claimId;
    int notStatus = verifyConflict(claimId, currentRefId);
    if (notStatus==-1) notarizationEntries->Delete(rocksdb::WriteOptions(), key);
    return CompleteID();
}

// db must be locked for this
double Database::getDiscountRate(CompleteID &currencyOrOblId)
{
    string key;
    string value;
    key.push_back('I'); // prefix to identification by Id
    key.append(currencyOrOblId.to20Char());
    key.append("DR"); // suffix for discount rate
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsDbl(value);
}

// db must be locked for this
CompleteID Database::getCollectingClaim(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
                                        unsigned char scenario, CompleteID &currentRefId)
{
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(entryID.to20Char());
    key.append("GCC"); // suffix for get collecting claims
    key.append(currencyOrOblId.to20Char());
    key.append(pubKeyId.to20Char());
    key.append(util.UcAsByteSeq(scenario));
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID claimId(value);
    if (isInGeneralList(claimId)) return claimId;
    int notStatus = verifyConflict(claimId, currentRefId);
    if (notStatus==-1) notarizationEntries->Delete(rocksdb::WriteOptions(), key);
    return CompleteID();
}

// db must be locked for this
double Database::getCollectableLiquidity(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
        double discountrate, unsigned char scenario, CompleteID &currentRefId, unsigned long long systemTime)
{
    double rawAmount = 0;
    unsigned long long discountStartDate = entryID.getTimeStamp(); // default discount start
    unsigned long long notTenure = type1entry->getNotaryTenure(type1entry->getLineage(entryID.getTimeStamp()));
    unsigned long long claimTimeLimit=ULLONG_MAX;
    if (scenario == 0) // entry must be of type 14 in this case
    {
        if (getEntryType(entryID) != 14) return 0;
        Type12Entry *type12entry = buildUnderlyingEntry(entryID,0);
        if (type12entry==nullptr) return 0;
        Entry* claimEntry = type12entry->underlyingEntry();
        if (claimEntry == nullptr || claimEntry->getType() != 14)
        {
            delete type12entry;
            return 0;
        }
        Type14Entry* type14entry = static_cast<Type14Entry*>(claimEntry);
        // check ownership
        CompleteID ownerId = type14entry->getOwnerID();
        CompleteID ownerIdFirst = getFirstID(ownerId);
        if (ownerIdFirst != pubKeyId)
        {
            delete type12entry;
            return 0;
        }
        // check currency
        CompleteID currencyOrObl2Id = type14entry->getCurrencyOrObligation();
        if (getFirstID(currencyOrObl2Id) != currencyOrOblId)
        {
            delete type12entry;
            return 0;
        }
        rawAmount = type14entry->getTotalAmount();
        delete type12entry;
    }
    else if (scenario == 1)
    {
        // check if still can collect
        CompleteID conflictingClaim = getTransferCollectingClaim(entryID, currentRefId);
        if (conflictingClaim.getNotary()>0) return 0;
        // load entry
        if (getEntryType(entryID) != 10) return 0;
        Type12Entry *type12entry = buildUnderlyingEntry(entryID,0);
        if (type12entry==nullptr) return 0;
        Entry* transferEntry = type12entry->underlyingEntry();
        if (transferEntry == nullptr || transferEntry->getType() != 10)
        {
            delete type12entry;
            return 0;
        }
        Type10Entry* type10entry = static_cast<Type10Entry*>(transferEntry);
        // get data
        double feeFactor = 1.0;
        feeFactor -= (type1entry->getFee(type1entry->getLineage(entryID.getTimeStamp())) * 0.01);
        rawAmount = type10entry->getTransferAmount() * feeFactor;
        // check if pubKeyId is the recipient
        bool correctRecipient = false;
        CompleteID connectedTransfer = getConnectedTransfer(entryID, currentRefId);
        if (connectedTransfer == entryID) // there is no connected transfer
        {
            if (type10entry->getRequestedAmount()==0) // standard transfer
            {
                CompleteID recipientID = type10entry->getTarget();
                correctRecipient = (getFirstID(recipientID) == pubKeyId);
            }
            else // terminated exchange request
            {
                CompleteID signatory = type12entry->pubKeyID();
                correctRecipient = (getFirstID(signatory) == pubKeyId);
            }
        }
        else if (connectedTransfer.getNotary()>0) // recipient is the signatory of the other entry
        {
            // load other entry
            string str2;
            if (loadInitialT13EntryStr(connectedTransfer, str2))
            {
                Type12Entry *type12entry2 = createT12FromT13Str(str2);
                if (type12entry2!=nullptr && type12entry2->underlyingType() == 10)
                {
                    CompleteID signatory2 = type12entry2->pubKeyID();
                    correctRecipient = (getFirstID(signatory2) == pubKeyId);
                }
                if (type12entry2!=nullptr) delete type12entry2;
            }
        }
        delete type12entry;
        if (!correctRecipient) return 0;
    }
    else if (scenario == 2)
    {
        if (getEntryType(entryID) != 10) return 0;
        Type12Entry *type12entry = buildUnderlyingEntry(entryID,0);
        if (type12entry==nullptr) return 0;
        Entry* transferEntry = type12entry->underlyingEntry();
        if (transferEntry == nullptr || transferEntry->getType() != 10)
        {
            delete type12entry;
            return 0;
        }
        Type10Entry* type10entry = static_cast<Type10Entry*>(transferEntry);
        CompleteID ownerId = type12entry->pubKeyID();
        CompleteID ownerIdFirst = getFirstID(ownerId);
        if (ownerIdFirst != pubKeyId)
        {
            delete type12entry;
            return 0;
        }
        rawAmount = -type10entry->getTransferAmount();
        delete type12entry;
    }
    else if (scenario == 3)
    {
        TNtrNr tNtrNr = getTotalNotaryNr(pubKeyId);
        if (tNtrNr.isZero()) return 0;
        TNtrNr tNtrNr2 = getTotalNotaryNr(entryID);
        if (tNtrNr2.isZero() || tNtrNr2.getLineage()!=tNtrNr.getLineage()) return 0;
        notTenure = type1entry->getNotaryTenure(tNtrNr2.getLineage());
        discountStartDate = type1entry->getNotaryTenureEndCorrected(tNtrNr2.getLineage(), tNtrNr2.getNotaryNr());
        unsigned long long currentTime = currentRefId.getTimeStamp();
        if (currentRefId.isZero()) currentTime = systemTime;
        if (currentTime<discountStartDate) return 0;
        claimTimeLimit = discountStartDate + notTenure*1000;
        double collectedFees = getCollectedFees(tNtrNr2, currencyOrOblId);
        double shareToKeep = getShareToKeep(tNtrNr2);
        if (tNtrNr.getNotaryNr() == tNtrNr2.getNotaryNr()) // claiming own fees
        {
            rawAmount = collectedFees * shareToKeep;
        }
        else
        {
            double shareToHandOver = 1.0;
            shareToHandOver -= shareToKeep;
            rawAmount = collectedFees * shareToHandOver;
            rawAmount *= (getRedistributionMultiplier(tNtrNr2, tNtrNr) / getMultipliersSum(tNtrNr2));
        }
    }
    else if (scenario == 5)
    {
        if (getEntryType(entryID) != 7) return 0;
        if (getThreadParticipantId(entryID) != pubKeyId) return 0;
        rawAmount = -getForfeit(entryID);
    }
    else if (scenario == 6)
    {
        if (getEntryType(entryID) != 8) return 0;
        if (getThreadParticipantId(entryID) != pubKeyId) return 0;
        CompleteID applicationId = getApplicationId(entryID);
        rawAmount = -getRefereeStake(applicationId);
    }
    else if (scenario == 4 || scenario == 7 || scenario == 8 || scenario == 9)
    {
        string key;
        string value;
        key.push_back('I'); // suffix for key identification by id
        key.append(pubKeyId.to20Char());
        key.append("TT"); // suffix for thread termination entries
        key.push_back('N'); // liquidity not claimed yet
        key.append(util.UcAsByteSeq(scenario));
        key.append(entryID.to20Char());
        key.push_back('T'); // suffix for total claimable liquidity
        rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
        const bool success = (s.ok() && value.length()>2);
        if (!success) return 0;
        rawAmount=util.byteSeqAsDbl(value);
        claimTimeLimit = entryID.getTimeStamp() + notTenure*1000;
    }
    else return 0;
    unsigned long long discountEndDate = currentRefId.getTimeStamp();
    if (currentRefId.isZero())
    {
        discountEndDate = systemTime;
        if (rawAmount * discountrate < 0) discountEndDate+=discountTimeBufferInMs;
        else discountEndDate-=discountTimeBufferInMs;
    }
    if (claimTimeLimit < discountEndDate) return 0;
    if (rawAmount == 0 || discountStartDate > discountEndDate) return rawAmount;
    return rawAmount * util.getDiscountFactor(discountStartDate, discountEndDate, discountrate);
}

// db must be locked for this
double Database::addToTTLiquidityNotClaimedYet(CompleteID &keyID, CompleteID &entryID, unsigned char scenario, bool totalLiqui, double amount)
{
    string key;
    string value;
    double oldSum=0;
    key.push_back('I'); // suffix for key identification by id
    key.append(keyID.to20Char());
    key.append("TT"); // suffix for thread termination entries
    key.push_back('N'); // liquidity not claimed yet
    key.append(util.UcAsByteSeq(scenario));
    key.append(entryID.to20Char());
    if (totalLiqui) key.push_back('T'); // suffix for total claimable liquidity
    else key.push_back('N'); // suffix for non transferable liquidity
    rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (success) oldSum=util.byteSeqAsDbl(value);
    if (amount==0) return oldSum;
    double newSum = oldSum+amount;
    publicKeys->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(newSum));
    return newSum;
}

// db must be locked for this
double Database::getCollectableTransferableLiqui(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
        double discountrate, unsigned char scenario, CompleteID &currentRefId, unsigned long long systemTime)
{
    return getCollectableLiquidity(entryID, pubKeyId, currencyOrOblId, discountrate, scenario, currentRefId, systemTime)
           -getCollectableNonTransferLiqui(entryID, pubKeyId, currencyOrOblId, discountrate, scenario, currentRefId, systemTime);
}

// db must be locked for this
double Database::getCollectableNonTransferLiqui(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
        double discountrate, unsigned char scenario, CompleteID &currentRefId, unsigned long long systemTime)
{
    double rawAmount = 0;
    unsigned long long notTenure = type1entry->getNotaryTenure(type1entry->getLineage(entryID.getTimeStamp()));
    unsigned long long claimTimeLimit=ULLONG_MAX;
    if (scenario == 0)
    {
        if (getEntryType(entryID) != 14) return 0;
        Type12Entry *type12entry = buildUnderlyingEntry(entryID,0);
        if (type12entry==nullptr) return 0;
        Entry* claimEntry = type12entry->underlyingEntry();
        if (claimEntry == nullptr || claimEntry->getType() != 14)
        {
            delete type12entry;
            return 0;
        }
        Type14Entry* type14entry = static_cast<Type14Entry*>(claimEntry);
        // check ownership
        CompleteID ownerId = type14entry->getOwnerID();
        CompleteID ownerIdFirst = getFirstID(ownerId);
        if (ownerIdFirst != pubKeyId)
        {
            delete type12entry;
            return 0;
        }
        // check currency
        CompleteID currencyOrObl2Id = type14entry->getCurrencyOrObligation();
        if (getFirstID(currencyOrObl2Id) != currencyOrOblId)
        {
            delete type12entry;
            return 0;
        }
        rawAmount = type14entry->getNonTransferAmount();
        delete type12entry;
    }
    else if (scenario == 6)
    {
        if (getEntryType(entryID) != 8) return 0;
        if (getThreadParticipantId(entryID) != pubKeyId) return 0;
        // get stake
        CompleteID applicationId = getApplicationId(entryID);
        double stake = getRefereeStake(applicationId);
        // get used claim entry
        CompleteID usedClaimIdFirst = getLiquidityClaimId(entryID);
        if (getEntryType(usedClaimIdFirst) != 14) return 0;
        Type12Entry *type12entryC = buildUnderlyingEntry(usedClaimIdFirst,0);
        if (type12entryC==nullptr) return 0;
        Entry* claimEntry = type12entryC->underlyingEntry();
        if (claimEntry == nullptr || claimEntry->getType() != 14)
        {
            delete type12entryC;
            return 0;
        }
        Type14Entry* type14entry = static_cast<Type14Entry*>(claimEntry);
        // calculate used non-transferable amount
        rawAmount = -min(type14entry->getNonTransferAmount(), stake);
        delete type12entryC;
    }
    else if (scenario == 4 || scenario == 8)
    {
        string key;
        string value;
        key.push_back('I'); // suffix for key identification by id
        key.append(pubKeyId.to20Char());
        key.append("TT"); // suffix for thread termination entries
        key.push_back('N'); // liquidity not claimed yet
        key.append(util.UcAsByteSeq(scenario));
        key.append(entryID.to20Char());
        key.push_back('N'); // suffix for non transferable liquidity
        rocksdb::Status s = publicKeys->Get(rocksdb::ReadOptions(), key, &value);
        const bool success = (s.ok() && value.length()>2);
        if (!success) return 0;
        rawAmount=util.byteSeqAsDbl(value);
        claimTimeLimit = entryID.getTimeStamp() + notTenure*1000;
    }
    else return 0;
    unsigned long long discountStartDate = entryID.getTimeStamp();
    unsigned long long discountEndDate = currentRefId.getTimeStamp();
    if (currentRefId.isZero())
    {
        discountEndDate = systemTime;
        if (rawAmount * discountrate < 0) discountEndDate+=discountTimeBufferInMs;
        else discountEndDate-=discountTimeBufferInMs;
    }
    if (claimTimeLimit < discountEndDate) return 0;
    if (rawAmount == 0 || discountStartDate > discountEndDate) return rawAmount;
    return rawAmount * util.getDiscountFactor(discountStartDate, discountEndDate, discountrate);
}

// db must be locked for this
bool Database::checkForConsistency(Type12Entry* type12entry, CompleteID &currentRefId)
{
    if (type12entry == nullptr || !type12entry->isGood()) return false;
    if (getFirstID(type12entry).getNotary() > 0) return false;
    if (currentRefId.isZero()) return false;

    // some definitions
    int underlyingType = type12entry->underlyingType();
    Entry* underlyingEntry = type12entry->underlyingEntry();
    if (underlyingEntry == nullptr || !underlyingEntry->isGood()) return false;

    // retrieve public key of signatory
    string pubKey;
    Type11Entry* type11entry;
    if (underlyingType == 11)
    {
        // retrieve public key directly from Type11Entry
        type11entry = static_cast<Type11Entry*>(underlyingEntry);
        pubKey = *type11entry->getPublicKey();
    }
    else if (underlyingType == 2)
    {
        // retrieve public key directly from Type2Entry
        Type2Entry* type2entry = static_cast<Type2Entry*>(underlyingEntry);
        pubKey = *type2entry->getPublicKey();
    }
    else
    {
        // retrieve key from ID
        CompleteID pubKeyID = type12entry->pubKeyID();
        CompleteID pubKeyIdFirst = getFirstID(pubKeyID);
        if (!loadPubKey(pubKeyIdFirst, pubKey))
        {
            puts("Database::checkForConsistency: loadPubKey failed");
            puts(pubKeyID.to27Char().c_str());
            puts(pubKeyIdFirst.to27Char().c_str());
            return false;
        }
    }

    // check if signature is correct
    string* signedSeq = type12entry->getSignedSequence();
    string* signa = type12entry->getSignature();

    CryptoPP::SecByteBlock signature((const byte*)signa->c_str(), signa->length());
    CryptoPP::RSA::PublicKey publicKey;
    CryptoPP::ByteQueue bq;
    bq.Put((const byte*)pubKey.c_str(), pubKey.length());
    publicKey.Load(bq);
    CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA3_384>::Verifier verifier(publicKey);

    bool correct = verifier.VerifyMessage((const byte*)signedSeq->c_str(),
                                          signedSeq->length(), signature, signature.size());
    if (!correct)
    {
        puts("Database::checkForConsistency: incorrect signature in type12entry");
        return false;
    }

    // determine public key id
    CompleteID pubKeyID = getFirstID(&pubKey);
    unsigned long long currentValidityDate = ULLONG_MAX;
    if (pubKeyID.getNotary() > 0)
    {
        currentValidityDate = getValidityDate(pubKeyID);
    }

    // for type 11 just check that old validity date is larger
    if (underlyingType == 11) return (type11entry->getValidityDate() < currentValidityDate);

    // check that key is still valid
    if (currentValidityDate < currentRefId.getTimeStamp())
    {
        puts("Database::checkForConsistency: key not valid");
        return false;
    }

    // consider only registered keys from now on
    if (!isInGeneralList(pubKeyID))
    {
        puts("Database::checkForConsistency: pubKeyID not in general list");
        return false;
    }
    CompleteID pubKeyIdSign = type12entry->pubKeyID();
    if (getFirstID(pubKeyIdSign) != pubKeyID)
    {
        puts("Database::checkForConsistency: pubKeyID not first ID");
        return false;
    }
    if (!type1entry->isFresh(pubKeyIdSign, currentRefId.getTimeStamp()) && !freshByDefi(pubKeyIdSign))
    {
        puts("Database::checkForConsistency: pubKeyIdSign not fresh");
        return false;
    }

    // retrieve ID of the owner of the underlying entry and compare with signatory
    if (underlyingType != 2 && underlyingType != 4 && underlyingType != 8 && underlyingType != 10)
    {
        CompleteID ownerId = underlyingEntry->getOwnerID();
        if (getFirstID(ownerId) != pubKeyID)
        {
            puts("Database::checkForConsistency: bad ownerId");
            return false;
        }
    }

    bool allGood = true;
    // check referenced entries for existence and freshness
    list<CompleteID>* referencedEntries = underlyingEntry->getReferencedEntries();
    list<CompleteID>::iterator it;
    if (referencedEntries!=nullptr)
    {
        for (it=referencedEntries->begin(); allGood && it!=referencedEntries->end(); ++it)
        {
            allGood = type1entry->isFresh(*it, currentRefId.getTimeStamp())
                      || getOblOwner(*it).getNotary()>0     // if type 15 predecessor
                      || getRefereeTenure(*it)>0            // if type 5 predecessor
                      || getValidityDate(*it)>0             // if type 11 predecessor
                      || underlyingType == 14;              // will check for freshness later in this case
            if (allGood) allGood = isInGeneralList(*it);
            if (!allGood)
            {
                puts("checkForConsistency: bad entry");
                puts(it->to27Char().c_str());
            }
        }
    }
    // check referenced currencies and obligations for existence
    list<CompleteID>* referencedCaO = underlyingEntry->getCurrenciesAndObligations();
    if (referencedCaO!=nullptr)
    {
        for (it=referencedCaO->begin(); allGood && it!=referencedCaO->end(); ++it)
        {
            allGood = (isInGeneralList(*it) && (getOblOwner(*it).getNotary()>0 || getRefereeTenure(*it)>0));
            if (!allGood)
            {
                puts("checkForConsistency: bad referencedCaO");
                puts(it->to27Char().c_str());
            }
        }
    }
    if (!allGood)
    {
        puts("checkForConsistency: bad referenced entries for");
        puts(currentRefId.to27Char().c_str());
        puts(to_string(underlyingType).c_str());
        puts(pubKeyID.to27Char().c_str());
        if (underlyingType == 14)
        {
            puts(static_cast<Type14Entry*>(underlyingEntry)->getCurrencyOrObligation().to27Char().c_str());
        }
        return false;
    }

    // entry specific checks
    if (underlyingType == 2)
    {
        if (!getTotalNotaryNr(pubKeyID).isZero())
        {
            puts("checkForConsistency: notary already exists");
            return false;
        }
        Type2Entry* type2entry = static_cast<Type2Entry*>(underlyingEntry);
        CompleteID terminationID = type2entry->getTerminationID();
        CompleteID terminationIdFirst = getFirstID(terminationID);
        if (!recentlyApprovedNotary(pubKeyID, terminationIdFirst, currentRefId.getTimeStamp()))
        {
            puts("checkForConsistency: notary not recently approved");
            return false;
        }
        if (!possibleToAppointNewNotaries(currentRefId.getTimeStamp()))
        {
            puts("checkForConsistency: not possible to appoint new notaries");
            return false;
        }
        // check that predecessor type 2 entry is correct
        CompleteID predecessorID = type2entry->getPredecessorID();
        CompleteID predecessorIDFirst = getFirstID(predecessorID);
        if (predecessorIDFirst != getLastEssentialEntryId(currentRefId))
        {
            puts("checkForConsistency: bad predecessor of type2entry");
            return false;
        }
        // check that application and currentRefId belong to the same lineage
        unsigned short lineage = type1entry->getLineage(currentRefId.getTimeStamp());
        CompleteID applicationId = getApplicationId(terminationIdFirst);
        if (lineage != type1entry->getLineage(applicationId.getTimeStamp()))
        {
            puts("checkForConsistency: wrong lineage for type2entry");
            return false;
        }
    }
    else if (underlyingType == 3)
    {
        if (!getTotalNotaryNr(pubKeyID).isZero()) return false;
        Type3Entry* type3entry = static_cast<Type3Entry*>(underlyingEntry);
        // check that there are no pending applications
        CompleteID keyID = type3entry->getOwnerID();
        keyID = getFirstID(keyID);
        list<CompleteID> idsList;
        CompleteID minApplId;
        if (getNotaryApplications(keyID, true, 0, minApplId, 2, idsList)>0) return false;
        if (getNotaryApplications(keyID, true, 1, minApplId, 2, idsList)>0) return false;
        if (getNotaryApplications(keyID, true, 2, minApplId, 2, idsList)>0) return false;
        if (getNotaryApplications(keyID, true, 3, minApplId, 2, idsList)>0) return false;
    }
    else if (underlyingType == 4)
    {
        Type4Entry* type4entry = static_cast<Type4Entry*>(underlyingEntry);
        TNtrNr totalNotNr = type1entry->getTotalNotaryNr(type4entry->getNotaryNr(), currentRefId.getTimeStamp());
        if (!totalNotNr.isGood() || getTotalNotaryNr(pubKeyID) != totalNotNr)
        {
            puts("checkForConsistency: bad totalNotNr");
            return false;
        }
        if (!isActingNotary(type4entry->getNotaryNr(), currentRefId.getTimeStamp()))
        {
            puts("checkForConsistency: notary not acting");
            return false;
        }
        CompleteID previousId = type4entry->getPreviousThreadEntry();
        CompleteID previousIdFirst = getFirstID(previousId);
        if (hasThreadSuccessor(previousIdFirst, currentRefId))
        {
            puts("checkForConsistency: preexisting thread successor");
            return false;
        }
        CompleteID applicationId = getApplicationId(previousIdFirst);
        if (!isActingNotary(type4entry->getNotaryNr(), applicationId.getTimeStamp()))
        {
            puts("checkForConsistency: notary not acting 2");
            return false;
        }
        if (hasTakenPartInThread(applicationId, pubKeyID, currentRefId))
        {
            puts("checkForConsistency: has taken part in thread already");
            return false;
        }
        unsigned long long timeLimit = (unsigned long long) getProcessingTime(applicationId);
        timeLimit *= 1000;
        timeLimit += previousIdFirst.getTimeStamp();
        if (timeLimit <= currentRefId.getTimeStamp())
        {
            puts("checkForConsistency: violated processing time");
            return false;
        }
        // check that application and currentRefId belong to the same lineage
        unsigned short lineage = type1entry->getLineage(currentRefId.getTimeStamp());
        if (lineage != type1entry->getLineage(applicationId.getTimeStamp()))
        {
            puts("checkForConsistency: bad lineage");
            return false;
        }
        // check that decisions thread initiation limit for notary is not violated
        if (previousIdFirst == applicationId)
        {
            checkForInitiatedThreadConflict(totalNotNr, currentRefId);
            unsigned short lineage = type1entry->getLineage(currentRefId.getTimeStamp());
            if (getInitiatedThreadsCount(totalNotNr) > type1entry->getMaxActing(lineage))
            {
                puts("checkForConsistency: initiated threads count violated");
                return false;
            }
        }
    }
    else if (underlyingType == 5)
    {
        if (underlyingEntry->getType() != 5) return false;
        Type5Or15Entry* type5or15entry = static_cast<Type5Or15Entry*>(underlyingEntry);
        if (getCurrencyOrOblId(type5or15entry).getNotary() > 0) return false;
        if (((Type5Entry*)type5or15entry)->getRefereeTenure() <= 0) return false;
    }
    else if (underlyingType == 6)
    {
        Type6Entry* type6Entry = static_cast<Type6Entry*>(underlyingEntry);
        if (type6Entry->getTenureStart() <= currentRefId.getTimeStamp()) return false;
        CompleteID currencyId = type6Entry->getCurrency();
        CompleteID currencyIdFirst = getFirstID(currencyId);
        unsigned long long maxTenureStart = (unsigned long long) getRefereeTenure(currencyIdFirst);
        maxTenureStart *= 1000;
        maxTenureStart += currentRefId.getTimeStamp();
        if (type6Entry->getTenureStart() >= maxTenureStart) return false;
        // get keyID
        CompleteID keyID = type6Entry->getOwnerID();
        keyID = getFirstID(keyID);
        // check that there are no pending applications
        list<CompleteID> idsList;
        CompleteID minApplId;
        if (getApplications("AR", keyID, true, currencyIdFirst, 0, minApplId, 2, idsList)>0) return false;
        if (getApplications("AR", keyID, true, currencyIdFirst, 1, minApplId, 2, idsList)>0) return false;
        if (getApplications("AR", keyID, true, currencyIdFirst, 2, minApplId, 2, idsList)>0) return false;
        // check that ref is not already acting at tenure start or tenure end
        unsigned long long minApplTime = (unsigned long long) getRefereeTenure(currencyIdFirst);
        minApplTime *= 3000;
        if (minApplTime<type6Entry->getTenureStart()) minApplTime = type6Entry->getTenureStart() - minApplTime;
        else minApplTime=0;
        minApplId = CompleteID(0,0,minApplTime);
        if (getApplications("AR", keyID, true, currencyIdFirst, 3, minApplId, 6, idsList)>0)
        {
            unsigned long long tenureEnd = (unsigned long long) getRefereeTenure(currencyIdFirst);
            tenureEnd *= 1000;
            tenureEnd += type6Entry->getTenureStart();
            list<CompleteID>::iterator it;
            for (it=idsList.begin(); it!=idsList.end(); ++it)
            {
                if (isActingReferee(keyID, *it, type6Entry->getTenureStart())) return false;
                if (isActingReferee(keyID, *it, tenureEnd)) return false;
            }
        }
    }
    else if (underlyingType == 7)
    {
        Type7Entry* type7entry = static_cast<Type7Entry*>(underlyingEntry);
        CompleteID currencyId = type7entry->getCurrency();
        CompleteID currencyIdFirst = getFirstID(currencyId);
        if (getEntryType(currencyIdFirst) != 5) return false;
        Type5Entry* type5entry = getTruncatedEntry(currencyIdFirst);
        if (type5entry==nullptr) return false;
        if (!type5entry->isGoodOpPrProcessingTime(type7entry->getProcessingTime()))
        {
            delete type5entry;
            return false;
        }
        if (!type5entry->isGoodFee(type7entry->getAmount(), type7entry->getFee()))
        {
            delete type5entry;
            return false;
        }
        double ownStake = type7entry->getOwnStake();
        if (ownStake > 0)
        {
            CompleteID claimEntryId = type7entry->getLiquidityClaim();
            CompleteID claimEntryIdFirst = getFirstID(claimEntryId);
            double carriedLiquidity = getCollectableTransferableLiqui(claimEntryIdFirst, pubKeyID,
                                      currencyIdFirst, getDiscountRate(currencyIdFirst), 0, currentRefId, 0);
            if (!isFreeClaim(claimEntryIdFirst, pubKeyID, currencyIdFirst, currentRefId) || carriedLiquidity < ownStake)
            {
                delete type5entry;
                return false;
            }
        }
        delete type5entry;
    }
    else if (underlyingType == 8)
    {
        Type8Entry* type8entry = static_cast<Type8Entry*>(underlyingEntry);
        // check if we have an acting referee
        CompleteID terminationID = type8entry->getTerminationID();
        CompleteID terminationIdFirst = getFirstID(terminationID);
        if (!isActingReferee(pubKeyID, terminationIdFirst, currentRefId.getTimeStamp())) return false;
        // check if the previous entry does not have a successor yet
        CompleteID previousId = type8entry->getPreviousThreadEntry();
        CompleteID previousIdFirst = getFirstID(previousId);
        if (hasThreadSuccessor(previousIdFirst, currentRefId)) return false;
        // check that this referee has not taken part yet (including being an applicant)
        CompleteID applicationId = getApplicationId(previousIdFirst);
        if (hasTakenPartInThread(applicationId, pubKeyID, currentRefId)) return false;
        // check that the time limit is not reached
        unsigned long long timeLimit = (unsigned long long) getProcessingTime(applicationId);
        timeLimit *= 1000;
        timeLimit += previousIdFirst.getTimeStamp();
        if (timeLimit <= currentRefId.getTimeStamp()) return false;
        // check that the stake can be "deposited"
        double stake = getRefereeStake(applicationId);
        if (stake == DBL_MAX) return false;
        CompleteID claimEntryId = type8entry->getLiquidityClaim();
        CompleteID claimEntryIdFirst = getFirstID(claimEntryId);
        CompleteID currencyId = getCurrencyId(applicationId);
        double carriedLiquidity = getCollectableLiquidity(claimEntryIdFirst, pubKeyID,
                                  currencyId, getDiscountRate(currencyId), 0, currentRefId, 0);
        if (!isFreeClaim(claimEntryIdFirst, pubKeyID, currencyId, currentRefId) || carriedLiquidity < stake) return false;
        // if this is the first entry after application check that referee has power
        if (previousIdFirst == applicationId)
        {
            if (latestApprovalInNotarization(pubKeyID, terminationIdFirst, currentRefId)) return false;
            double requestedLiquidity = getRequestedLiquidity(applicationId);
            if (requestedLiquidity > 0)
            {
                double liquidityLim = getLiquidityLimitPerRef(currencyId);
                double createdLiquidity = getLiquidityCreatedSoFar(pubKeyID, terminationIdFirst);
                if (createdLiquidity + requestedLiquidity > liquidityLim) return false;
            }
            else
            {
                unsigned short appointmentLim = getAppointmentLimitPerRef(currencyId);
                unsigned short appointedRefs = getRefsAppointedSoFar(pubKeyID, terminationIdFirst);
                if (appointedRefs >= appointmentLim) return false;
            }
        }
    }
    else if (underlyingType == 9)
    {
        Type9Entry* type9entry = static_cast<Type9Entry*>(underlyingEntry);
        CompleteID previousId = type9entry->getPreviousThreadEntry();
        CompleteID previousIdFirst = getFirstID(previousId);
        if (hasThreadSuccessor(previousIdFirst, currentRefId)) return false;
        CompleteID applicationId = getApplicationId(previousIdFirst);
        unsigned long notaryNr = getTotalNotaryNr(pubKeyID).getNotaryNr();
        if (!isActingNotary(notaryNr, currentRefId.getTimeStamp())) return false;
        unsigned long long terminationTime = (unsigned long long) getProcessingTime(applicationId);
        terminationTime *= 1000;
        terminationTime += previousIdFirst.getTimeStamp();
        if (terminationTime > currentRefId.getTimeStamp()) return false;
        if (type1entry->getLineage(previousIdFirst.getTimeStamp())
                != type1entry->getLineage(currentRefId.getTimeStamp()))
        {
            if (notaryNr != 1) return false; // must be the first notary if lineage break
        }
        else if (type1entry->getLineage(previousIdFirst.getTimeStamp()) == type1entry->latestLin())
        {
            unsigned long long scheduledTerminationTime = getTerminationTime(previousIdFirst);
            if (scheduledTerminationTime == 0)
            {
                storeT9eCreationParameters(previousIdFirst, terminationTime);
                scheduledTerminationTime = getTerminationTime(previousIdFirst);
            }
            if (scheduledTerminationTime == 0 || scheduledTerminationTime != terminationTime) return false;
            // make sure that notary fits the schedule
            unsigned long long timeDiff = currentRefId.getTimeStamp() - scheduledTerminationTime;
            unsigned long long timeStep = type1entry->getLatestMaxNotarizationTime();
            unsigned long c = timeDiff / timeStep;
            c = c % getNotariesListLength(previousIdFirst, false);
            unsigned long correctNotary = getNotaryInSchedule(previousIdFirst, c, false);
            if (notaryNr != correctNotary) return false;
        }
    }
    else if (underlyingType == 10)
    {
        Type10Entry* type10entry = static_cast<Type10Entry*>(underlyingEntry);
        double amount = type10entry->getTransferAmount();
        CompleteID currencyOrOblId = type10entry->getCurrencyOrObl();
        CompleteID currencyOrOblIdFirst = getFirstID(currencyOrOblId);
        // check currency or obligation
        CompleteID oblOwner = getOblOwner(currencyOrOblIdFirst);
        if (oblOwner.getNotary()<=0 && getRefereeTenure(currencyOrOblIdFirst)<=0) return false;
        // check lower transfer limit
        if (amount>0 && amount<getLowerTransferLimit(currencyOrOblIdFirst)) return false;
        // check source and determine type
        CompleteID sourceId = type10entry->getSource();
        CompleteID sourceIdFirst = getFirstID(sourceId);
        int sourceType; // 0 for transfer request, 1 for obligation printing, 2 for claim based
        if (getValidityDate(sourceIdFirst) > 0) // have type 11 entry as source
        {
            if (sourceIdFirst != pubKeyID) return false;
            if (amount==0)
            {
                if (currencyOrOblId != type10entry->getTarget()) return false;
                sourceType = 0;
            }
            else
            {
                if (oblOwner != pubKeyID || amount<=0) return false;
                sourceType = 1;
            }
        }
        else
        {
            sourceType = 2;
            if (amount<=0) return false;
            if (!isFreeClaim(sourceIdFirst, pubKeyID, currencyOrOblIdFirst, currentRefId))
            {
                puts("checkForConsistency: claim not free");
                return false;
            }
            double carriedLiquidity = getCollectableTransferableLiqui(sourceIdFirst, pubKeyID,
                                      currencyOrOblIdFirst, getDiscountRate(currencyOrOblIdFirst), 0, currentRefId, 0);
            if (carriedLiquidity < amount)
            {
                puts("checkForConsistency: carriedLiquidity insufficient");
                return false;
            }
        }
        // check target
        CompleteID targetId = type10entry->getTarget();
        if (targetId.getNotary()<=0) return (sourceType==2); // deletion
        CompleteID targetIdFirst = getFirstID(targetId);
        if (sourceType == 1 && targetIdFirst != sourceIdFirst) return false; // printing for owner only
        double requested = type10entry->getRequestedAmount();
        if (getValidityDate(targetIdFirst)>0) // have a type 11 entry as target
        {
            if (sourceType==0 || requested>0) return false;
        }
        else if (getOblOwner(targetIdFirst).getNotary()>0
                 || getRefereeTenure(targetIdFirst)>0) // have a type 5 or 15 entry as target
        {
            if (requested==0) return false;
            if (requested<getLowerTransferLimit(targetIdFirst)) return false;
        }
        else if (getConnectedTransfer(targetIdFirst, currentRefId).getNotary()>0) // have a type 10 entry as target
        {
            // load other type 10 entry
            if (amount<=0) return false;
            if (getEntryType(targetIdFirst) != 10) return false;
            Type12Entry* otherT10eWrap = buildUnderlyingEntry(targetIdFirst,0);
            if (otherT10eWrap==nullptr) return false;
            // check that the other entry is still available
            CompleteID claimForTheOtherTransfer = getTransferCollectingClaim(targetIdFirst, currentRefId);
            CompleteID connectedTransfer = getConnectedTransfer(targetIdFirst, currentRefId);
            if (claimForTheOtherTransfer.getNotary()>0 || connectedTransfer!=targetIdFirst)
            {
                delete otherT10eWrap;
                return false;
            }
            // continue loading
            if (otherT10eWrap->underlyingEntry()==nullptr || otherT10eWrap->underlyingEntry()->getType()!=10)
            {
                delete otherT10eWrap;
                return false;
            }
            Type10Entry* otherT10e = (Type10Entry*) otherT10eWrap->underlyingEntry();
            // check that the two type 10 entries are consistent
            CompleteID otherTarget = otherT10e->getTarget();
            if (currencyOrOblIdFirst != getFirstID(otherTarget) || amount != otherT10e->getRequestedAmount()
                    || requested != otherT10e->getTransferAmount())
            {
                delete otherT10eWrap;
                return false;
            }
            delete otherT10eWrap;
        }
        else return false;
    }
    else if (underlyingType == 14)
    {
        Type14Entry* type14entry = static_cast<Type14Entry*>(underlyingEntry);
        // check that the predecessor claim is the correct one
        CompleteID currencyOrOblId = type14entry->getCurrencyOrObligation();
        CompleteID currencyOrOblIdFirst = getFirstID(currencyOrOblId);
        unsigned long long claimCount = getClaimCount(pubKeyID, currencyOrOblIdFirst);
        if (claimCount <= 0)
        {
            // verify that there is indeed no first claim (e.g. in notarization)
            CompleteID firstClaim = getClaim(pubKeyID, currencyOrOblIdFirst, 0, currentRefId);
            if (firstClaim.getNotary()>0)
            {
                puts("checkForConsistency, type 14: firstClaim exists");
                return false;
                // the above should not trigger, but checking is important for conflicts
            }
            // verify that type14entry has no predecessor
            if (type14entry->hasPredecessorClaim())
            {
                puts("checkForConsistency, type 14: unexpected predecessor");
                return false;
            }
        }
        else
        {
            // verify that there is no later claim (e.g. in notarization)
            CompleteID laterClaim = getClaim(pubKeyID, currencyOrOblIdFirst, claimCount, currentRefId);
            if (laterClaim.getNotary()>0)
            {
                puts("checkForConsistency, type 14: laterClaim exists");
                return false;
                // the above should not trigger, but checking is important for conflicts
            }
            // check that predecessor is set
            if (!type14entry->hasPredecessorClaim())
            {
                puts("checkForConsistency, type 14: missing predecessor");
                puts(pubKeyID.to27Char().c_str());
                puts(currencyOrOblIdFirst.to27Char().c_str());
                puts(to_string(claimCount).c_str());
                puts(currentRefId.to27Char().c_str());
                puts(getClaim(pubKeyID, currencyOrOblIdFirst, 0, currentRefId).to27Char().c_str());
                return false;
            }
            CompleteID predecessorClaimId = type14entry->getPredecessorClaim();
            CompleteID predecessorClaimIdFirst = getFirstID(predecessorClaimId);
            CompleteID predecessorClaimId2 = getClaim(pubKeyID, currencyOrOblIdFirst, claimCount-1, currentRefId);
            CompleteID predecessorClaimId2First = getFirstID(predecessorClaimId2);
            if (predecessorClaimId2First != predecessorClaimIdFirst)
            {
                puts("checkForConsistency, type 14: bad predecessor");
                puts(pubKeyID.to27Char().c_str());
                puts(currencyOrOblIdFirst.to27Char().c_str());
                puts(to_string(claimCount).c_str());
                puts(currentRefId.to27Char().c_str());
                for (unsigned short j=0; j<claimCount; j++) puts(getClaim(pubKeyID, currencyOrOblIdFirst, j, currentRefId).to27Char().c_str());
                return false;
            }
            // check that transfer successor is considered
            CompleteID transferID = getOutflowEntry(predecessorClaimIdFirst, currentRefId);
            if (transferID.getNotary() <= 0)
            {
                puts("checkForConsistency, type 14: missing transferID");
                return false;
            }
            if (transferID != predecessorClaimIdFirst)
            {
                list<CompleteID>* outflows = type14entry->getOutflowEntries();
                if (outflows->size()<=0)
                {
                    puts("checkForConsistency, type 14: missing outflows");
                    return false;
                }
                bool considered = false;
                list<CompleteID>::iterator it;
                for (it=outflows->begin(); !considered && it!=outflows->end(); ++it)
                {
                    considered = (transferID == getFirstID(*it));
                }
                if (!considered)
                {
                    puts("checkForConsistency, type 14: outflow not considered");
                    return false;
                }
            }
        }
        // run through all predecessors
        const unsigned short predecessorCount = type14entry->getPredecessorsCount();
        double totalImpact = 0;
        double totalNonTransfer = 0;
        double discountRate = getDiscountRate(currencyOrOblIdFirst);
        double maxAbsImpact = 0;
        for (unsigned short i=0; i<predecessorCount; i++)
        {
            unsigned char scenario = type14entry->getScenario(i);
            CompleteID predecessorId = type14entry->getPredecessor(i);
            // check for freshness
            if (!type1entry->isFresh(predecessorId, currentRefId.getTimeStamp()) && !freshByDefi(predecessorId)
                    && scenario != 2 && scenario != 5 && scenario != 6)
            {
                puts("checkForConsistency, type 14: predecessorId not fresh:");
                puts(predecessorId.to27Char().c_str());
                return false;
            }
            // check for claim conflict
            CompleteID predecessorIdFirst = getFirstID(predecessorId);
            CompleteID conflictingClaim = getCollectingClaim(predecessorIdFirst, pubKeyID, currencyOrOblIdFirst,
                                          scenario, currentRefId);
            if (conflictingClaim.getNotary()>0)
            {
                puts("checkForConsistency, type 14: conflictingClaim found");
                return false;
            }
            // check claim amount
            double carriedAmount = getCollectableLiquidity(predecessorIdFirst, pubKeyID,
                                   currencyOrOblIdFirst, discountRate, scenario, currentRefId, 0);
            if (!util.roughlyTheSameW(type14entry->getImpact(i), carriedAmount))
            {
                puts("checkForConsistency, type 14: roughlyTheSameW failed:");
                puts(to_string(carriedAmount).c_str());
                puts(to_string(type14entry->getImpact(i)).c_str());
                CompleteID zeroID;
                if (getConnectedTransfer(predecessorIdFirst, zeroID) == predecessorIdFirst)
                    insertEntryToDownload(predecessorIdFirst, 0, 0); // re-download transaction if waiting for connection
                return false;
            }
            totalImpact += carriedAmount;
            totalNonTransfer += getCollectableNonTransferLiqui(predecessorIdFirst, pubKeyID,
                                currencyOrOblIdFirst, discountRate, scenario, currentRefId, 0);
            maxAbsImpact = max(maxAbsImpact, abs(carriedAmount));
        }
        if (totalImpact<0) totalImpact=0;
        if (totalNonTransfer<0) totalNonTransfer=0;
        if (totalNonTransfer>totalImpact) totalNonTransfer=totalImpact;

        if (!util.roughlyLargerOrEqual(totalImpact, type14entry->getTotalAmount(), maxAbsImpact)) return false;
        if (!util.roughlyLargerOrEqual(totalNonTransfer, type14entry->getNonTransferAmount(), maxAbsImpact)) return false;
        if (!util.roughlyLargerOrEqual(totalImpact-totalNonTransfer, type14entry->getTransferableAmount(), maxAbsImpact)) return false;
    }
    else if (underlyingType == 15)
    {
        if (underlyingEntry->getType() != 15) return false;
        Type5Or15Entry* type5or15entry = static_cast<Type5Or15Entry*>(underlyingEntry);
        if (getCurrencyOrOblId(type5or15entry).getNotary() > 0) return false;
    }
    else return false;

    return true;
}

// db must be locked for this
size_t Database::getClaims(CompleteID &id, CompleteID &currencyId, CompleteID &maxClaimId, unsigned short &maxClaimsNum, list<CompleteID> &idsList)
{
    if (!isInGeneralList(id) || !idsList.empty() || !isInGeneralList(currencyId))
    {
        puts("Database::getClaims: bad parameters supplied");
        return 0;
    }
    CompleteID firstKeyId = getFirstID(id);
    CompleteID zeroId;
    // find largest possible k via binary search
    unsigned long long a=0;
    unsigned long long b=getClaimCount(firstKeyId, currencyId);
    if (b==0)
    {
        puts("Database::getClaims: zero claim count");
        return 0;
    }
    else b--;
    while (b-a>1)
    {
        unsigned long long m = (a+b)/2;
        CompleteID claimId = getClaim(firstKeyId, currencyId, m, zeroId);
        if (claimId<maxClaimId) a=m;
        else b=m;
    }
    if (getClaim(firstKeyId, currencyId, b, zeroId) > maxClaimId) b=a;
    if (getClaim(firstKeyId, currencyId, b, zeroId) > maxClaimId) return 0;
    // add maxClaimsNum claims beginning with b
    for (unsigned short i=0; i<maxClaimsNum && i<=b; i++)
    {
        CompleteID claimId = getClaim(firstKeyId, currencyId, b-i, zeroId);
        if (claimId.getNotary()>0) idsList.push_front(claimId);
        else puts("Database::getClaims: bad claimId");
    }
    return idsList.size();
}

// db must be locked for this
size_t Database::getRelatedEntries(CompleteID &id, list<CompleteID> &idsList)
{
    if (!isInGeneralList(id) || !idsList.empty()) return 0;
    CompleteID idFirst = getFirstID(id);
    unsigned short type = getEntryType(idFirst);

    if (type == 2)
    {
    }
    else if (type == 5)
    {
        idsList.push_back(idFirst);
    }
    else if (type == 3 || type == 4 || type == 6 || type == 7 || type == 8 || type == 9)
    {
        CompleteID runningId = getApplicationId(idFirst);
        CompleteID zeroId;
        while (isInGeneralList(runningId))
        {
            idsList.push_back(getLatestID(runningId));
            runningId = getThreadSuccessor(runningId, zeroId);
        }
    }
    else if (type == 10)
    {
        idsList.push_back(getLatestID(idFirst));
        CompleteID zeroId;
        CompleteID collectingClaim = getTransferCollectingClaim(idFirst, zeroId);
        CompleteID connectedTransfer = getConnectedTransfer(idFirst, zeroId);
        if (collectingClaim.getNotary()>0) idsList.push_back(getLatestID(collectingClaim));
        if (connectedTransfer != idFirst) idsList.push_back(getLatestID(connectedTransfer));
    }
    else if (type == 11)
    {
        idsList.push_back(getLatestID(idFirst));
        CompleteID validityDateEnryId = getValidityDateEntry(idFirst);
        idsList.push_back(getLatestID(validityDateEnryId));
    }
    else if (type == 14)
    {
    }
    else if (type == 15)
    {
        idsList.push_back(idFirst);
    }

    return idsList.size();
}

// db must be locked for this
CompleteID Database::getCurrencyOrOblId(Type5Or15Entry* entry)
{
    if (entry == nullptr) return CompleteID();
    string key;
    string value;
    key.push_back('B'); // prefix to identification by byte sequence
    string *charaByteSeq = entry->getByteSeq();
    key.append(util.UlAsByteSeq(charaByteSeq->length()));
    key.append(*charaByteSeq);
    rocksdb::Status s = currenciesAndObligations->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID out(value);
    return out;
}

// db must be locked for this
unsigned long Database::getSignatureCount(CompleteID &signId)
{
    unsigned long out=0;
    for (unsigned char i=0; i<3 && out<=0; i++)
        out = getSignatureCount(signId, i);
    return out;
}

// db must be locked for this
unsigned long Database::getSignatureCount(CompleteID &signId, bool renot)
{
    unsigned char l = 1;
    if (renot) l = 2;
    return getSignatureCount(signId, l);
}

// db must be locked for this
unsigned long Database::getSignatureCount(CompleteID &signId, unsigned char l)
{
    string key;
    string value;
    rocksdb::Status s;
    if (l==0)
    {
        key.push_back('B'); // prefix to distinguish from list head
        key.append(signId.to20Char());
        key.append("SL");
        key.push_back('H');
        s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    }
    else if (l==1)
    {
        key.append(signId.to20Char());
        key.append("SC"); // suffix for signature count leading to this type 13 entry
        s = entriesInNotarization->Get(rocksdb::ReadOptions(), key, &value);
    }
    else
    {
        key.push_back('I'); // prefix for in renotarization
        key.append(signId.to20Char());
        key.append("SC"); // suffix for signature count leading to this type 13 entry
        s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
    }
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUl(value);
}

// db must be locked for this
CompleteID Database::getFirstNotSignId(CompleteID &signId, bool renot)
{
    string keyPref;
    rocksdb::DB* dbList = entriesInNotarization;
    if (renot)
    {
        keyPref.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;
    }
    string key;
    string value;
    key.append(keyPref);
    key.append(signId.to20Char());
    key.push_back('F'); // suffix for first id
    rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID out(value);
    return out;
}

// db must be locked for this
unsigned long long Database::getNotTimeLimit(CompleteID &firstSignId, bool renot)
{
    string key;
    string value;
    rocksdb::Status s;
    if (!renot)
    {
        key.append(firstSignId.to20Char());
        key.append("TL"); // suffix for time limit
        s = entriesInNotarization->Get(rocksdb::ReadOptions(), key, &value);
    }
    else
    {
        key.push_back('I'); // prefix for in renotarization
        key.append(firstSignId.to20Char());
        key.append("TL"); // suffix for time limit
        s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
    }
    const bool success = (s.ok() && value.length()>2);
    if (!success) return 0;
    return util.byteSeqAsUll(value);
}

// db must be locked for this
CompleteID Database::getFirstID(Type12Entry* type12entry)
{
    string key;
    string value;
    key.push_back('C'); // prefix for characterizing byte sequence
    string *charaByteSeq = type12entry->getByteSeq();
    key.append(util.UllAsByteSeq(charaByteSeq->length()));
    key.append(*charaByteSeq);
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID firstId(value);
    return firstId;
}

// db must be locked for this
bool Database::loadSigningNotaries(CompleteID &notEntryId, list<unsigned long> &notariesList)
{
    if (notariesList.size()!=0) return false;
    list<Type13Entry*> targetList;
    bool success = loadType13Entries(notEntryId, targetList);
    list<Type13Entry*>::iterator it;
    for (it=targetList.begin(); it!=targetList.end(); ++it)
    {
        if (success) notariesList.push_back((*it)->getCompleteID().getNotary());
        delete *it;
    }
    return success;
}

// db must be locked for this
bool Database::loadType13Entries(CompleteID &notEntryId, list<Type13Entry*> &targetList)
{
    if (targetList.size()!=0) return false;
    string keyPrefPref;
    keyPrefPref.push_back('B');
    keyPrefPref.append(notEntryId.to20Char());
    string key;
    string value;
    // get nr of signatures
    key = "";
    key.append(keyPrefPref);
    key.append("SL");
    key.push_back('H');
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    if (!(s.ok() && value.length()>2)) return false;
    const unsigned long nr = util.byteSeqAsUl(value);
    // load signatures
    for (unsigned long i=0; i<nr; i++)
    {
        value = "";
        key = "";
        key.append(keyPrefPref);
        key.append("SL");
        key.push_back('B'); // suffix for body of signatures list
        key.append(util.UlAsByteSeq(i));
        s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
        if (!(s.ok() && value.length()>2)) return false;
        Type13Entry *t13e = new Type13Entry(value);
        if (!t13e->isGood())
        {
            puts("Database::loadType13Entries: bad t13e");
            delete t13e;
            return false;
        }
        targetList.push_back(t13e);
    }
    // load confirmation entry if existent
    value = "";
    key = "";
    key.append(keyPrefPref);
    key.append("CE");
    s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    if (!(s.ok() && value.length()>2)) return true;
    Type13Entry *t13e = new Type13Entry(value);
    if (!t13e->isGood())
    {
        puts("Database::loadType13Entries: bad confirmation entry");
        delete t13e;
        return false;
    }
    targetList.push_back(t13e);
    return true;
}

// db must be locked for this
void Database::saveConfirmationEntry(CompleteID &firstSignId, Type13Entry *confEntry)
{
    if (confEntry==nullptr || firstSignId.getNotary()<=0)
    {
        return;
    }
    string key = "";
    key.push_back('B');
    key.append(firstSignId.to20Char());
    key.append("CE"); // confirmation entry
    notarizationEntries->Put(rocksdb::WriteOptions(), key, *confEntry->getByteSeq());
}

// db must be locked for this
bool Database::integrateNewNotEntry(CompleteID &firstSignId, bool renot, Type13Entry *confEntry)
{
    // save to general list
    string keyPref;
    rocksdb::DB* dbList = entriesInNotarization;
    if (renot)
    {
        keyPref.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;
    }

    string key;
    string value;

    CompleteID firstId;

    // obtain signature entries as strings list
    list<string> type13entriesStr;
    Type12Entry *type12entry = nullptr;
    CompleteID nextID = firstSignId;
    bool first = true;
    string keyPrefPref;
    while (nextID.getNotary() > 0)
    {
        keyPrefPref = "";
        keyPrefPref.append(keyPref);
        keyPrefPref.append(nextID.to20Char());

        // get actual entry
        key = "";
        key.append(keyPrefPref);
        key.append("AE"); // suffix for entry
        value = "";
        rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
        bool success = (s.ok() && value.length()>2);
        if (!success) return false;
        type13entriesStr.insert(type13entriesStr.end(), value);

        if (first)
        {
            if (renot)
            {
                key = "";
                key.append(keyPrefPref);
                key.push_back('P'); // suffix for previous type 13 entry
                value = "";
                s = dbList->Get(rocksdb::ReadOptions(), key, &value);
                success = (s.ok() && value.length()>2);
                if (!success) return false;
                CompleteID id(value);
                firstId = getFirstID(id);
            }
            else
            {
                type12entry = createT12FromT13Str(value);
                if (type12entry == nullptr) return false;
                if (!type12entry->isGood())
                {
                    delete type12entry;
                    return false;
                }
                firstId = firstSignId;
            }
            first = false;
        }

        // get reference to next
        key = "";
        key.append(keyPrefPref);
        key.push_back('N'); // suffix for next type 13 entry
        value = "";
        s = dbList->Get(rocksdb::ReadOptions(), key, &value);
        success = (s.ok() && value.length()>2);
        if (!success) nextID = CompleteID();
        else nextID = CompleteID(value);
    }

    // determine firstId
    if (!renot)
    {
        // save firstId for getFirstIdByFirstSigId
        key = "";
        key.push_back('C'); // prefix for characterizing byte sequence
        string *charaByteSeq = type12entry->getByteSeq();
        key.append(util.UllAsByteSeq(charaByteSeq->length()));
        key.append(*charaByteSeq);
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());

        keyPrefPref = "";
        keyPrefPref.push_back('B');
        keyPrefPref.append(firstId.to20Char());

        // create LN
        key = "";
        key.append(keyPrefPref);
        key.append("LN");
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());

        // create UET
        key = "";
        key.append(keyPrefPref);
        key.append("UET");
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(type12entry->underlyingType()));
    }
    else
    {
        // update LN for firstId
        key = "";
        key.push_back('B');
        key.append(firstId.to20Char());
        key.append("LN");
        notarizationEntries->Put(rocksdb::WriteOptions(), key, getLatestID(firstId).maximum(firstSignId).to20Char());
    }

    keyPrefPref = "";
    keyPrefPref.push_back('B');
    keyPrefPref.append(firstSignId.to20Char());

    // save FN
    key = "";
    key.append(keyPrefPref);
    key.append("FN");
    notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());

    // save signatures head
    key = "";
    key.append(keyPrefPref);
    key.append("SL");
    key.push_back('H');
    notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(type13entriesStr.size()));

    // save signatures body
    unsigned int c = 0;
    list<string>::iterator it;
    for (it=type13entriesStr.begin(); it!=type13entriesStr.end(); ++it)
    {
        key = "";
        key.append(keyPrefPref);
        key.append("SL");
        key.push_back('B');
        key.append(util.UlAsByteSeq(c));
        notarizationEntries->Put(rocksdb::WriteOptions(), key, *it);
        c++;
        // save actual entry
        if (it==type13entriesStr.begin())
        {
            key = "";
            key.append(keyPrefPref);
            key.append("AE");
            notarizationEntries->Put(rocksdb::WriteOptions(), key, *it);
        }
    }

    // save confirmation entry
    if (confEntry != nullptr)
    {
        saveConfirmationEntry(firstSignId, confEntry);
    }

    if (renot)
    {
        if (type12entry!=nullptr) delete type12entry;
        return true;
    }

    int underlyingType = type12entry->underlyingType();

    if (underlyingType == 2)
    {
        // get entry
        Type2Entry* type2entry = static_cast<Type2Entry*>(type12entry->underlyingEntry());
        CompleteID ownerID = type12entry->pubKeyID();
        CompleteID pubKeyID = getFirstID(ownerID);
        string *pubKey = type2entry->getPublicKey();
        // get total notary number
        unsigned short lineage = type1entry->getLineage(firstId.getTimeStamp());
        TNtrNr totalNotaryNr(lineage, getNrOfNotariesInLineage(lineage)+1);
        string tNtrNrStr = totalNotaryNr.toString();

        // save new NrOfNotariesInLineage
        key = "";
        key.push_back('H'); // prefix for list head (nr of entries)
        key.append(util.UsAsByteSeq(lineage));
        notaries->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(totalNotaryNr.getNotaryNr()));

        // save tNtrNrStr
        key = "";
        key.push_back('K'); // prefix for identification by public key
        key.append(util.UlAsByteSeq(pubKey->length()));
        key.append(*pubKey);
        notaries->Put(rocksdb::WriteOptions(), key, tNtrNrStr);

        // save total notary number to public keys
        key = "";
        key.push_back('I'); // suffix for key identification by id
        key.append(pubKeyID.to20Char());
        key.append("TN"); // suffix for total notary number
        publicKeys->Put(rocksdb::WriteOptions(), key, totalNotaryNr.toString());

        // save publicKeyId to notaries list
        key = "";
        key.push_back('B'); // prefix for body
        key.append(totalNotaryNr.toString());
        key.append("ID"); // prefix for id
        notaries->Put(rocksdb::WriteOptions(), key, pubKeyID.to20Char());

        // save public key to notaries list
        key = "";
        key.push_back('B'); // prefix for body
        key.append(totalNotaryNr.toString());
        key.append("PK"); // prefix for public key
        notaries->Put(rocksdb::WriteOptions(), key, *pubKey);

        // save share to keep
        key = "";
        key.push_back('B'); // prefix for body
        key.append(totalNotaryNr.toString());
        key.append("SK"); // prefix for share to keep
        notaries->Put(rocksdb::WriteOptions(), key,  util.DblAsByteSeq(1));

        // store t2e id to notaries
        key = "";
        key.push_back('B'); // prefix for body
        key.append(totalNotaryNr.toString());
        key.append("T2E"); // prefix for type 2 entry
        notaries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());

        delete type12entry;

        // make sure that notary is not missed as abstainer:
        string prefix;
        prefix.push_back('B'); // prefix for body
        prefix.append(util.UcAsByteSeq(5));
        unsigned long prefLength = prefix.length();
        // define min key
        string keyMin(prefix);
        keyMin.append(firstId.to20Char());
        // go through relevant notary applications
        rocksdb::Iterator* it = notaryApplications->NewIterator(rocksdb::ReadOptions());
        for (it->Seek(keyMin); it->Valid(); it->Next())
        {
            // check the prefix
            string keyStr = it->key().ToString();
            if (keyStr.length()<=prefLength) break;
            if (keyStr.substr(0, prefLength).compare(keyPref) != 0) break;
            // get entry id
            string idStr = keyStr.substr(prefLength, keyStr.length()-prefLength);
            CompleteID terminationId(idStr);
            // get application id
            CompleteID applicationId = getApplicationId(terminationId);
            // reconstruct opposing and supporting
            set<TNtrNr, TNtrNr::CompareTNtrNrs> supportingNotaries;
            set<TNtrNr, TNtrNr::CompareTNtrNrs> opposingNotaries;
            size_t noteCount = 0;
            CompleteID firstNoteId;
            CompleteID zeroId;
            CompleteID runningId = getThreadSuccessor(applicationId, zeroId);
            while (isInGeneralList(runningId) && runningId!=terminationId)
            {
                if (firstNoteId.isZero()) firstNoteId=runningId;

                CompleteID notaryId = getThreadParticipantId(runningId);
                if (notaryId.getNotary()<=0) break;
                notaryId = getFirstID(notaryId);
                TNtrNr ttlNotaryNr = getTotalNotaryNr(notaryId);

                noteCount++;
                if (isActingNotary(ttlNotaryNr.getNotaryNr(), terminationId.getTimeStamp()))
                {
                    if (noteCount % 2 == 1)
                    {
                        supportingNotaries.insert(ttlNotaryNr);
                    }
                    else
                    {
                        opposingNotaries.insert(ttlNotaryNr);
                    }
                }
                runningId = getThreadSuccessor(runningId, zeroId);
            }
            // redistribute fees if abstainer
            if (!firstNoteId.isZero() && noteCount % 2 == 1
                    && supportingNotaries.count(totalNotaryNr)<=0 && opposingNotaries.count(totalNotaryNr)<=0
                    && isActingNotary(totalNotaryNr.getNotaryNr(), terminationId.getTimeStamp()))
            {
                double penaltyFactor = type1entry->getPenaltyFactor2(lineage);
                decreaseShareToKeep(totalNotaryNr, penaltyFactor);
                set<TNtrNr>::iterator it2;
                for (it2=supportingNotaries.begin(); it2!=supportingNotaries.end(); ++it2)
                {
                    TNtrNr winner = *it2;
                    addToRedistributionMultiplier(totalNotaryNr, winner, penaltyFactor);
                }
            }
        }
        delete it;
        return true;
    }
    else if (underlyingType == 3)
    {
        CompleteID ownerID = type12entry->pubKeyID();
        CompleteID pubKeyID = getFirstID(ownerID);
        // save to notary applications
        key = "";
        key.push_back('B'); // prefix for body
        key.append(util.UcAsByteSeq(0));
        key.append(firstSignId.to20Char());
        notaryApplications->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        // save to public keys
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(pubKeyID.to20Char());
        key.append("AN"); // prefix for notary application
        key.append(util.UcAsByteSeq(0));
        key.append(firstSignId.to20Char());
        publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        // save thread info
        string keyP="";
        keyP.push_back('B'); // prefix to distinguish from list head
        keyP.append(firstSignId.to20Char());
        keyP.push_back('T'); // suffix for thread entry

        key=keyP;
        key.push_back('F'); // suffix for application entry
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        key=keyP;
        key.push_back('P'); // suffix for participant id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, pubKeyID.to20Char());

        unsigned short lineage = type1entry->getLineage(firstSignId.getTimeStamp());
        unsigned long processingTime = type1entry->getProposalProcessingTime(lineage);
        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("PT"); // suffix for processing time
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(processingTime));

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("TP"); // suffix for thread participants list. Output: ID of respective thread entry
        key.append(pubKeyID.to20Char());
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("TS"); // suffix for thread status
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UcAsByteSeq(0));

        // schedule termination
        unsigned long long terminationTime = processingTime;
        terminationTime *= 1000;
        terminationTime += firstSignId.getTimeStamp();
        key="E"; // prefix for expected terminations
        key.append(util.flip(util.UllAsByteSeq(terminationTime)));
        key.append(firstSignId.to20Char());
        scheduledActions->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        delete type12entry;
        return true;
    }
    else if (underlyingType == 4)
    {
        // get entry
        Type4Entry* type4entry = static_cast<Type4Entry*>(type12entry->underlyingEntry());
        CompleteID ownerID = type12entry->pubKeyID();
        CompleteID pubKeyID = getFirstID(ownerID);
        CompleteID predecessorId = type4entry->getPreviousThreadEntry();
        CompleteID predecessorIdFirst = getFirstID(predecessorId);
        CompleteID applicationId = getApplicationId(predecessorIdFirst);

        // update thread entry data
        string keyP="";
        keyP.push_back('B'); // prefix to distinguish from list head
        keyP.append(firstSignId.to20Char());
        keyP.push_back('T'); // suffix for thread entry

        key=keyP;
        key.push_back('F'); // suffix for application entry
        notarizationEntries->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());

        key=keyP;
        key.push_back('P'); // suffix for participant id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, pubKeyID.to20Char());

        // update application entry data
        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(applicationId.to20Char());
        key.push_back('T'); // suffix for thread entry
        key.push_back('A'); // suffix for application entry information
        key.append("TP"); // suffix for thread participants list. Output: ID of respective thread entry
        key.append(pubKeyID.to20Char());
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        // update thread status everywhere
        unsigned char oldThreadStatus = getThreadStatus(applicationId);
        unsigned char newThreadStatus = oldThreadStatus + 1;
        if (newThreadStatus>2) newThreadStatus = 1;

        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(applicationId.to20Char());
        key.push_back('T'); // suffix for thread entry
        key.push_back('A'); // suffix for application entry information
        key.append("TS"); // suffix for thread status
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UcAsByteSeq(newThreadStatus));

        key = "";
        key.push_back('B'); // prefix for body
        key.append(util.UcAsByteSeq(newThreadStatus));
        key.append(applicationId.to20Char());
        notaryApplications->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
        key = "";
        key.push_back('B'); // prefix for body
        key.append(util.UcAsByteSeq(oldThreadStatus));
        key.append(applicationId.to20Char());
        notaryApplications->Delete(rocksdb::WriteOptions(), key);

        CompleteID applicantId = getThreadParticipantId(applicationId);
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(applicantId.to20Char());
        key.append("AN"); // prefix for notary application
        key.append(util.UcAsByteSeq(newThreadStatus));
        key.append(applicationId.to20Char());
        publicKeys->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(applicantId.to20Char());
        key.append("AN"); // prefix for notary application
        key.append(util.UcAsByteSeq(oldThreadStatus));
        key.append(applicationId.to20Char());
        publicKeys->Delete(rocksdb::WriteOptions(), key);

        CompleteID zeroId;
        CompleteID runningId = getThreadSuccessor(applicationId, zeroId);
        while (isInGeneralList(runningId))
        {
            CompleteID notaryId = getThreadParticipantId(runningId);
            if (notaryId.getNotary()<=0) break;
            notaryId = getFirstID(notaryId);
            TNtrNr totalNotaryNr = getTotalNotaryNr(notaryId);

            key="";
            key.push_back('B'); // prefix for body
            key.append(totalNotaryNr.toString());
            key.append("AN"); // suffix for notary applications
            key.append(util.UcAsByteSeq(newThreadStatus));
            key.append(applicationId.to20Char());
            notaries->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());

            key="";
            key.push_back('B'); // prefix for body
            key.append(totalNotaryNr.toString());
            key.append("AN"); // suffix for notary applications
            key.append(util.UcAsByteSeq(oldThreadStatus));
            key.append(applicationId.to20Char());
            notaries->Delete(rocksdb::WriteOptions(), key);

            runningId = getThreadSuccessor(runningId, zeroId);
        }

        // schedule termination
        unsigned long long terminationTime = getProcessingTime(applicationId);
        terminationTime *= 1000;
        terminationTime += firstSignId.getTimeStamp();
        key="E"; // prefix for expected terminations
        key.append(util.flip(util.UllAsByteSeq(terminationTime)));
        key.append(firstSignId.to20Char());
        scheduledActions->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        deleteT9eCreationParameters(predecessorIdFirst);

        // increase initiated threads count if new thread
        if (predecessorIdFirst == applicationId)
        {
            TNtrNr totalNotaryNr = getTotalNotaryNr(pubKeyID);
            unsigned short threadsCount = getInitiatedThreadsCount(totalNotaryNr);
            threadsCount++;
            setInitiatedThreadsCount(totalNotaryNr, threadsCount);
        }

        delete type12entry;
        return true;
    }
    else if (underlyingType == 5)
    {
        key="";
        key.push_back('B'); // prefix for body
        key.append(firstSignId.to20Char());
        perpetualEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        Type5Entry* type5entry = static_cast<Type5Entry*>(type12entry->underlyingEntry());

        // save char seq to currenciesAndObligations
        key = "";
        key.push_back('B'); // prefix to identification by byte sequence
        string *charaByteSeq = type5entry->getByteSeq();
        key.append(util.UlAsByteSeq(charaByteSeq->length()));
        key.append(*charaByteSeq);
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        // save truncated byte seq
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("TB"); // suffix for truncated byte seq
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, *(type5entry->getByteSeqTrunc()));

        // save referee tenure
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("RT"); // suffix for referee tenure
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(type5entry->getRefereeTenure()));

        // save liquidity limit per ref
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("LL"); // suffix for liquidity limit
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type5entry->getLiquidityCreationLimit()));

        // save ref appointment limit
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("RL"); // suffix for referee limit
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(type5entry->getRefAppointmentLimit()));

        // save discount rate
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("DR"); // suffix for discount rate
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type5entry->getRate()));

        // save initial liquidity
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("IL"); // suffix for initial liquidity
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type5entry->getInitialLiquidity()));

        // save parent share
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("PS"); // suffix for parent share
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type5entry->getParentShare()));

        // save min transfer amount
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("MT"); // suffix for minimal transfer amount
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type5entry->getMinTransferAmount()));

        // save initial liquidity for first ref
        CompleteID refId = type5entry->getOwnerID();
        CompleteID refIdFirst = getFirstID(refId);
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(refIdFirst.to20Char());
        key.append("TC"); // to claim asap
        key.append(firstSignId.to20Char());
        key.append(firstSignId.to20Char());
        key.append(util.UcAsByteSeq(4));
        publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        addToTTLiquidityNotClaimedYet(refIdFirst, firstSignId, 4, true, type5entry->getInitialLiquidity());
        addToTTLiquidityNotClaimedYet(refIdFirst, firstSignId, 4, false, type5entry->getInitialLiquidity());

        // mark first ref as acting
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(refIdFirst.to20Char());
        key.push_back('R'); // suffix for referee information
        key.push_back('T'); // suffix for termination entry
        key.append(firstSignId.to20Char());
        key.push_back('T'); // suffix for tenure start
        publicKeys->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(firstSignId.getTimeStamp()));

        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(refIdFirst.to20Char());
        key.push_back('R'); // suffix for referee information
        key.push_back('T'); // suffix for termination entry
        key.append(firstSignId.to20Char());
        key.push_back('C'); // suffix for respective currency
        publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        delete type12entry;
        return true;
    }
    else if (underlyingType == 6)
    {
        // get entry
        Type6Entry* type6entry = static_cast<Type6Entry*>(type12entry->underlyingEntry());
        CompleteID ownerID = type12entry->pubKeyID();
        CompleteID pubKeyID = getFirstID(ownerID);
        CompleteID currencyId = type6entry->getCurrency();
        // save to currenciesAndObligations
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(currencyId.to20Char());
        key.append("AR"); // prefix for referee application
        key.append(util.UcAsByteSeq(0));
        key.append(firstSignId.to20Char());
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        // save to public keys
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(pubKeyID.to20Char());
        key.append("AR"); // prefix for referee application
        key.append(util.UcAsByteSeq(0));
        key.append(currencyId.to20Char());
        key.append(firstSignId.to20Char());
        publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        // save thread info
        string keyP="";
        keyP.push_back('B'); // prefix to distinguish from list head
        keyP.append(firstSignId.to20Char());
        keyP.push_back('T'); // suffix for thread entry

        key=keyP;
        key.push_back('F'); // suffix for application entry
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        key=keyP;
        key.push_back('P'); // suffix for participant id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, pubKeyID.to20Char());

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("CI"); // suffix for currency id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, currencyId.to20Char());

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("BT"); // suffix for begin of tenure
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(type6entry->getTenureStart()));

        unsigned long processingTime = 0;
        Type5Entry* type5entry = getTruncatedEntry(currencyId);
        if (type5entry!=nullptr)
        {
            key=keyP;
            key.push_back('A'); // suffix for application entry information
            key.append("RS"); // suffix for referee stake
            notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type5entry->getRefApplStake()));

            key=keyP;
            key.push_back('A'); // suffix for application entry information
            key.append("PT"); // suffix for processing time
            notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(type5entry->getRefApplProcessingTime()));

            processingTime = type5entry->getRefApplProcessingTime();

            delete type5entry;
        }
        else
        {
            puts("integrateNewNotEntry error: type5entry==nullptr");
        }

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("TP"); // suffix for thread participants list. Output: ID of respective thread entry
        key.append(pubKeyID.to20Char());
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("TS"); // suffix for thread status
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UcAsByteSeq(0));

        // schedule termination
        unsigned long long terminationTime = processingTime;
        terminationTime *= 1000;
        terminationTime += firstSignId.getTimeStamp();
        key="E"; // prefix for expected terminations
        key.append(util.flip(util.UllAsByteSeq(terminationTime)));
        key.append(firstSignId.to20Char());
        scheduledActions->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        delete type12entry;
        return true;
    }
    else if (underlyingType == 7)
    {
        // get entry
        Type7Entry* type7entry = static_cast<Type7Entry*>(type12entry->underlyingEntry());
        CompleteID ownerID = type12entry->pubKeyID();
        CompleteID pubKeyID = getFirstID(ownerID);
        CompleteID currencyId = type7entry->getCurrency();
        CompleteID claimId = type7entry->getLiquidityClaim();
        CompleteID claimIdFirst = getFirstID(claimId);
        // save to currenciesAndObligations
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(currencyId.to20Char());
        key.append("OP"); // prefix for operation proposals
        key.append(util.UcAsByteSeq(0));
        key.append(firstSignId.to20Char());
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        // save to public keys
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(pubKeyID.to20Char());
        key.append("OP"); // suffix for operation proposals
        key.append(util.UcAsByteSeq(0));
        key.append(currencyId.to20Char());
        key.append(firstSignId.to20Char());
        publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        // save thread info
        string keyP="";
        keyP.push_back('B'); // prefix to distinguish from list head
        keyP.append(firstSignId.to20Char());
        keyP.push_back('T'); // suffix for thread entry

        key=keyP;
        key.push_back('F'); // suffix for application entry
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        key=keyP;
        key.push_back('P'); // suffix for participant id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, pubKeyID.to20Char());

        key=keyP;
        key.push_back('C'); // suffix for underlying liquidity claim
        notarizationEntries->Put(rocksdb::WriteOptions(), key, claimIdFirst.to20Char());

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("CI"); // suffix for currency id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, currencyId.to20Char());

        Type5Entry* type5entry = getTruncatedEntry(currencyId);
        if (type5entry!=nullptr)
        {
            key=keyP;
            key.push_back('A'); // suffix for application entry information
            key.append("RS"); // suffix for referee stake
            double refereeStake = type5entry->getRefereeStake(type7entry->getAmount(), type7entry->getFee());
            notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(refereeStake));
            delete type5entry;
        }
        else
        {
            puts("integrateNewNotEntry error: type5entry==nullptr");
        }

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("PT"); // suffix for processing time
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UlAsByteSeq(type7entry->getProcessingTime()));

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("TP"); // suffix for thread participants list. Output: ID of respective thread entry
        key.append(pubKeyID.to20Char());
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("RL"); // suffix for requested liquidity
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type7entry->getAmount()));

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("FL"); // suffix for forfeited liquidity
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type7entry->getOwnStake()));

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("PF"); // suffix for processing fee
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type7entry->getFee()));

        key=keyP;
        key.push_back('A'); // suffix for application entry information
        key.append("TS"); // suffix for thread status
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UcAsByteSeq(0));

        // schedule termination
        unsigned long long terminationTime = type7entry->getProcessingTime();
        terminationTime *= 1000;
        terminationTime += firstSignId.getTimeStamp();
        key="E"; // prefix for expected terminations
        key.append(util.flip(util.UllAsByteSeq(terminationTime)));
        key.append(firstSignId.to20Char());
        scheduledActions->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        delete type12entry;
        return true;
    }
    else if (underlyingType == 8)
    {
        // get entry
        Type8Entry* type8entry = static_cast<Type8Entry*>(type12entry->underlyingEntry());
        CompleteID ownerID = type12entry->pubKeyID();
        CompleteID pubKeyID = getFirstID(ownerID);
        CompleteID predecessorId = type8entry->getPreviousThreadEntry();
        CompleteID predecessorIdFirst = getFirstID(predecessorId);
        CompleteID applicationId = getApplicationId(predecessorIdFirst);
        CompleteID currencyId = getCurrencyId(applicationId);
        CompleteID terminationId = type8entry->getTerminationID();
        CompleteID terminationIdFirst = getFirstID(terminationId);
        CompleteID claimId = type8entry->getLiquidityClaim();
        CompleteID claimIdFirst = getFirstID(claimId);

        if (applicationId == predecessorIdFirst)
        {
            double liquidity = getRequestedLiquidity(applicationId);
            if (liquidity>0)
            {
                // update LiquidityCreatedSoFar
                liquidity += getLiquidityCreatedSoFar(pubKeyID, terminationIdFirst);
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(pubKeyID.to20Char());
                key.push_back('R'); // suffix for referee information
                key.push_back('T'); // suffix for termination entry
                key.append(terminationId.to20Char());
                key.push_back('L'); // suffix for liquidity created
                publicKeys->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(liquidity));
            }
            else
            {
                // update RefsAppointedSoFar
                unsigned short num = getRefsAppointedSoFar(pubKeyID, terminationIdFirst)+1;
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(pubKeyID.to20Char());
                key.push_back('R'); // suffix for referee information
                key.push_back('T'); // suffix for termination entry
                key.append(terminationId.to20Char());
                key.push_back('R'); // suffix for referees appointed
                publicKeys->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(num));
            }
        }
        // update thread entry data
        string keyP="";
        keyP.push_back('B'); // prefix to distinguish from list head
        keyP.append(firstSignId.to20Char());
        keyP.push_back('T'); // suffix for thread entry

        key=keyP;
        key.push_back('F'); // suffix for application entry
        notarizationEntries->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());

        key=keyP;
        key.push_back('P'); // suffix for participant id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, pubKeyID.to20Char());

        key=keyP;
        key.push_back('C'); // suffix for underlying liquidity claim
        notarizationEntries->Put(rocksdb::WriteOptions(), key, claimIdFirst.to20Char());

        key=keyP;
        key.push_back('T'); // suffix for termination entry (of participating ref)
        notarizationEntries->Put(rocksdb::WriteOptions(), key, terminationIdFirst.to20Char());

        key=keyP;
        key.push_back('N'); // suffix for non-transferable liquidity in stake
        double nonTransferPart = min(getRefereeStake(applicationId),
                                     getCollectableNonTransferLiqui(claimIdFirst, pubKeyID, currencyId, getDiscountRate(currencyId), 0, firstSignId, 0));
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(nonTransferPart));

        // update application entry data
        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(applicationId.to20Char());
        key.push_back('T'); // suffix for thread entry
        key.push_back('A'); // suffix for application entry information
        key.append("TP"); // suffix for thread participants list. Output: ID of respective thread entry
        key.append(pubKeyID.to20Char());
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        // update thread status everywhere
        unsigned char oldThreadStatus = getThreadStatus(applicationId);
        unsigned char newThreadStatus = oldThreadStatus + 1;
        if (newThreadStatus>2) newThreadStatus = 1;

        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(applicationId.to20Char());
        key.push_back('T'); // suffix for thread entry
        key.push_back('A'); // suffix for application entry information
        key.append("TS"); // suffix for thread status
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UcAsByteSeq(newThreadStatus));

        string OPorAR="OP";
        if (getRequestedLiquidity(applicationId)<=0) OPorAR="AR";

        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(currencyId.to20Char());
        key.append(OPorAR); // prefix for operation proposals
        key.append(util.UcAsByteSeq(newThreadStatus));
        key.append(applicationId.to20Char());
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(currencyId.to20Char());
        key.append(OPorAR); // prefix for operation proposals
        key.append(util.UcAsByteSeq(oldThreadStatus));
        key.append(applicationId.to20Char());
        currenciesAndObligations->Delete(rocksdb::WriteOptions(), key);

        CompleteID applicantId = getThreadParticipantId(applicationId);
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(applicantId.to20Char());
        key.append(OPorAR); // suffix for operation proposals
        key.append(util.UcAsByteSeq(newThreadStatus));
        key.append(currencyId.to20Char());
        key.append(applicationId.to20Char());
        publicKeys->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
        key="";
        key.push_back('I'); // suffix for key identification by id
        key.append(applicantId.to20Char());
        key.append(OPorAR); // suffix for operation proposals
        key.append(util.UcAsByteSeq(oldThreadStatus));
        key.append(currencyId.to20Char());
        key.append(applicationId.to20Char());
        publicKeys->Delete(rocksdb::WriteOptions(), key);

        CompleteID zeroId;
        CompleteID runningId = getThreadSuccessor(applicationId, zeroId);
        while (isInGeneralList(runningId))
        {
            CompleteID refereeId = getThreadParticipantId(runningId);
            if (refereeId.getNotary()<=0) break;
            CompleteID refereeIdFirst = getFirstID(refereeId);

            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(refereeIdFirst.to20Char());
            key.push_back('R'); // suffix for referee information
            key.push_back('C'); // suffix for currency choice
            key.append(currencyId.to20Char());
            key.append(OPorAR); // suffix for operation proposals
            key.append(util.UcAsByteSeq(newThreadStatus));
            key.append(applicationId.to20Char());
            publicKeys->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(refereeIdFirst.to20Char());
            key.push_back('R'); // suffix for referee information
            key.push_back('C'); // suffix for currency choice
            key.append(currencyId.to20Char());
            key.append(OPorAR); // suffix for operation proposals
            key.append(util.UcAsByteSeq(oldThreadStatus));
            key.append(applicationId.to20Char());
            publicKeys->Delete(rocksdb::WriteOptions(), key);

            runningId = getThreadSuccessor(runningId, zeroId);
        }

        // schedule termination
        unsigned long long terminationTime = getProcessingTime(applicationId);
        terminationTime *= 1000;
        terminationTime += firstSignId.getTimeStamp();
        key="E"; // prefix for expected terminations
        key.append(util.flip(util.UllAsByteSeq(terminationTime)));
        key.append(firstSignId.to20Char());
        scheduledActions->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        deleteT9eCreationParameters(predecessorIdFirst);

        delete type12entry;
        return true;
    }
    else if (underlyingType == 9)
    {
        // get entry
        Type9Entry* type9entry = static_cast<Type9Entry*>(type12entry->underlyingEntry());
        CompleteID predecessorId = type9entry->getPreviousThreadEntry();
        CompleteID predecessorIdFirst = getFirstID(predecessorId);
        CompleteID applicationId = getApplicationId(predecessorIdFirst);
        CompleteID currencyId = getCurrencyId(applicationId);
        // thread entry data
        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(firstSignId.to20Char());
        key.push_back('T'); // suffix for thread entry
        key.push_back('F'); // suffix for application entry
        notarizationEntries->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
        // update application entry data
        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(applicationId.to20Char());
        key.push_back('T'); // suffix for thread entry
        key.push_back('A'); // suffix for application entry information
        key.append("TI"); // suffix for termination entry id
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        // update thread status everywhere
        unsigned char oldThreadStatus = getThreadStatus(applicationId);
        unsigned char newThreadStatus = oldThreadStatus + 2;
        if (newThreadStatus==2) newThreadStatus = 4;

        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(applicationId.to20Char());
        key.push_back('T'); // suffix for thread entry
        key.push_back('A'); // suffix for application entry information
        key.append("TS"); // suffix for thread status
        notarizationEntries->Put(rocksdb::WriteOptions(), key, util.UcAsByteSeq(newThreadStatus));

        if (currencyId.getNotary()<=0) // notary application
        {
            if (predecessorIdFirst != applicationId)
            {
                // add to terminated (but considered) applications
                key = "";
                key.push_back('B'); // prefix for body
                key.append(util.UcAsByteSeq(5));
                key.append(firstSignId.to20Char());
                notaryApplications->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

                // decrease initiated threads count
                CompleteID zeroId;
                CompleteID firstNoteId = getThreadSuccessor(applicationId, zeroId);
                CompleteID participantId = getThreadParticipantId(firstNoteId);
                TNtrNr totalNotaryNr = getTotalNotaryNr(participantId);
                unsigned short threadsCount = getInitiatedThreadsCount(totalNotaryNr);
                if (threadsCount>0) threadsCount--;
                setInitiatedThreadsCount(totalNotaryNr, threadsCount);
            }

            // update thread status for notary application
            key = "";
            key.push_back('B'); // prefix for body
            key.append(util.UcAsByteSeq(newThreadStatus));
            key.append(applicationId.to20Char());
            notaryApplications->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
            key = "";
            key.push_back('B'); // prefix for body
            key.append(util.UcAsByteSeq(oldThreadStatus));
            key.append(applicationId.to20Char());
            notaryApplications->Delete(rocksdb::WriteOptions(), key);

            CompleteID applicantId = getThreadParticipantId(applicationId);
            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(applicantId.to20Char());
            key.append("AN"); // prefix for notary application
            key.append(util.UcAsByteSeq(newThreadStatus));
            key.append(applicationId.to20Char());
            publicKeys->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(applicantId.to20Char());
            key.append("AN"); // prefix for notary application
            key.append(util.UcAsByteSeq(oldThreadStatus));
            key.append(applicationId.to20Char());
            publicKeys->Delete(rocksdb::WriteOptions(), key);

            set<TNtrNr, TNtrNr::CompareTNtrNrs> supportingNotaries;
            set<TNtrNr, TNtrNr::CompareTNtrNrs> opposingNotaries;
            size_t noteCount = 0;
            CompleteID firstNoteId;
            CompleteID zeroId;
            CompleteID runningId = getThreadSuccessor(applicationId, zeroId);
            while (isInGeneralList(runningId) && runningId!=firstSignId)
            {
                if (firstNoteId.isZero()) firstNoteId=runningId;

                CompleteID notaryId = getThreadParticipantId(runningId);
                if (notaryId.getNotary()<=0) break;
                notaryId = getFirstID(notaryId);
                TNtrNr totalNotaryNr = getTotalNotaryNr(notaryId);

                noteCount++;
                if (isActingNotary(totalNotaryNr.getNotaryNr(), firstSignId.getTimeStamp()))
                {
                    if (noteCount % 2 == 1)
                    {
                        supportingNotaries.insert(totalNotaryNr);
                    }
                    else
                    {
                        opposingNotaries.insert(totalNotaryNr);
                    }
                }

                key="";
                key.push_back('B'); // prefix for body
                key.append(totalNotaryNr.toString());
                key.append("AN"); // suffix for notary applications
                key.append(util.UcAsByteSeq(newThreadStatus));
                key.append(applicationId.to20Char());
                notaries->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());

                key="";
                key.push_back('B'); // prefix for body
                key.append(totalNotaryNr.toString());
                key.append("AN"); // suffix for notary applications
                key.append(util.UcAsByteSeq(oldThreadStatus));
                key.append(applicationId.to20Char());
                notaries->Delete(rocksdb::WriteOptions(), key);

                runningId = getThreadSuccessor(runningId, zeroId);
            }

            // set notary appointment to be finalized flag (if approved)
            if (noteCount % 2 == 1)
            {
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(applicantId.to20Char());
                key.append("NA"); // suffix for notary appointment to be finalized
                publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
            }

            // redistribute fees
            if (!firstNoteId.isZero())
            {
                unsigned short lineage = type1entry->getLineage(applicationId.getTimeStamp());

                if (noteCount % 2 == 1)
                {
                    // redistribute from opposing to supporting
                    set<TNtrNr>::iterator it;
                    for (it=opposingNotaries.begin(); it!=opposingNotaries.end(); ++it)
                    {
                        TNtrNr loser = *it;
                        double penaltyFactor = type1entry->getPenaltyFactor1(lineage);
                        decreaseShareToKeep(loser, penaltyFactor);
                        set<TNtrNr>::iterator it2;
                        for (it2=supportingNotaries.begin(); it2!=supportingNotaries.end(); ++it2)
                        {
                            TNtrNr winner = *it2;
                            addToRedistributionMultiplier(loser, winner, penaltyFactor);
                        }
                    }

                    // determine the abstainers
                    set<TNtrNr, TNtrNr::CompareTNtrNrs> abstainers;
                    set<unsigned long>* actingNotaries = getActingNotaries(applicationId.getTimeStamp());
                    set<unsigned long>::iterator it3;
                    for (it3=actingNotaries->begin(); it3!=actingNotaries->end(); ++it3)
                    {
                        TNtrNr tNtrNr(lineage, *it3);
                        if (supportingNotaries.count(tNtrNr)<=0 && opposingNotaries.count(tNtrNr)<=0
                                && isActingNotary(tNtrNr.getNotaryNr(), firstSignId.getTimeStamp()))
                        {
                            abstainers.insert(tNtrNr);
                        }
                    }
                    actingNotaries->clear();
                    delete actingNotaries;

                    // redistribute from abstainers to supporting
                    for (it=abstainers.begin(); it!=abstainers.end(); ++it)
                    {
                        TNtrNr loser = *it;
                        double penaltyFactor = type1entry->getPenaltyFactor2(lineage);
                        decreaseShareToKeep(loser, penaltyFactor);
                        set<TNtrNr>::iterator it2;
                        for (it2=supportingNotaries.begin(); it2!=supportingNotaries.end(); ++it2)
                        {
                            TNtrNr winner = *it2;
                            addToRedistributionMultiplier(loser, winner, penaltyFactor);
                        }
                    }
                }
                else
                {
                    // redistribute from supporting to opposing
                    set<TNtrNr>::iterator it;
                    for (it=supportingNotaries.begin(); it!=supportingNotaries.end(); ++it)
                    {
                        TNtrNr loser = *it;
                        double penaltyFactor = type1entry->getPenaltyFactor1(lineage);
                        decreaseShareToKeep(loser, penaltyFactor);
                        set<TNtrNr>::iterator it2;
                        for (it2=opposingNotaries.begin(); it2!=opposingNotaries.end(); ++it2)
                        {
                            TNtrNr winner = *it2;
                            addToRedistributionMultiplier(loser, winner, penaltyFactor);
                        }
                    }
                }
            }
        }
        else // operation proposal or referee application
        {
            string OPorAR="OP";
            if (getRequestedLiquidity(applicationId)<=0) OPorAR="AR";

            // update thread status
            key = "";
            key.push_back('I'); // prefix to identification by Id
            key.append(currencyId.to20Char());
            key.append(OPorAR); // prefix for operation proposals
            key.append(util.UcAsByteSeq(newThreadStatus));
            key.append(applicationId.to20Char());
            currenciesAndObligations->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
            key = "";
            key.push_back('I'); // prefix to identification by Id
            key.append(currencyId.to20Char());
            key.append(OPorAR); // prefix for operation proposals
            key.append(util.UcAsByteSeq(oldThreadStatus));
            key.append(applicationId.to20Char());
            currenciesAndObligations->Delete(rocksdb::WriteOptions(), key);

            CompleteID applicantId = getThreadParticipantId(applicationId);
            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(applicantId.to20Char());
            key.append(OPorAR); // suffix for operation proposals
            key.append(util.UcAsByteSeq(newThreadStatus));
            key.append(currencyId.to20Char());
            key.append(applicationId.to20Char());
            publicKeys->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(applicantId.to20Char());
            key.append(OPorAR); // suffix for operation proposals
            key.append(util.UcAsByteSeq(oldThreadStatus));
            key.append(currencyId.to20Char());
            key.append(applicationId.to20Char());
            publicKeys->Delete(rocksdb::WriteOptions(), key);

            CompleteID firstNoteId;
            CompleteID zeroId;
            CompleteID runningId = getThreadSuccessor(applicationId, zeroId);
            while (isInGeneralList(runningId) && runningId!=firstSignId)
            {
                if (firstNoteId.isZero()) firstNoteId=runningId;

                CompleteID refereeId = getThreadParticipantId(runningId);
                if (refereeId.getNotary()<=0) break;
                CompleteID refereeIdFirst = getFirstID(refereeId);

                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(refereeIdFirst.to20Char());
                key.push_back('R'); // suffix for referee information
                key.push_back('C'); // suffix for currency choice
                key.append(currencyId.to20Char());
                key.append(OPorAR); // suffix for operation proposals
                key.append(util.UcAsByteSeq(newThreadStatus));
                key.append(applicationId.to20Char());
                publicKeys->Put(rocksdb::WriteOptions(), key, applicationId.to20Char());
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(refereeIdFirst.to20Char());
                key.push_back('R'); // suffix for referee information
                key.push_back('C'); // suffix for currency choice
                key.append(currencyId.to20Char());
                key.append(OPorAR); // suffix for operation proposals
                key.append(util.UcAsByteSeq(oldThreadStatus));
                key.append(applicationId.to20Char());
                publicKeys->Delete(rocksdb::WriteOptions(), key);

                runningId = getThreadSuccessor(runningId, zeroId);
            }
            // set new liquidity creation flags
            if (OPorAR.compare("OP")==0) // for operation proposals
            {
                if (newThreadStatus==4 && !firstNoteId.isZero())
                {
                    CompleteID firstRefTerminationId = getRefTerminationEntryId(firstNoteId);
                    CompleteID firstRefId = getThreadParticipantId(firstNoteId);
                    double liquidity = getLiquidityCreatedSoFar(firstRefId, firstRefTerminationId);
                    double feeFactor = 1.0;
                    feeFactor -= (type1entry->getFee(type1entry->getLineage(applicationId.getTimeStamp())) * 0.01);
                    liquidity -= (getRequestedLiquidity(applicationId) * feeFactor);
                    // update LiquidityCreatedSoFar
                    key="";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(firstRefId.to20Char());
                    key.push_back('R'); // suffix for referee information
                    key.push_back('T'); // suffix for termination entry
                    key.append(firstRefTerminationId.to20Char());
                    key.push_back('L'); // suffix for liquidity created
                    publicKeys->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(liquidity));
                }
                else if (newThreadStatus==3)
                {
                    // save granted liquidity for applicant
                    double requestedLiquidityAndForfeit = getRequestedLiquidity(applicationId)+getForfeit(applicationId);
                    key="";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(applicantId.to20Char());
                    key.append("TC"); // to claim asap
                    key.append(currencyId.to20Char());
                    key.append(firstSignId.to20Char());
                    key.append(util.UcAsByteSeq(9));
                    publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
                    addToTTLiquidityNotClaimedYet(applicantId, firstSignId, 9, true, requestedLiquidityAndForfeit);
                }
            }
            else // referee application
            {
                if (newThreadStatus==4 && !firstNoteId.isZero())
                {
                    CompleteID firstRefTerminationId = getRefTerminationEntryId(firstNoteId);
                    CompleteID firstRefId = getThreadParticipantId(firstNoteId);
                    unsigned short appointedSoFar = getRefsAppointedSoFar(firstRefId, firstRefTerminationId)-1;
                    // update refsAppointedSoFar
                    key="";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(firstRefId.to20Char());
                    key.push_back('R'); // suffix for referee information
                    key.push_back('T'); // suffix for termination entry
                    key.append(firstRefTerminationId.to20Char());
                    key.push_back('R'); // suffix for refs appointed so far
                    publicKeys->Put(rocksdb::WriteOptions(), key, util.UsAsByteSeq(appointedSoFar));
                }
                else if (newThreadStatus==3)
                {
                    // save initial liquidity for new ref
                    double initialLiqui = getInitialLiquidity(currencyId);
                    key="";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(applicantId.to20Char());
                    key.append("TC"); // to claim asap
                    key.append(currencyId.to20Char());
                    key.append(firstSignId.to20Char());
                    key.append(util.UcAsByteSeq(4));
                    publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
                    addToTTLiquidityNotClaimedYet(applicantId, firstSignId, 4, true, initialLiqui);
                    addToTTLiquidityNotClaimedYet(applicantId, firstSignId, 4, false, initialLiqui);
                    // mark ref as acting
                    key="";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(applicantId.to20Char());
                    key.push_back('R'); // suffix for referee information
                    key.push_back('T'); // suffix for termination entry
                    key.append(firstSignId.to20Char());
                    key.push_back('T'); // suffix for tenure start
                    publicKeys->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(getTenureStart(applicationId)));

                    key="";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(applicantId.to20Char());
                    key.push_back('R'); // suffix for referee information
                    key.push_back('T'); // suffix for termination entry
                    key.append(firstSignId.to20Char());
                    key.push_back('C'); // suffix for respective currency
                    publicKeys->Put(rocksdb::WriteOptions(), key, currencyId.to20Char());
                }
            }
            // save liquidity to be claimed by winning refs
            if (!firstNoteId.isZero())
            {
                double stake = getRefereeStake(applicationId);
                double fee = getProcessingFee(applicationId);
                CompleteID winningNoteId = firstNoteId;
                if (newThreadStatus==4) winningNoteId = getThreadSuccessor(firstNoteId, zeroId);
                while (isInGeneralList(winningNoteId) && winningNoteId!=firstSignId)
                {
                    CompleteID refId = getThreadParticipantId(winningNoteId);
                    // claim own stake back
                    key="";
                    key.push_back('I'); // suffix for key identification by id
                    key.append(refId.to20Char());
                    key.append("TC"); // to claim asap
                    key.append(currencyId.to20Char());
                    key.append(firstSignId.to20Char());
                    key.append(util.UcAsByteSeq(8));
                    publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
                    addToTTLiquidityNotClaimedYet(refId, firstSignId, 8, true, stake);
                    addToTTLiquidityNotClaimedYet(refId, firstSignId, 8, false, getNonTransferPartInStake(winningNoteId));
                    // claim won fee or stake
                    double wonAmount = stake;
                    if (winningNoteId == firstNoteId) wonAmount=fee;
                    double shareFactor = getParentShare(currencyId)/100;
                    CompleteID refTerminationId = getRefTerminationEntryId(winningNoteId);
                    CompleteID refAppointmentId = getFirstNoteId(refTerminationId); // appointing note of the respective ref
                    do
                    {
                        key="";
                        key.push_back('I'); // suffix for key identification by id
                        key.append(refId.to20Char());
                        key.append("TC"); // to claim asap
                        key.append(currencyId.to20Char());
                        key.append(firstSignId.to20Char());
                        key.append(util.UcAsByteSeq(7));
                        publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
                        addToTTLiquidityNotClaimedYet(refId, firstSignId, 7, true, wonAmount*(((double)1.0)-shareFactor));

                        wonAmount *= shareFactor;

                        if (refAppointmentId.getNotary()>0)
                        {
                            refId = getThreadParticipantId(refAppointmentId);
                            refTerminationId = getRefTerminationEntryId(refAppointmentId);
                            refAppointmentId = getFirstNoteId(refTerminationId);
                        }
                        else break;
                    }
                    while (isInGeneralList(refTerminationId));
                    // add the remaining amount to first ref of currency
                    addToTTLiquidityNotClaimedYet(refId, firstSignId, 7, true, wonAmount);
                    // consider next winning note
                    winningNoteId = getThreadSuccessor(winningNoteId, zeroId); // now losing note
                    winningNoteId = getThreadSuccessor(winningNoteId, zeroId); // now winning note
                }
            }
        }

        deleteT9eCreationParameters(predecessorIdFirst);

        delete type12entry;
        return true;
    }
    else if (underlyingType == 10)
    {
        // get entry
        Type10Entry* type10entry = static_cast<Type10Entry*>(type12entry->underlyingEntry());
        // set as outflow entry for the claim
        CompleteID claimId = type10entry->getSource();
        CompleteID claimIdFirst = getFirstID(claimId);
        CompleteID zeroId;
        if (getOutflowEntry(claimIdFirst, zeroId).getNotary()>0)
        {
            key="";
            key.push_back('B'); // prefix to distinguish from list head
            key.append(claimIdFirst.to20Char());
            key.push_back('C'); // suffix for claim entry
            key.append("OE"); // suffix for outflow entry
            notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        }
        // collect notarization fee
        if (type10entry->getTransferAmount()>0)
        {
            unsigned short lineage = type1entry->getLineage(firstSignId.getTimeStamp());
            double fee = type10entry->getTransferAmount() * type1entry->getFee(lineage) * 0.01;
            TNtrNr tNtrNr(lineage, firstSignId.getNotary());
            CompleteID currencyID = type10entry->getCurrencyOrObl();
            currencyID = getFirstID(currencyID);
            addToCollectedFees(tNtrNr, currencyID, fee);
            // store id in "transfersWithFees"
            key="";
            key.push_back('B'); // prefix for body
            key.append(firstSignId.to20Char());
            transfersWithFees->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        }
        CompleteID targetId = type10entry->getTarget();
        CompleteID targetIdFirst = getFirstID(targetId);
        // check if target is a type10entry
        CompleteID connectedTransferId = getConnectedTransfer(targetIdFirst, zeroId);
        if (connectedTransferId.getNotary()<=0)
        {
            connectedTransferId = firstSignId;
        }
        else if (connectedTransferId != targetIdFirst)
        {
            connectedTransferId = targetIdFirst;
        }
        // set connected transfer for this entry
        string key;
        key.push_back('B'); // prefix to distinguish from list head
        key.append(firstSignId.to20Char());
        key.append("LT"); // suffix for liquidity transfer entry information
        key.append("CT"); // suffix for the connected transfer
        notarizationEntries->Put(rocksdb::WriteOptions(), key, connectedTransferId.to20Char());
        if (connectedTransferId == targetIdFirst)
        {
            // set connected transfer for the other entry
            key="";
            key.push_back('B'); // prefix to distinguish from list head
            key.append(connectedTransferId.to20Char());
            key.append("LT"); // suffix for liquidity transfer entry information
            key.append("CT"); // suffix for the connected transfer
            notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        }
        // add to list (or lists) in publicKeys
        CompleteID pubKeyID = type12entry->pubKeyID();
        pubKeyID = getFirstID(pubKeyID);
        CompleteID currencyId = type10entry->getCurrencyOrObl();
        if (connectedTransferId == firstSignId && targetIdFirst.isZero()) // liquidity deletion
        {
            // nothing to do
        }
        else if (connectedTransferId == firstSignId && type10entry->getRequestedAmount()>0) // offer (request/exchange)
        {
            if (type10entry->getTransferAmount()==0)
            {
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(pubKeyID.to20Char());
                key.append("TR"); // suffix for transfer request
                key.append(currencyId.to20Char());
                key.append(firstSignId.to20Char());
                publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
            }
            else
            {
                addToExchangeOffersList(firstSignId, pubKeyID, currencyId, targetIdFirst, type10entry->getRequestedAmount());
            }
        }
        else if (connectedTransferId == firstSignId && getValidityDate(targetIdFirst)>0) // standard transfer
        {
            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(targetIdFirst.to20Char());
            key.append("TC"); // to claim asap
            key.append(currencyId.to20Char());
            key.append(firstSignId.to20Char());
            key.append(util.UcAsByteSeq(1));
            publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        }
        else if (connectedTransferId != firstSignId) // have a pair of connected transfers
        {
            // get signatory of the other entry
            Type12Entry* otherT10eWrap = buildUnderlyingEntry(connectedTransferId,0);
            CompleteID otherKeyId = otherT10eWrap->pubKeyID();
            CompleteID otherKeyIdFirst = getFirstID(otherKeyId);
            Type10Entry* type10entryOther = static_cast<Type10Entry*>(otherT10eWrap->underlyingEntry());
            CompleteID otherCurrencyId = type10entryOther->getCurrencyOrObl();
            // save to public key list for the other entry
            key="";
            key.push_back('I'); // suffix for key identification by id
            key.append(otherKeyIdFirst.to20Char());
            key.append("TC"); // to claim asap
            key.append(currencyId.to20Char());
            key.append(firstSignId.to20Char());
            key.append(util.UcAsByteSeq(1));
            publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
            // save to public key list for this entry
            if (type10entryOther->getTransferAmount()>0)
            {
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(pubKeyID.to20Char());
                key.append("TC"); // to claim asap
                key.append(otherCurrencyId.to20Char());
                key.append(connectedTransferId.to20Char());
                key.append(util.UcAsByteSeq(1));
                publicKeys->Put(rocksdb::WriteOptions(), key, connectedTransferId.to20Char());
                // remove EO from list
                removeFromExchangeOffersList(connectedTransferId, otherKeyIdFirst, otherCurrencyId, currencyId);
            }
            delete otherT10eWrap;
        }
        delete type12entry;
        return true;
    }
    else if (underlyingType == 11)
    {
        key="";
        key.push_back('B'); // prefix for body
        key.append(firstSignId.to20Char());
        perpetualEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
        // build entry
        Type11Entry* type11entry = static_cast<Type11Entry*>(type12entry->underlyingEntry());
        string* pubKey = type11entry->getPublicKey();
        unsigned long long validityDate = type11entry->getValidityDate();
        CompleteID pubKeyId = getFirstID(pubKey);
        if (pubKeyId.getNotary() > 0)
        {
            unsigned long long oldValidityDate = getValidityDate(pubKeyId);
            if (validityDate < oldValidityDate)
            {
                // update validity date
                key = "";
                key.push_back('I'); // suffix for key identification by id
                key.append(pubKeyId.to20Char());
                key.append("VD"); // suffix for validity date
                publicKeys->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(validityDate));
                // update entry for validity date
                key = "";
                key.push_back('I'); // suffix for key identification by id
                key.append(pubKeyId.to20Char());
                key.append("VE"); // suffix for validity date entry
                publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());
            }
        }
        else
        {
            pubKeyId = firstSignId;
            // save key to publicKeys
            key = "";
            key.push_back('B'); // suffix for key identification by byte sequence
            key.append(util.UlAsByteSeq(pubKey->length()));
            key.append(*pubKey);
            publicKeys->Put(rocksdb::WriteOptions(), key, pubKeyId.to20Char());

            keyPrefPref = "";
            keyPrefPref.push_back('I'); // suffix for key identification by id
            keyPrefPref.append(pubKeyId.to20Char());

            key = "";
            key.append(keyPrefPref);
            key.append("PK"); // suffix for public key
            publicKeys->Put(rocksdb::WriteOptions(), key, *pubKey);

            key = "";
            key.append(keyPrefPref);
            key.append("VD"); // suffix for validity date
            publicKeys->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(validityDate));

            // update entry for validity date
            key = "";
            key.append(keyPrefPref);
            key.append("VE"); // suffix for validity date entry
            publicKeys->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

            TNtrNr totalNotaryNr = getTotalNotaryNr(pubKey);
            if (!totalNotaryNr.isZero())
            {
                // save total notary number to public keys
                key = "";
                key.append(keyPrefPref);
                key.append("TN"); // suffix for total notary number
                publicKeys->Put(rocksdb::WriteOptions(), key, totalNotaryNr.toString());

                CompleteID notaryId = getNotaryId(totalNotaryNr);
                if (notaryId.getNotary() <= 0)
                {
                    // save publicKeyId to notaries list
                    key = "";
                    key.push_back('B'); // prefix for body
                    key.append(totalNotaryNr.toString());
                    key.append("ID"); // prefix for id
                    notaries->Put(rocksdb::WriteOptions(), key, pubKeyId.to20Char());

                    // save public key to notaries list
                    key = "";
                    key.push_back('B'); // prefix for body
                    key.append(totalNotaryNr.toString());
                    key.append("PK"); // prefix for id
                    notaries->Put(rocksdb::WriteOptions(), key, *pubKey);

                    if (getShareToKeep(totalNotaryNr) == 0)
                    {
                        // save share to keep
                        key = "";
                        key.push_back('B'); // prefix for body
                        key.append(totalNotaryNr.toString());
                        key.append("SK"); // prefix for share to keep
                        notaries->Put(rocksdb::WriteOptions(), key,  util.DblAsByteSeq(1));
                    }
                }
            }
        }
        delete type12entry;
        return true;
    }
    else if (underlyingType == 14)
    {
        Type14Entry* type14entry = static_cast<Type14Entry*>(type12entry->underlyingEntry());
        CompleteID ownerId = type14entry->getOwnerID();
        CompleteID ownerIdFirst = getFirstID(ownerId);
        CompleteID currencyId = type14entry->getCurrencyOrObligation();

        // update claim count
        unsigned long long claimCount = getClaimCount(ownerIdFirst, currencyId);
        string key;
        key.push_back('I'); // suffix for key identification by id
        key.append(ownerIdFirst.to20Char());
        key.push_back('C'); // suffix for liquidity claims
        key.append(currencyId.to20Char());
        key.push_back('H'); // suffix for head (count)
        claimCount++;
        publicKeys->Put(rocksdb::WriteOptions(), key, util.UllAsByteSeq(claimCount));

        // set outflow entry to this new claim
        key="";
        key.push_back('B'); // prefix to distinguish from list head
        key.append(firstSignId.to20Char());
        key.push_back('C'); // suffix for claim entry
        key.append("OE"); // suffix for outflow entry
        notarizationEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        // remove to-claim-asap-tag etc.
        const unsigned short predecessorCount = type14entry->getPredecessorsCount();
        for (unsigned short i=0; i<predecessorCount; i++)
        {
            unsigned char scenario = type14entry->getScenario(i);
            if (scenario != 0)
            {
                CompleteID predecessorId = type14entry->getPredecessor(i);
                CompleteID predecessorIdFirst = getFirstID(predecessorId);
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(ownerIdFirst.to20Char());
                key.append("TC"); // to claim asap
                key.append(currencyId.to20Char());
                key.append(predecessorIdFirst.to20Char());
                key.append(util.UcAsByteSeq(scenario));
                publicKeys->Delete(rocksdb::WriteOptions(), key);
                // remove exchange offer if necessary
                if (scenario == 1)
                {
                    key="";
                    string value;
                    key.push_back('B'); // prefix to distinguish from list head
                    key.append(predecessorIdFirst.to20Char());
                    key.append("EO"); // suffix for exchange offer
                    key.append("RA"); // suffix for requested amount
                    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
                    const bool success = (s.ok() && value.length()>2);
                    if (success) // have exchange offer
                    {
                        string entryStr;
                        if (loadInitialT13EntryStr(predecessorIdFirst, entryStr))
                        {
                            Type12Entry *type12entry2 = createT12FromT13Str(entryStr);
                            if (type12entry2 != nullptr && type12entry2->underlyingType()==10)
                            {
                                Type10Entry *t10e = (Type10Entry*) type12entry2->underlyingEntry();
                                CompleteID offerOwner = type12entry2->pubKeyID();
                                offerOwner = getFirstID(offerOwner);
                                CompleteID currencyOId = t10e->getCurrencyOrObl();
                                CompleteID currencyRId = t10e->getTarget();
                                removeFromExchangeOffersList(predecessorIdFirst, offerOwner, currencyOId, currencyRId);
                            }
                            if (type12entry2 != nullptr) delete type12entry2;
                        }
                    }
                }
            }
            if (scenario == 4 || scenario == 7 || scenario == 8 || scenario == 9)
            {
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(ownerIdFirst.to20Char());
                key.append("TT"); // suffix for thread termination entries
                key.push_back('N'); // liquidity not claimed yet
                key.append(util.UcAsByteSeq(scenario));
                key.append(firstSignId.to20Char());
                key.push_back('T'); // suffix for total claimable liquidity
                publicKeys->Delete(rocksdb::WriteOptions(), key);
            }
            if (scenario == 4 || scenario == 8)
            {
                key="";
                key.push_back('I'); // suffix for key identification by id
                key.append(ownerIdFirst.to20Char());
                key.append("TT"); // suffix for thread termination entries
                key.push_back('N'); // liquidity not claimed yet
                key.append(util.UcAsByteSeq(scenario));
                key.append(firstSignId.to20Char());
                key.push_back('N'); // suffix for non transferable liquidity
                publicKeys->Delete(rocksdb::WriteOptions(), key);
            }
        }
        delete type12entry;
        return true;
    }
    else if (underlyingType == 15)
    {
        key="";
        key.push_back('B'); // prefix for body
        key.append(firstSignId.to20Char());
        perpetualEntries->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        Type15Entry* type15entry = static_cast<Type15Entry*>(type12entry->underlyingEntry());

        // save char seq to currenciesAndObligations
        key = "";
        key.push_back('B'); // prefix to identification by byte sequence
        string *charaByteSeq = type15entry->getByteSeq();
        key.append(util.UlAsByteSeq(charaByteSeq->length()));
        key.append(*charaByteSeq);
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, firstSignId.to20Char());

        // save obligation owner
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("OO"); // suffix for obligation owner
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, type15entry->getOwnerID().to20Char());

        // save discount rate
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("DR"); // suffix for discount rate
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type15entry->getRate()));

        // save min transfer amount
        key = "";
        key.push_back('I'); // prefix to identification by Id
        key.append(firstSignId.to20Char());
        key.append("MT"); // suffix for minimal transfer amount
        currenciesAndObligations->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(type15entry->getMinTransferAmount()));

        delete type12entry;
        return true;
    }
    delete type12entry;
    return false;
}

// db must be locked for this
void Database::deleteTailSignatures(CIDsSet &ids, bool renot)
{
    string keyPref;
    rocksdb::DB* dbList = entriesInNotarization;
    if (renot)
    {
        keyPref.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;
    }

    set<CompleteID, CompleteID::CompareIDs>* idsSet = ids.getSetPointer();
    set<CompleteID, CompleteID::CompareIDs>::iterator it;
    for (it=idsSet->begin(); it!=idsSet->end(); ++it)
    {
        CompleteID nextID = *it;
        if (getSignatureCount(nextID, renot) == 1) continue;

        string keyPrefPref = "";
        keyPrefPref.append(keyPref);
        keyPrefPref.append(nextID.to20Char());

        // delete reference to first
        string key = "";
        key.append(keyPrefPref);
        key.push_back('F'); // suffix for first type 13 entry
        dbList->Delete(rocksdb::WriteOptions(), key);

        // delete actual entry
        key = "";
        key.append(keyPrefPref);
        key.append("AE"); // suffix for entry
        dbList->Delete(rocksdb::WriteOptions(), key);

        // delete signature count
        key = "";
        key.append(keyPrefPref);
        key.append("SC"); // suffix for signature count
        dbList->Delete(rocksdb::WriteOptions(), key);

        // get reference to next and delete reference
        key = "";
        key.append(keyPrefPref);
        key.push_back('N'); // suffix for next type 13 entry
        dbList->Delete(rocksdb::WriteOptions(), key);
    }
}

// db must be locked for this
CompleteID Database::deleteSignatures(CompleteID &firstSignId, bool renot)
{
    CompleteID out;
    string keyPref;
    rocksdb::DB* dbList = entriesInNotarization;
    if (renot)
    {
        keyPref.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;
    }
    else
    {
        // get characterizing byte sequence
        string type13Str;
        if (!loadType13EntryStr(firstSignId, 1, type13Str)) return CompleteID();
        Type12Entry *type12entry = createT12FromT13Str(type13Str);
        if (type12entry == nullptr) return CompleteID();
        int type = type12entry->underlyingType();
        string* charaByteSeq;
        if (type == 5 || type == 15) charaByteSeq = type12entry->underlyingEntry()->getByteSeq();
        else if (type == 11)
        {
            Type11Entry* type11entry = static_cast<Type11Entry*>(type12entry->underlyingEntry());
            charaByteSeq = type11entry->getPublicKey();
        }
        else charaByteSeq = type12entry->getByteSeq();

        // delete characterizing byte sequence
        string key;
        key.push_back('C'); // suffix for characterizing byte sequence
        key.append(util.UllAsByteSeq(charaByteSeq->length()));
        key.append(*charaByteSeq);
        dbList->Delete(rocksdb::WriteOptions(), key);

        delete type12entry;
    }

    CompleteID nextID = firstSignId;
    bool first = true;
    string keyPrefPref;
    string key;
    while (nextID.getNotary() > 0)
    {
        keyPrefPref = "";
        keyPrefPref.append(keyPref);
        keyPrefPref.append(nextID.to20Char());

        // delete reference to first
        key = "";
        key.append(keyPrefPref);
        key.push_back('F'); // suffix for first type 13 entry
        dbList->Delete(rocksdb::WriteOptions(), key);

        // delete actual entry
        key = "";
        key.append(keyPrefPref);
        key.append("AE"); // suffix for entry
        dbList->Delete(rocksdb::WriteOptions(), key);

        // delete signature count
        key = "";
        key.append(keyPrefPref);
        key.append("SC"); // suffix for signature count
        dbList->Delete(rocksdb::WriteOptions(), key);

        if (first)
        {
            key = "";
            key.append(keyPrefPref);
            key.append("TL"); // suffix for time limit
            dbList->Delete(rocksdb::WriteOptions(), key);
            first = false;

            if (renot)
            {
                // get reference to previous notarization entry and delete
                key = "";
                key.append(keyPrefPref);
                key.push_back('P'); // suffix for previous type 13 entry
                string value;
                rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
                const bool success = (s.ok() && value.length()>2);
                if (success)
                {
                    string prevIdStr(value);
                    dbList->Delete(rocksdb::WriteOptions(), key);
                    // delete previous
                    key = "";
                    key.push_back('I'); // prefix for in renotarization
                    key.append(prevIdStr);
                    key.push_back('F'); // suffix for first type 13 entry
                    dbList->Delete(rocksdb::WriteOptions(), key);

                    CompleteID id(prevIdStr);
                    //storeRenotarizationParameters(id);
                    out=id;
                }
            }
        }

        // get reference to next and delete reference
        key = "";
        key.append(keyPrefPref);
        key.push_back('N'); // suffix for next type 13 entry
        string value;
        rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
        const bool success = (s.ok() && value.length()>2);
        if (!success) nextID = CompleteID();
        else nextID = CompleteID(value);
        dbList->Delete(rocksdb::WriteOptions(), key);
    }
    return out;
}

// db must be locked for this
bool Database::removeFromExchangeOffersList(CompleteID &offerId, CompleteID &ownerId, CompleteID &currencyOId, CompleteID &currencyRId)
{
    // get amount requested
    string key;
    string value;
    key.push_back('B'); // prefix to distinguish from list head
    key.append(offerId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append("RA"); // suffix for requested amount
    rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    bool success = (s.ok() && value.length()>2);
    if (!success) return false;
    double amountR = util.byteSeqAsDbl(value);
    // remove amount requested
    notarizationEntries->Delete(rocksdb::WriteOptions(), key);
    // calculate range
    double minTransfer = getLowerTransferLimit(currencyRId);
    if (minTransfer<=0) minTransfer=0.01;
    unsigned short range = util.getRange(amountR, minTransfer);
    // get stored exchange rate
    key="";
    value="";
    key.push_back('B'); // prefix to distinguish from list head
    key.append(offerId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append("ER"); // suffix for exchange ratio
    s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
    success = (s.ok() && value.length()>2);
    if (!success) return false;
    double exchangeRatioStored = util.byteSeqAsDbl(value);
    // remove stored exchange ratio
    notarizationEntries->Delete(rocksdb::WriteOptions(), key);
    // remove EO from publicKeys
    key="";
    key.push_back('I'); // suffix for key identification by id
    key.append(ownerId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append(currencyOId.to20Char());
    key.append(currencyRId.to20Char());
    key.append(util.UsAsByteSeq(range));
    key.append(util.flip(util.DblAsByteSeq(exchangeRatioStored)));
    key.append(offerId.to20Char());
    publicKeys->Delete(rocksdb::WriteOptions(), key);
    // remove EO from currencies
    key="";
    key.push_back('I'); // suffix for key identification by id
    key.append(currencyOId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append(currencyRId.to20Char());
    key.append(util.UsAsByteSeq(range));
    key.append(util.flip(util.DblAsByteSeq(exchangeRatioStored)));
    key.append(offerId.to20Char());
    currenciesAndObligations->Delete(rocksdb::WriteOptions(), key);
    return true;
}

// db must be locked for this
void Database::addToExchangeOffersList(CompleteID &offerId, CompleteID &ownerId, CompleteID &currencyOId, CompleteID &currencyRId, double amountR)
{
    // get range
    double minTransfer = getLowerTransferLimit(currencyRId);
    if (minTransfer<=0) minTransfer=0.01;
    unsigned short range = util.getRange(amountR, minTransfer);
    // get exchange ratio
    double discountRate = getDiscountRate(currencyOId);
    CompleteID zeroId;
    double carriedAmount = getCollectableLiquidity(offerId, ownerId, currencyOId, discountRate, 1, zeroId, systemTimeInMs());
    double exchangeRatio = amountR/carriedAmount;
    // store exchange rate to main list
    string key="";
    key.push_back('B'); // prefix to distinguish from list head
    key.append(offerId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append("ER"); // suffix for exchange ratio
    notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(exchangeRatio));
    // store amount requested
    key="";
    key.push_back('B'); // prefix to distinguish from list head
    key.append(offerId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append("RA"); // suffix for requested amount
    notarizationEntries->Put(rocksdb::WriteOptions(), key, util.DblAsByteSeq(amountR));
    // store to public keys
    key = "";
    key.push_back('I'); // suffix for key identification by id
    key.append(ownerId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append(currencyOId.to20Char());
    key.append(currencyRId.to20Char());
    key.append(util.UsAsByteSeq(range));
    key.append(util.flip(util.DblAsByteSeq(exchangeRatio)));
    key.append(offerId.to20Char());
    publicKeys->Put(rocksdb::WriteOptions(), key, offerId.to20Char());
    // save EO to currencies
    key="";
    key.push_back('I'); // suffix for key identification by id
    key.append(currencyOId.to20Char());
    key.append("EO"); // suffix for exchange offer
    key.append(currencyRId.to20Char());
    key.append(util.UsAsByteSeq(range));
    key.append(util.flip(util.DblAsByteSeq(exchangeRatio)));
    key.append(offerId.to20Char());
    currenciesAndObligations->Put(rocksdb::WriteOptions(), key, offerId.to20Char());
}

// db must be locked for this
CompleteID Database::getEntryInRenotarization(CompleteID &firstNotSignId)
{
    string key;
    string value;
    key.push_back('I'); // prefix for in renotarization
    key.append(firstNotSignId.to20Char());
    key.push_back('P'); // suffix for previous
    rocksdb::Status s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (!success) return CompleteID();
    CompleteID out(value);
    return out;
}

// db must be locked for this
bool Database::addInitialSignature(Type13Entry* entry, Type12Entry* type12entry)
{
    if (entry == nullptr) return false;
    CompleteID predNotId = entry->getPredecessorID();
    CompleteID firstId = entry->getCompleteID();
    string firstIdStr = firstId.to20Char();
    if (firstId != entry->getFirstID()
            || (type12entry == nullptr && predNotId == firstId)
            || (type12entry != nullptr && predNotId != firstId))
    {
        return false;
    }

    string keyPref;
    rocksdb::DB* dbList = entriesInNotarization;
    string key;
    string value;
    if (type12entry == nullptr) // if renotarization
    {
        keyPref.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;

        // predecessor notarization entry: save link to first signature in renotarization
        key = "";
        key.append(keyPref);
        key.append(predNotId.to20Char());
        key.push_back('F'); // suffix for first type 13 entry
        value = "";
        value.append(firstIdStr);
        dbList->Put(rocksdb::WriteOptions(), key, value);
    }
    else
    {
        // get characterizing byte sequence
        string* charaByteSeq;
        int type = type12entry->underlyingType();
        if (type == 5 || type == 15) charaByteSeq = type12entry->underlyingEntry()->getByteSeq();
        else if (type == 11)
        {
            Type11Entry* type11entry = static_cast<Type11Entry*>(type12entry->underlyingEntry());
            charaByteSeq = type11entry->getPublicKey();
        }
        else charaByteSeq = type12entry->getByteSeq();

        // characterizing byte sequence save
        key = "";
        key.push_back('C'); // suffix for characterizing byte sequence
        key.append(util.UllAsByteSeq(charaByteSeq->length()));
        key.append(*charaByteSeq);
        value = "";
        value.append(firstIdStr);
        dbList->Put(rocksdb::WriteOptions(), key, value);

        // save info to general list (based on type)
        if (type == 2)
        {
            key="";
            key.push_back('B'); // prefix for body
            key.append(firstId.to20Char());
            essentialEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
        }
        else if (type == 4)
        {
            Type4Entry* type4entry = static_cast<Type4Entry*>(type12entry->underlyingEntry());
            CompleteID predecessorId = type4entry->getPreviousThreadEntry();
            CompleteID predecessorIdFirst = getFirstID(predecessorId);
            // update thread predecessor data
            key="";
            key.push_back('B'); // prefix to distinguish from list head
            key.append(predecessorIdFirst.to20Char());
            key.push_back('T'); // suffix for thread entry
            key.append("SE");  // suffix for successor entry
            notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());

            // place initiated thread in notarization flag
            if (getApplicationId(predecessorIdFirst) == predecessorIdFirst)
            {
                CompleteID pubKeyID = type12entry->pubKeyID();
                pubKeyID = getFirstID(pubKeyID);
                TNtrNr tNtrNr = getTotalNotaryNr(pubKeyID);
                key="";
                key.push_back('B'); // prefix for body
                key.append(tNtrNr.toString());
                key.append("ITN"); // prefix for initiated thread in notarization
                notaries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
            }
        }
        else if (type == 7)
        {
            // get entry
            Type7Entry* type7entry = static_cast<Type7Entry*>(type12entry->underlyingEntry());
            // set as outflow entry for the claim (if applicable)
            if (type7entry->getOwnStake()>0)
            {
                CompleteID claimId = type7entry->getLiquidityClaim();
                CompleteID claimIdFirst = getFirstID(claimId);
                CompleteID zeroId;
                if (getOutflowEntry(claimIdFirst, zeroId).getNotary()>0)
                {
                    string key;
                    key.push_back('B'); // prefix to distinguish from list head
                    key.append(claimIdFirst.to20Char());
                    key.push_back('C'); // suffix for claim entry
                    key.append("OE"); // suffix for outflow entry
                    notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
                }
            }
        }
        else if (type == 8)
        {
            Type8Entry* type8entry = static_cast<Type8Entry*>(type12entry->underlyingEntry());
            CompleteID predecessorId = type8entry->getPreviousThreadEntry();
            CompleteID predecessorIdFirst = getFirstID(predecessorId);
            CompleteID applicationId = getApplicationId(predecessorIdFirst);
            // if first in thread
            if (predecessorIdFirst == applicationId)
            {
                CompleteID ownerID = type12entry->pubKeyID();
                CompleteID pubKeyID = getFirstID(ownerID);
                CompleteID terminationId = type8entry->getTerminationID();
                CompleteID terminationIdFirst = getFirstID(terminationId);
                string key;
                key.push_back('I'); // suffix for key identification by id
                key.append(pubKeyID.to20Char());
                key.push_back('R'); // suffix for referee information
                key.push_back('T'); // suffix for termination entry
                key.append(terminationIdFirst.to20Char());
                key.push_back('A'); // suffix for last approval (type 8 entry id)
                publicKeys->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
            }
            // set as outflow entry
            CompleteID claimId = type8entry->getLiquidityClaim();
            CompleteID claimIdFirst = getFirstID(claimId);
            string key;
            key.push_back('B'); // prefix to distinguish from list head
            key.append(claimIdFirst.to20Char());
            key.push_back('C'); // suffix for claim entry
            key.append("OE"); // suffix for outflow entry
            notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
            // update thread predecessor data
            key="";
            key.push_back('B'); // prefix to distinguish from list head
            key.append(predecessorIdFirst.to20Char());
            key.push_back('T'); // suffix for thread entry
            key.append("SE");  // suffix for successor entry
            notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
        }
        else if (type == 9)
        {
            Type9Entry* type9entry = static_cast<Type9Entry*>(type12entry->underlyingEntry());
            CompleteID predecessorId = type9entry->getPreviousThreadEntry();
            CompleteID predecessorIdFirst = getFirstID(predecessorId);
            // update thread predecessor data
            key="";
            key.push_back('B'); // prefix to distinguish from list head
            key.append(predecessorIdFirst.to20Char());
            key.push_back('T'); // suffix for thread entry
            key.append("SE");  // suffix for successor entry
            notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
        }
        else if (type == 10)
        {
            // get entry
            Type10Entry* type10entry = static_cast<Type10Entry*>(type12entry->underlyingEntry());
            // set as outflow entry for the claim (if applicable)
            CompleteID claimId = type10entry->getSource();
            CompleteID claimIdFirst = getFirstID(claimId);
            CompleteID zeroId;
            if (getOutflowEntry(claimIdFirst, zeroId).getNotary()>0)
            {
                string key;
                key.push_back('B'); // prefix to distinguish from list head
                key.append(claimIdFirst.to20Char());
                key.push_back('C'); // suffix for claim entry
                key.append("OE"); // suffix for outflow entry
                notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
            }
            // set connected transfer for the other entry (if applicable)
            CompleteID transferId = type10entry->getTarget();
            CompleteID transferIdFirst = getFirstID(transferId);
            if (getConnectedTransfer(transferIdFirst, zeroId).getNotary()>0)
            {
                string key;
                key.push_back('B'); // prefix to distinguish from list head
                key.append(transferIdFirst.to20Char());
                key.append("LT"); // suffix for liquidity transfer entry information
                key.append("CT"); // suffix for the connected transfer
                notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
            }
        }
        else if (type == 14) // save info to public keys as well
        {
            Type14Entry* type14entry = static_cast<Type14Entry*>(type12entry->underlyingEntry());
            CompleteID ownerId = type14entry->getOwnerID();
            CompleteID ownerIdFirst = getFirstID(ownerId);
            CompleteID currencyId = type14entry->getCurrencyOrObligation();
            unsigned long long claimCount = getClaimCount(ownerIdFirst, currencyId);
            // store claim to list
            string key;
            key.push_back('I'); // suffix for key identification by id
            key.append(ownerIdFirst.to20Char());
            key.push_back('C'); // suffix for liquidity claims
            key.append(currencyId.to20Char());
            key.push_back('B'); // suffix for body
            key.append(util.UllAsByteSeq(claimCount));
            publicKeys->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
            // store collecting claims for predecessors
            const unsigned short predecessorCount = type14entry->getPredecessorsCount();
            for (unsigned short i=0; i<predecessorCount; i++)
            {
                unsigned char scenario = type14entry->getScenario(i);
                CompleteID predecessorId = type14entry->getPredecessor(i);
                CompleteID predecessorIdFirst = getFirstID(predecessorId);
                if (scenario != 1)
                {
                    key="";
                    key.push_back('B'); // prefix to distinguish from list head
                    key.append(predecessorIdFirst.to20Char());
                    key.append("GCC"); // suffix for get collecting claims
                    key.append(currencyId.to20Char());
                    key.append(ownerIdFirst.to20Char());
                    key.append(util.UcAsByteSeq(scenario));
                    notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
                }
                else
                {
                    key="";
                    key.push_back('B'); // prefix to distinguish from list head
                    key.append(predecessorIdFirst.to20Char());
                    key.append("LT"); // suffix for liquidity transfer entry information
                    key.append("UC"); // suffix for using claim
                    notarizationEntries->Put(rocksdb::WriteOptions(), key, firstId.to20Char());
                }
            }
        }
    }

    keyPref.append(firstIdStr);

    // save first
    key = "";
    key.append(keyPref);
    key.push_back('F'); // suffix for first type 13 entry
    value = "";
    value.append(firstIdStr);
    dbList->Put(rocksdb::WriteOptions(), key, value);

    // save actual entry
    key = "";
    key.append(keyPref);
    key.append("AE"); // suffix for entry
    dbList->Put(rocksdb::WriteOptions(), key, *entry->getByteSeq());

    if (type12entry == nullptr) // if renotarization
    {
        // save previous
        key = "";
        key.append(keyPref);
        key.push_back('P'); // suffix for previous type 13 entry
        value = "";
        value.append(predNotId.to20Char());
        dbList->Put(rocksdb::WriteOptions(), key, value);
    }

    // save signatures count
    key = "";
    key.append(keyPref);
    key.append("SC"); // suffix for signature count
    value = util.UlAsByteSeq(1);
    dbList->Put(rocksdb::WriteOptions(), key, value);

    // save time limit
    key = "";
    key.append(keyPref);
    key.append("TL"); // suffix for time limit
    unsigned long long timeLimit = deduceTimeLimit(type12entry, firstId, predNotId);
    value = util.UllAsByteSeq(timeLimit);
    dbList->Put(rocksdb::WriteOptions(), key, value);

    return true;
}

// db must be locked for this
bool Database::isConflicting(CompleteID &firstID)
{
    string key;
    string value;
    key.append(firstID.to20Char());
    rocksdb::Status s = conflicts->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    return success;
}

// db must be locked for this
void Database::registerConflicts(CompleteID &badEntryId)
{
    if (conflictingEntries.isEmpty())
    {
        return;
    }
    if (badEntryId.getNotary()<=0 || type1entry->getLineage(badEntryId.getTimeStamp()) != type1entry->latestLin())
    {
        return;
    }
    conflictingEntries.add(badEntryId);
    set<CompleteID, CompleteID::CompareIDs> *entriesSet = conflictingEntries.getSetPointer();
    set<CompleteID, CompleteID::CompareIDs>::iterator it;
    for (it = entriesSet->begin(); it != entriesSet->end(); ++it)
    {
        CompleteID id = *it;
        string key(id.to20Char());
        string value(key);
        conflicts->Put(rocksdb::WriteOptions(), key, value);
    }
    conflictingEntries.clear();
}

// db must be locked for this
unsigned long long Database::deduceTimeLimit(Type12Entry* type12entry, CompleteID &firstId, CompleteID &predNotId)
{
    unsigned long long timeLimit = type1entry->getNotarizationTimeLimit(firstId.getTimeStamp());
    if (type12entry!=nullptr)
    {
        timeLimit = min(timeLimit, type12entry->getNotarizationTimeLimit());
        // correct time limit for type9entries based on time windows (if applicable)
        if (type12entry->underlyingType() == 9)
        {
            Type9Entry* type9entry = static_cast<Type9Entry*>(type12entry->underlyingEntry());
            CompleteID predecessorId = type9entry->getPreviousThreadEntry();
            CompleteID predecessorIdFirst = getFirstID(predecessorId);
            if (type1entry->getLineage(predecessorIdFirst.getTimeStamp()) == type1entry->latestLin())
            {
                unsigned long long scheduledTerminationTime = getTerminationTime(predecessorIdFirst);
                if (scheduledTerminationTime > 0)
                {
                    unsigned long long timeDiff = firstId.getTimeStamp() - scheduledTerminationTime;
                    unsigned long long timeStep = type1entry->getLatestMaxNotarizationTime();
                    unsigned long c = timeDiff / timeStep;
                    unsigned long long nextAttempt = scheduledTerminationTime + timeStep * (c+1);
                    timeLimit = min(timeLimit, nextAttempt);
                }
            }
        }
    }
    else    // if renotarization
    {
        if (type1entry->getLineage(predNotId.getTimeStamp()) == type1entry->latestLin())
        {
            unsigned long long earliestRenotTime = getEarliestRenotTime(predNotId);
            if (earliestRenotTime != ULLONG_MAX)
            {
                unsigned long long timeDiff = firstId.getTimeStamp() - earliestRenotTime;
                unsigned long long timeStep = type1entry->getLatestMaxNotarizationTime();
                unsigned long c = timeDiff / timeStep;
                unsigned long long nextAttempt = earliestRenotTime + timeStep * (c+1);
                timeLimit = min(timeLimit, nextAttempt);
            }
        }
    }
    return timeLimit;
}

Type1Entry* Database::getType1Entry()
{
    return type1entry;
}

// db must be locked for this
bool Database::addNextSignature(Type13Entry* entry, bool renot)
{
    CompleteID predecessorId = entry->getPredecessorID();
    unsigned short linPred = type1entry->getLineage(predecessorId.getTimeStamp());
    CompleteID entryID = entry->getCompleteID();
    unsigned short linEntry = type1entry->getLineage(entryID.getTimeStamp());
    if (linPred == 0 || linEntry == 0 || linEntry != linPred)
    {
        return false;
    }

    string keyPref;
    string key;
    string value;
    rocksdb::DB* dbList = entriesInNotarization;
    if (renot)
    {
        keyPref.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;
    }
    string entryIdStr = entryID.to20Char();
    string predIdStr = predecessorId.to20Char();

    // save next to previous
    key = "";
    key.append(keyPref);
    key.append(predIdStr);
    key.push_back('N'); // suffix for next type 13 entry
    // check if a successor already exists
    value = "";
    rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
    const bool success = (s.ok() && value.length()>2);
    if (success) return false;
    // save next to previous
    value = "";
    value.append(entryIdStr);
    dbList->Put(rocksdb::WriteOptions(), key, value);

    // update prefix
    keyPref.append(entryIdStr);

    // get first id and signature count
    CompleteID firstEntryId = getFirstNotSignId(predecessorId, renot);
    unsigned long sigCount = getSignatureCount(predecessorId, renot);

    if (firstEntryId.getNotary() <= 0 || sigCount <= 0) return false;

    // save first id
    key = "";
    key.append(keyPref);
    key.push_back('F'); // suffix for first type 13 entry
    value = "";
    value.append(firstEntryId.to20Char());
    dbList->Put(rocksdb::WriteOptions(), key, value);

    // save signatures count
    key = "";
    key.append(keyPref);
    key.append("SC"); // suffix for signature count
    value = util.UlAsByteSeq(sigCount+1);
    dbList->Put(rocksdb::WriteOptions(), key, value);

    // save actual entry
    key = "";
    key.append(keyPref);
    key.append("AE"); // suffix for entry
    dbList->Put(rocksdb::WriteOptions(), key, *entry->getByteSeq());

    return true;
}

// db must be locked for this
bool Database::loadUnderlyingType12EntryStr(CompleteID &entryId, unsigned char l, string &str)
{
    string T13Str;
    if (l==0)
    {
        CompleteID firstId = getFirstID(entryId);
        if (!loadInitialT13EntryStr(firstId, T13Str)) return false;
    }
    else if (l==1)
    {
        CompleteID firstNotSignId = getFirstNotSignId(entryId, false);
        if (!loadType13EntryStr(firstNotSignId, l, T13Str)) return false;
    }
    else
    {
        CompleteID firstNotSignId = getFirstNotSignId(entryId, true);
        CompleteID notEntryId = getEntryInRenotarization(firstNotSignId);
        CompleteID firstId = getFirstID(notEntryId);
        if (!loadInitialT13EntryStr(firstId, T13Str)) return false;
    }
    Type12Entry *type12entry = createT12FromT13Str(T13Str);
    if (type12entry==nullptr) return false;
    str.append(*type12entry->getByteSeq());
    delete type12entry;
    return true;
}

// db must be locked for this
// returns the actual type 13 entry behind an id
bool Database::loadType13EntryStr(CompleteID &entryId, unsigned char l, string &str)
{
    if (str.length()>0) return false;
    string key;
    rocksdb::Status s;
    if (l==0)
    {
        key.push_back('B'); // prefix to distinguish from list head
        key.append(entryId.to20Char());
        key.append("AE"); // suffix for actual entry
        s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &str);
    }
    else if (l==1)
    {
        key.append(entryId.to20Char());
        key.append("AE"); // suffix for actual entry
        s = entriesInNotarization->Get(rocksdb::ReadOptions(), key, &str);
    }
    else
    {
        key.push_back('I'); // prefix for in renotarization
        key.append(entryId.to20Char());
        key.append("AE"); // suffix for actual entry
        s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &str);
    }
    return (s.ok() && str.length()>2);
}

// db must be locked for this
bool Database::verifySignature(string &signedSequence, string &signatureStr, unsigned long notaryNum, unsigned long long timeStamp)
{
    TNtrNr totalNotaryNr = type1entry->getTotalNotaryNr(notaryNum, timeStamp);
    string pubKeyStr;
    if (!loadNotaryPubKey(totalNotaryNr, pubKeyStr))
    {
        if (correspondingNotary != totalNotaryNr) return false;
        CryptoPP::SecByteBlock signature((const byte*)signatureStr.c_str(), signatureStr.length());
        CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA3_384>::Verifier verifier(*correspondingNotaryPublicKey);
        return verifier.VerifyMessage((const byte*)signedSequence.c_str(), signedSequence.length(), signature, signature.size());
    }
    CompleteID notaryPubKeyId = getNotaryId(totalNotaryNr);
    if (notaryPubKeyId.getNotary() > 0 && getValidityDate(notaryPubKeyId) < timeStamp) return false;
    CryptoPP::SecByteBlock signature((const byte*)signatureStr.c_str(), signatureStr.length());
    CryptoPP::RSA::PublicKey publicKey;
    CryptoPP::ByteQueue bq;
    bq.Put((const byte*)pubKeyStr.c_str(), pubKeyStr.length());
    publicKey.Load(bq);
    CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA3_384>::Verifier verifier(publicKey);
    return verifier.VerifyMessage((const byte*)signedSequence.c_str(), signedSequence.length(), signature, signature.size());
}

// db must be locked for this
bool Database::amModerator(CompleteID &firstID)
{
    if (type1entry->getLineage(firstID.getTimeStamp()) != type1entry->latestLin()) return false;
    if (ownNumber != firstID.getNotary()) return false;
    CompleteID firstNotSignId = getFirstNotSignId(firstID, false);
    if (firstNotSignId.isZero()) firstNotSignId = getFirstNotSignId(firstID, true);
    if (firstNotSignId != firstID) return false;
    return true;
}

// db must be locked for this
size_t Database::getExistingEntriesIds(CompleteID &id, list<CompleteID> &idsList, bool renot)
{
    if (idsList.size()>0) return 0;

    string keyPref;
    rocksdb::DB* dbList = entriesInNotarization;
    if (renot)
    {
        keyPref.push_back('I'); // prefix for in renotarization
        dbList = subjectToRenotarization;
    }

    CompleteID nextID = getFirstNotSignId(id, renot);
    if (nextID.getNotary() <= 0) return 0;

    string key;
    while (nextID.getNotary() > 0)
    {
        idsList.push_back(nextID);

        // get reference to next
        key = "";
        key.append(keyPref);
        key.append(nextID.to20Char());
        key.push_back('N'); // suffix for next type 13 entry
        string value;
        rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
        const bool success = (s.ok() && value.length()>2);
        if (!success) nextID = CompleteID();
        else nextID = CompleteID(value);
    }

    return idsList.size();
}

// db must be locked for this
bool Database::addType13Entry(Type13Entry* entry, bool integrateIfPossible)
{
    if (entry == nullptr || !entry->isGood()) return false;
    conflictingEntries.clear();
    // do some basic checks
    CompleteID entryID = entry->getCompleteID();
    if (isInGeneralList(entryID)) return false;
    unsigned long long entryTime = entryID.getTimeStamp();
    if (entryTime > systemTimeInMs() + 2000) return false;
    unsigned short linCurrent = type1entry->getLineage(entryTime);
    if (linCurrent == 0) return false;

    CompleteID predecessorID = entry->getPredecessorID();
    unsigned short linPred = type1entry->getLineage(predecessorID.getTimeStamp());
    if (linPred == 0 || linPred>linCurrent) return false;

    CompleteID firstID = entry->getFirstID();
    unsigned short linFirst = type1entry->getLineage(firstID.getTimeStamp());
    if (linFirst == 0) return false;
    if (linFirst!=linCurrent && entryID!=type1entry->getConfirmationId()) return false;
    if (linFirst==linCurrent && entryID==type1entry->getConfirmationId()) return false;

    // extract some information about the notarization type
    unsigned long signatureCountPred = 0;
    // signature count of the predecessor in the notarizing block
    bool renot = false; // are we in a renotarization
    unsigned char predSourceType = 3;
    // 3 for new notarization (no predecessor)
    // 0 for notarization entry as predecessor
    // 1 for predecessor is in first notarizing block
    // 2 for predecessor is in a renotarizing block
    if (predecessorID != entryID)
    {
        predSourceType = 1;
        signatureCountPred = getSignatureCount(predecessorID, predSourceType);
        if (signatureCountPred <= 0)
        {
            renot = true;
            predSourceType = 0;
            signatureCountPred = getSignatureCount(predecessorID, predSourceType);
            if (signatureCountPred > 0) // predecessor is notarization entry
            {
                signatureCountPred = 0;
            }
            else
            {
                predSourceType = 2;
                signatureCountPred = getSignatureCount(predecessorID, predSourceType);
                if (signatureCountPred <= 0)
                {
                    // schedule download of predecessorID
                    if (firstID == entryID && predecessorID != firstID && !isInGeneralList(predecessorID))
                    {
                        insertEntryToDownload(predecessorID, 0, 0);
                        addToMissingPredecessors(entryID, predecessorID);
                    }
                    return false;
                }
            }
        }
    }

    // some more basic checks
    if (predSourceType == 3 && entryID != firstID) return false;
    if (predSourceType == 0 && (entryID != firstID || predecessorID >= entryID)) return false;
    if (predSourceType == 1 || predSourceType == 2)
    {
        if(firstID >= entryID || predecessorID >= entryID || predecessorID < firstID) return false;
        // check correctness of firstID
        if (getFirstNotSignId(predecessorID, renot) != firstID) return false;
    }

    // checks for renotarizations
    if (predSourceType == 0)
    {
        // check for freshness of predecessor notarization entry
        if (!isInGeneralList(predecessorID) || !type1entry->isFresh(predecessorID, entryTime))
        {
            puts("Database::addType13Entry: bad predecessor");
            return false;
        }
        // check that predecessor is the latest in sequence
        CompleteID predecessorIDFirst = getFirstID(predecessorID);
        CompleteID predecessorIDLatest = getLatestID(predecessorIDFirst);
        if (predecessorIDLatest != predecessorID)
        {
            return false;
        }
    }

    // check the notary
    unsigned long notaryNr = entry->getNotary();
    if (!isActingNotary(notaryNr, entryTime))
    {
        //puts("Database::addType13Entry: missing notary");
        addToMissingNotaries(entryID, notaryNr);
        return false;
    }

    // get public key of notary
    TNtrNr totalNotaryNr = type1entry->getTotalNotaryNr(notaryNr, entryTime);
    CompleteID notaryPubKeyId = getNotaryId(totalNotaryNr);
    if (notaryPubKeyId.getNotary() > 0 && getValidityDate(notaryPubKeyId) < entryTime)
        return false;
    string pubKeyStr;
    if (!loadNotaryPubKey(totalNotaryNr, pubKeyStr)) return false;

    // check if signature is correct
    string signedSeq(entryID.to20Char());
    signedSeq.append(predecessorID.to20Char());
    signedSeq.append(firstID.to20Char());
    string type12Str;
    if (predSourceType == 3)
    {
        Type12Entry *t12e = createT12FromT13Str(*entry->getByteSeq());
        if (t12e==nullptr)
        {
            return false;
        }
        // check that first notary is the right one
        if (entryID.getNotary() != t12e->getFirstNotaryNr())
        {
            puts("Database::addType13Entry: wrong first notary");
            delete t12e;
            return false;
        }
        // get byte sequence
        type12Str.append(*t12e->getByteSeq());
        delete t12e;
        signedSeq.append(type12Str);
    }
    else
    {
        if (!loadUnderlyingType12EntryStr(predecessorID, predSourceType, type12Str))
        {
            puts("Database::addType13Entry: loadUnderlyingType12EntryStr failed");
            return false;
        }
        signedSeq.append(type12Str);
        string type13Str;
        if (!loadType13EntryStr(predecessorID, predSourceType, type13Str))
        {
            puts("Database::addType13Entry: loadType13EntryStr failed");
            return false;
        }
        signedSeq.append(type13Str);
    }
    string* signa = entry->getSignature();

    CryptoPP::SecByteBlock signature((const byte*)signa->c_str(), signa->length());
    CryptoPP::RSA::PublicKey publicKey;
    CryptoPP::ByteQueue bq;
    bq.Put((const byte*)pubKeyStr.c_str(), pubKeyStr.length());
    publicKey.Load(bq);
    CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA3_384>::Verifier verifier(publicKey);

    bool correct = verifier.VerifyMessage((const byte*)signedSeq.c_str(),
                                          signedSeq.length(), signature, signature.size());
    if (!correct)
    {
        puts("Database::addType13Entry: signature verification failed");
        return false;
    }

    // check time limitations
    if (predSourceType == 1 || predSourceType == 2)
    {
        if (entryID != type1entry->getConfirmationId() && entryTime >= getNotTimeLimit(firstID, renot))
        {
            puts("Database::addType13Entry: notarization time limit would be violated");
            return false;
        }
    }

    bool alreadyKnown = false;
    // check for conflict based on type12Str
    if (predSourceType==3)
    {
        string key;
        string value;

        key.push_back('C'); // prefix for characterizing byte sequence
        key.append(util.UllAsByteSeq(type12Str.length()));
        key.append(type12Str);
        rocksdb::Status s = notarizationEntries->Get(rocksdb::ReadOptions(), key, &value);
        bool success = (s.ok() && value.length()>2);
        if (success)
        {
            puts("Database::addType13Entry: conflict based on type12Str");
            return false;
        }

        // get characterizing byte sequence
        string* charaByteSeq;
        Type12Entry type12entry(type12Str);
        int type = type12entry.underlyingType();
        if (type == 5 || type == 15) charaByteSeq = type12entry.underlyingEntry()->getByteSeq();
        else if (type == 11)
        {
            Type11Entry* type11entry = static_cast<Type11Entry*>(type12entry.underlyingEntry());
            charaByteSeq = type11entry->getPublicKey();
        }
        else charaByteSeq = &type12Str;

        key = "";
        value = "";
        key.push_back('C'); // prefix for characterizing byte sequence
        key.append(util.UllAsByteSeq(charaByteSeq->length()));
        key.append(*charaByteSeq);
        s = entriesInNotarization->Get(rocksdb::ReadOptions(), key, &value);
        success = (s.ok() && value.length()>2);

        if (success)
        {
            CompleteID conflictingEntryId(value);
            if (conflictingEntryId==entryID) alreadyKnown = true;
            else verifyConflict(conflictingEntryId, entryID);
        }
    }
    else if (signatureCountPred == 0) // check for conflict based on firstID:
    {
        string key;
        string value;
        key.push_back('I'); // prefix for in renotarization
        key.append(predecessorID.to20Char());
        key.push_back('F'); // suffix for first type 13 entry
        rocksdb::Status s = subjectToRenotarization->Get(rocksdb::ReadOptions(), key, &value);
        const bool success = (s.ok() && value.length()>2);
        if (success)
        {
            CompleteID conflictingEntryId(value);
            if (conflictingEntryId == entryID) alreadyKnown = true;
            else verifyConflict(conflictingEntryId, entryID);
        }
    }

    // check that predecessor does not have a successor yet
    if (predSourceType == 1 || predSourceType == 2)
    {
        string key;
        rocksdb::DB* dbList = entriesInNotarization;
        if (renot)
        {
            key.push_back('I'); // prefix for in renotarization
            dbList = subjectToRenotarization;
        }
        key.append(predecessorID.to20Char());
        key.push_back('N'); // suffix for next type 13 entry
        string value;
        rocksdb::Status s = dbList->Get(rocksdb::ReadOptions(), key, &value);
        const bool success = (s.ok() && value.length()>2);
        if (success)
        {
            CompleteID conflictingEntryId(value);
            if (conflictingEntryId == entryID)
            {
                alreadyKnown = true;
            }
            else
            {
                puts("Database::addType13Entry: conflictingEntryId found");
                return false;
            }
        }
    }

    bool integrationOccurred = false;

    // integrate if sufficient signatures already known and integration allowed now
    if (alreadyKnown)
    {
        // integrate if new notarization entry
        if (integrateIfPossible && type1entry->notarizationStatus(signatureCountPred+1, entryTime) == 2 && linPred == type1entry->latestLin())
        {
            if (!integrateNewNotEntry(firstID, renot, nullptr))
            {
                deleteSignatures(firstID, renot);
                return false;
            }
            integrationOccurred = true;
            CompleteID id = deleteSignatures(firstID, renot);
            deleteRenotarizationParameters(id);
            storeRenotarizationParameters(firstID);
        }
        else return true;
    }

    if (!integrationOccurred)
    {
        // check if any predecessor entries for this entry are missing
        Type12Entry type12entry(type12Str);
        bool predecessorsMissing=false;
        if (predSourceType == 3)
        {
            // check pubKeyID for existence (and schedule download)
            CompleteID pubKeyID = type12entry.pubKeyID();
            if (pubKeyID.getNotary()>0 && !isInGeneralList(pubKeyID))
            {
                insertEntryToDownload(pubKeyID, 0, 0);
                addToMissingPredecessors(entryID, pubKeyID);
                predecessorsMissing=true;
            }
            Entry *underlyingEntry = type12entry.underlyingEntry();
            // check referenced entries for existence (and schedule download)
            list<CompleteID>* referencedEntries = underlyingEntry->getReferencedEntries();
            list<CompleteID>::iterator it;
            if (referencedEntries!=nullptr)
            {
                for (it=referencedEntries->begin(); it!=referencedEntries->end(); ++it)
                {
                    if ((*it).getNotary()>0 && !isInGeneralList(*it))
                    {
                        insertEntryToDownload(*it, 0, 0);
                        addToMissingPredecessors(entryID, *it);
                        predecessorsMissing=true;
                    }
                }
            }
            // check referenced currencies and obligations for existence (and schedule download)
            list<CompleteID>* referencedCaO = underlyingEntry->getCurrenciesAndObligations();
            if (referencedCaO!=nullptr)
            {
                for (it=referencedCaO->begin(); it!=referencedCaO->end(); ++it)
                {
                    if ((*it).getNotary()>0 && !isInGeneralList(*it))
                    {
                        insertEntryToDownload(*it, 0, 0);
                        addToMissingPredecessors(entryID, *it);
                        predecessorsMissing=true;
                    }
                }
            }
        }

        // content based checks and integration
        if (entryID == type1entry->getConfirmationId()) // integrate if old lineage filter passed
        {
            if (signatureCountPred > 0 && type1entry->notarizationStatus(signatureCountPred, predecessorID.getTimeStamp()) == 2)
            {
                if (!integrateIfPossible) return true;

                if (!integrateNewNotEntry(firstID, renot, entry))
                {
                    deleteSignatures(firstID, renot);
                    return false;
                }
                integrationOccurred = true;
                CompleteID id = deleteSignatures(firstID, renot);
                deleteRenotarizationParameters(id);
                storeRenotarizationParameters(firstID);
            }
            else
            {
                return false;
            }
        }
        else // add normal signature
        {
            if (predecessorsMissing)
            {
                return false;
            }

            // lineage check
            if (predSourceType!=0 && linPred!=linCurrent)
            {
                return false;
            }

            if (signatureCountPred == 0) // start new notarization or renotarization
            {
                if (!renot)
                {
                    // check underlying entry for consistency
                    bool checkResult = checkForConsistency(&type12entry, entryID);
                    if (!checkResult)
                    {
                        puts("checkForConsistency failed in addType13Entry");
                        puts(entryID.to27Char().c_str());
                        return false;
                    }

                    if (conflictingEntries.isEmpty())
                    {
                        // start new notarization
                        if (!addInitialSignature(entry, &type12entry))
                        {
                            puts("addInitialSignature failed in addType13Entry");
                            return false;
                        }
                        // check that time limit is ok
                        if (entryTime >= getNotTimeLimit(firstID, renot))
                        {
                            puts("timeLimit reached in addType13Entry");
                            deleteSignatures(firstID, renot);
                            return false;
                        }
                    }
                    else
                    {
                        puts("Database::addType13Entry: conflicting Entries exist, failure");
                        return false;
                    }
                }
                else
                {
                    if (type1entry->getLineage(predecessorID.getTimeStamp()) != type1entry->getLineage(entryTime))
                    {
                        if (notaryNr != 1) return false; // must be the first notary if lineage break
                    }
                    else if (type1entry->getLineage(predecessorID.getTimeStamp()) == type1entry->latestLin())
                    {
                        // check if renotarization fits schedule
                        unsigned long long earliestRenotTime = getEarliestRenotTime(predecessorID);
                        if (earliestRenotTime == ULLONG_MAX)
                        {
                            deleteRenotarizationParameters(predecessorID);
                            storeRenotarizationParameters(predecessorID);
                            earliestRenotTime = getEarliestRenotTime(predecessorID);
                        }
                        if (earliestRenotTime != ULLONG_MAX && entryTime >= earliestRenotTime)
                        {
                            // make sure that notary fits the schedule
                            unsigned long long timeDiff = entryTime - earliestRenotTime;
                            unsigned long long timeStep = type1entry->getLatestMaxNotarizationTime();
                            unsigned long c = timeDiff / timeStep;
                            c = c % getNotariesListLength(predecessorID, true);
                            unsigned long correctNotary = getNotaryInSchedule(predecessorID, c, true);
                            if (notaryNr != correctNotary)
                            {
                                puts("Database::addType13Entry: renotarization does not fit schedule 1");
                                return false;
                            }
                        }
                        else
                        {
                            puts("Database::addType13Entry: renotarization does not fit schedule 2");
                            return false;
                        }
                    }

                    if (conflictingEntries.isEmpty())
                    {
                        // start renotarization
                        if (!addInitialSignature(entry, nullptr))
                        {
                            return false;
                        }
                        // check that time limit is ok
                        if (entryTime >= getNotTimeLimit(firstID, renot))
                        {
                            puts("timeLimit reached in addType13Entry");
                            deleteSignatures(firstID, renot);
                            return false;
                        }
                    }
                    else
                    {
                        puts("Database::addType13Entry: conflicting Entries exist, failure");
                        return false;
                    }
                }
            }
            else // add next signature
            {
                if (!addNextSignature(entry, renot))
                {
                    puts("Database::addType13Entry: addNextSignature failed");
                    return false;
                }
            }

            // integrate if new notari entry
            if (integrateIfPossible && type1entry->notarizationStatus(signatureCountPred+1, entryTime) == 2 && linPred == type1entry->latestLin())
            {
                if (!integrateNewNotEntry(firstID, renot, nullptr))
                {
                    deleteSignatures(firstID, renot);
                    return false;
                }
                integrationOccurred = true;
                CompleteID id = deleteSignatures(firstID, renot);
                deleteRenotarizationParameters(id);
                storeRenotarizationParameters(firstID);
            }
        }
    }

    // update download and up-to-date info
    if (!integrationOccurred)
    {
        return true;
    }

    // clean up download schedule
    if (lastDownloadAttempt.count(firstID)>0)
    {
        lastDownloadAttempt.erase(firstID);
    }
    if (missingPredecessors.count(firstID)>0)
    {
        delete missingPredecessors[firstID];
        missingPredecessors.erase(firstID);
    }
    if (missingNotaries.count(firstID)>0)
    {
        delete missingNotaries[firstID];
        missingNotaries.erase(firstID);
    }
    DownloadStatus* status;
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesInD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload;
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesToD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload;
    if ((*entriesToD).count(firstID)>0)
    {
        status = (*entriesToD)[firstID];
        (*entriesToD).erase(firstID);
    }
    else if ((*entriesInD).count(firstID)>0)
    {
        status = (*entriesInD)[firstID];
        (*entriesInD).erase(firstID);
    }
    else return true;

    getUpToDateID(0);
    getUpToDateID(1);
    getUpToDateID(2);
    getUpToDateID(3);
    getUpToDateID(4);

    set<pair<unsigned char,unsigned long>>::iterator it;
    for (it = status->neededFor.begin(); it != status->neededFor.end(); ++it)
    {
        UpToDateTimeInfo* info = getInfoFromType(it->first);
        unsigned long notary = it->second;
        if (info->conditionalUpToDates.count(notary)>0)
        {
            UpToDateCondition* cond = info->conditionalUpToDates[notary];
            if (cond->missingEntries.count(firstID)>0) cond->missingEntries.erase(firstID);
            if (cond->missingEntries.empty())
            {
                info->conditionalUpToDates.erase(notary);
                updateIndividualUpToDate(info, notary, cond->upToDateIDConditional);
                delete cond;
            }
        }
    }
    delete status;

    if (correctUpToDateTime(listEssentials)) saveUpToDateTime(0);
    if (correctUpToDateTime(listGeneral)) saveUpToDateTime(1);
    if (correctUpToDateTime(listTerminations)) saveUpToDateTime(2);
    if (correctUpToDateTime(listPerpetuals)) saveUpToDateTime(3);
    if (correctUpToDateTime(listTransfers)) saveUpToDateTime(4);

    return true;
}

// db to be locked for this
void Database::updateIndividualUpToDate(UpToDateTimeInfo* info, unsigned long notary, CompleteID id)
// correctUpToDateTime must be called after this (before unlocking the db)
{
    CompleteID oldID = info->individualUpToDates[notary];
    info->individualUpToDates[notary] = id;
    info->individualUpToDatesByID.erase(pair<CompleteID,unsigned long>(oldID, notary));
    info->individualUpToDatesByID.insert(pair<CompleteID,unsigned long>(id, notary));
    CompleteID zeroID(0,0,0);
    info->getUpToDateIDOverall()->resetTo(zeroID); // upToDateIDOverall has to be recalculated
}

// db to be locked for this
bool Database::correctUpToDateTime(UpToDateTimeInfo* info)
{
    if (!info->getUpToDateIDOverall()->isZero()) return false; // upToDateIDOverall is not subject to recalculation yet
    set<pair<CompleteID,unsigned long>, CompleteID::CompareIDIntPairs> *cIDs = &info->individualUpToDatesByID;
    if (cIDs->size() <= 0) return false;
    set<pair<CompleteID,unsigned long>, CompleteID::CompareIDIntPairs>::iterator it = cIDs->begin();
    advance(it, cIDs->size()/2);
    info->getUpToDateIDOverall()->resetTo(it->first);
    return true;
}

Database::UpToDateTimeInfo::UpToDateTimeInfo(CompleteID u) : upToDateIDOverall(u)
{

}

CompleteID* Database::UpToDateTimeInfo::getUpToDateIDOverall()
{
    return (CompleteID*) &upToDateIDOverall;
}

Database::UpToDateTimeInfo::~UpToDateTimeInfo()
{
    map<unsigned long, UpToDateCondition*>::iterator it;
    for (it = conditionalUpToDates.begin(); it != conditionalUpToDates.end(); ++it)
    {
        UpToDateCondition* cond = it->second;
        delete cond;
    }
    conditionalUpToDates.clear();
    individualUpToDates.clear();
    individualUpToDatesByID.clear();
}

Database::UpToDateCondition::UpToDateCondition(CompleteID &id1, CompleteID &id2) : upToDateIDConditional(id2)
{
    if (id1.getNotary()>0) missingEntries.insert(id1);
    if (id2.getNotary()>0) missingEntries.insert(id2);
}

Database::UpToDateCondition::~UpToDateCondition()
{
    missingEntries.clear();
}

Database::UpToDateTimeInfo* Database::getInfoFromType(unsigned char listType)
{
    if (listType==0) return listEssentials;
    else if (listType==1) return listGeneral;
    else if (listType==2) return listTerminations;
    else if (listType==3) return listPerpetuals;
    else if (listType==4) return listTransfers;
    else return nullptr;
}

// db to be locked for this
// returns new individual up-to-date-id or zero if need to download
CompleteID Database::newEntriesIdsReport(unsigned char listType, unsigned long notary, CompleteID id1, CompleteID id2)
{
    if (listType<0 || listType>4 || notary<=0) return CompleteID(0,0,0);
    if (id1>id2) return CompleteID(0,0,0);
    if (id1.getNotary()==0 && id2.getNotary()>0) return CompleteID(0,0,0);
    if (id1 == id2 && id1.getNotary()>0) // just download
    {
        if (!isInGeneralList(id2)) insertEntryToDownload(id2, listType, 0);
        return CompleteID(0,0,0);
    }

    // schedule for download (without neededFor)
    if (id1.getNotary()>0 && !isInGeneralList(id1))
    {
        insertEntryToDownload(id1, listType, 0);
    }
    if (id2!=id1 && id2.getNotary()>0 && !isInGeneralList(id2))
    {
        insertEntryToDownload(id2, listType, 0);
    }

    // schedule conditional up-to-date:
    getUpToDateID(listType);
    UpToDateTimeInfo* info=getInfoFromType(listType);
    // return if old condition satisfied
    if (info->individualUpToDates.count(notary)>0 && id2 <= info->individualUpToDates[notary])
    {
        if (id2.getNotary()<=0) return CompleteID(0,0,0);
        CompleteID out = info->individualUpToDates[notary];
        if (lastReportedIndividualUpToDate.count(notary) > 0
                && lastReportedIndividualUpToDate[notary] >= out)
        {
            lastReportedIndividualUpToDate.erase(notary);
            return CompleteID(0,0,0);
        }
        if (lastReportedIndividualUpToDate.count(notary) > 0)
        {
            lastReportedIndividualUpToDate[notary] = out;
        }
        else
        {
            lastReportedIndividualUpToDate.insert(pair<unsigned long, CompleteID>(notary, out));
        }
        return out;
    }
    else if (info->individualUpToDates.count(notary)<=0)
    {
        return CompleteID(0,0,0);
    }
    else if (id1.getNotary() == 0 || isInGeneralList(id1))
    {
        id1=id2;
        if (id1.getNotary() == 0 || isInGeneralList(id1))
        {
            // delete old condition if existent
            if (info->conditionalUpToDates.count(notary)>0)
            {
                UpToDateCondition* cond = info->conditionalUpToDates[notary];
                info->conditionalUpToDates.erase(notary);
                delete cond;
            }
            // update individual and overall up-to-date
            updateIndividualUpToDate(info, notary, id1);
            if (lastReportedIndividualUpToDate.count(notary) > 0)
            {
                lastReportedIndividualUpToDate[notary] = id1;
            }
            else
            {
                lastReportedIndividualUpToDate.insert(pair<unsigned long, CompleteID>(notary, id1));
            }
            if (correctUpToDateTime(info)) saveUpToDateTime(listType);
            return id1;
        }
    }

    // build condition
    UpToDateCondition* cond;
    if (id2.getNotary()<=0) cond = new UpToDateCondition(id1, id2);
    else
    {
        if (id1 <= info->individualUpToDates[notary])
        {
            if (!isInGeneralList(id1)) return CompleteID(0,0,0);
            id1=id2;
        }
        cond = new UpToDateCondition(id1, id1);
    }

    // insert condition
    if (info->conditionalUpToDates.count(notary)>0)
    {
        delete info->conditionalUpToDates[notary];
        info->conditionalUpToDates[notary] = cond;
    }
    else
    {
        info->conditionalUpToDates.insert(pair<unsigned long, UpToDateCondition*>(notary, cond));
    }

    insertEntryToDownload(id1, listType, notary);
    return CompleteID(0,0,0);
}

// db to be locked for this
void Database::addToMissingPredecessors(CompleteID &id, CompleteID &idMissing)
{
    if (missingPredecessors.count(id)<=0)
    {
        CIDsSet* newSet = new CIDsSet();
        missingPredecessors.insert(pair<CompleteID, CIDsSet*>(id, newSet));
    }
    missingPredecessors[id]->add(idMissing);
}

// db to be locked for this
void Database::addToMissingNotaries(CompleteID &id, unsigned long notaryNr)
{
    if (missingNotaries.count(id)<=0)
    {
        set<unsigned long>* newSet = new set<unsigned long>();
        missingNotaries.insert(pair<CompleteID, set<unsigned long>*>(id, newSet));
    }
    if (missingNotaries[id]->count(notaryNr) > 0) return;
    missingNotaries[id]->insert(notaryNr);
}

// db to be locked for this
void Database::insertEntryToDownload(CompleteID &id, unsigned char listType, unsigned long notary)
{
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesInD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesInDownload;
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesToD =
        (map<CompleteID, DownloadStatus*, CompleteID::CompareIDs>*)entriesToDownload;
    pair<unsigned char,unsigned long> listNotaryPair(listType,notary);
    if ((*entriesToD).count(id)>0)
    {
        if (notary>0 && (*entriesToD)[id]->neededFor.count(listNotaryPair)<=0)
            (*entriesToD)[id]->neededFor.insert(listNotaryPair);
    }
    else if ((*entriesInD).count(id)>0)
    {
        if (notary>0 && (*entriesInD)[id]->neededFor.count(listNotaryPair)<=0)
            (*entriesInD)[id]->neededFor.insert(listNotaryPair);
    }
    else
    {
        while ((*entriesToD).size() + (*entriesInD).size() > maxEntriesToDownload)
        {
            // delete entry with largest id
            // first determine map and largest id
            map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *mapToDeleteFrom = entriesToD;
            if (mapToDeleteFrom->size() <= 0) mapToDeleteFrom = entriesInD;
            else if (entriesInD->size() > 0)
            {
                CompleteID candidate1=mapToDeleteFrom->rbegin()->first;
                CompleteID candidate2=entriesInD->rbegin()->first;
                if (candidate2>candidate1) mapToDeleteFrom = entriesInD;
            }
            CompleteID idToDelete = mapToDeleteFrom->rbegin()->first;
            DownloadStatus* status = mapToDeleteFrom->rbegin()->second;
            // delete id and status etc.
            delete status;
            mapToDeleteFrom->erase(idToDelete);
            if (lastDownloadAttempt.count(idToDelete)>0)
            {
                lastDownloadAttempt.erase(idToDelete);
            }
            if (missingPredecessors.count(idToDelete)>0)
            {
                delete missingPredecessors[idToDelete];
                missingPredecessors.erase(idToDelete);
            }
            if (missingNotaries.count(idToDelete)>0)
            {
                delete missingNotaries[idToDelete];
                missingNotaries.erase(idToDelete);
            }
        }
        DownloadStatus* newDownloadStatus = new DownloadStatus();
        newDownloadStatus->attempts = 0;
        if (notary>0) newDownloadStatus->neededFor.insert(listNotaryPair);
        (*entriesToD).insert(pair<CompleteID,DownloadStatus*>(id,newDownloadStatus));
    }
}

void Database::lock()
{
    db_mutex.lock();
}

void Database::unlock()
{
    db_mutex.unlock();
}

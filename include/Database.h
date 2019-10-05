#ifndef DATABASE_H
#define DATABASE_H

#include <iostream>
#include <cstdio>
#include <string>
#include <map>
#include <set>
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "Entry.h"
#include "Type1Entry.h"
#include "Type2Entry.h"
#include "Type3Entry.h"
#include "Type4Entry.h"
#include "Type5Or15Entry.h"
#include "Type5Entry.h"
#include "Type6Entry.h"
#include "Type7Entry.h"
#include "Type8Entry.h"
#include "Type9Entry.h"
#include "Type10Entry.h"
#include "Type11Entry.h"
#include "Type12Entry.h"
#include "Type13Entry.h"
#include "Type14Entry.h"
#include "Type15Entry.h"
#include "CompleteID.h"
#include "CIDsSet.h"
#include "OtherServersHandler.h"
#include "Util.h"
#include "TNtrNr.h"
#include "ContactInfo.h"
#include "RefereeInfo.h"
#include "NotaryInfo.h"
#include <string>
#include <chrono>
#include "secblock.h"
#include "pssr.h"
#include "sha3.h"
#include <climits>
#include <cfloat>

using namespace std;

class Database
{
public:
    Database(const string& dbDir);
    bool init(unsigned long ownNr, TNtrNr corrNotary, CryptoPP::RSA::PublicKey* corrNotaryPublicKey);
    ~Database();
    bool loadNewerEntriesIds(unsigned char listType, CompleteID &benchmarkId, CIDsSet &newerIDs);
    CompleteID getUpToDateID(unsigned char listType);
    size_t getEntriesInDownload();
    CompleteID getNextEntryToDownload();
    CompleteID getOldestEntryToDownload();
    bool loadNextEntryToSign(string &type13entryStr, string &type12entryStr);
    bool isLastInBlock(CompleteID &signatureID);
    void updateRenotarizationAttempts();
    int loadNextEntryToRenotarize(CompleteID &entryId);
    void checkThreadTerminations();
    bool loadNextTerminatingThread(CompleteID &lastEntryId);
    bool addType13Entry(Type13Entry* entry, bool integrateIfPossible);
    CompleteID newEntriesIdsReport(unsigned char listType, unsigned long notary, CompleteID id1, CompleteID id2);
    void addContactsToServers(OtherServersHandler *servers, unsigned long ownNr);
    void lock();
    void unlock();
    set<unsigned long>* getActingNotaries(unsigned long long currentTime);
    CompleteID getLatestNotaryId(TNtrNr &totalNotaryNr);
    bool loadNotaryPubKey(TNtrNr &totalNotaryNr, string &str);
    bool isFreshNow(CompleteID &id);
    static unsigned long long systemTimeInMs();
    bool loadType13Entries(CompleteID &notEntryId, list<Type13Entry*> &targetList);
    CompleteID getFirstID(string* pubKey);
    CompleteID getCurrencyOrOblId(Type5Or15Entry* entry);
    size_t getRelatedEntries(CompleteID &id, list<CompleteID> &idsList);
    size_t getExistingEntriesIds(CompleteID &id, list<CompleteID> &idsList, bool renot);
    size_t getClaims(CompleteID &id, CompleteID &currencyId, CompleteID &maxClaimId, unsigned short &maxClaimsNum, list<CompleteID> &idsList);
    Type14Entry* buildNextClaim(CompleteID &pubKeyID, CompleteID &currencyId);
    size_t getExchangeOffers(CompleteID &pubKeyID, CompleteID &currencyOId, CompleteID &currencyRId, unsigned short &rangeNum, unsigned short &maxNum, list<CompleteID> &idsList);
    void updateExchangeOfferRatio(CompleteID &offerId, Type12Entry* offerEntry);
    static Type12Entry* createT12FromT13Str(string &str);
    size_t getTransferRequests(CompleteID &pubKeyID, CompleteID &currencyId, CompleteID &maxId, unsigned short &maxNum, list<CompleteID> &idsList);
    size_t getApplications(string type, CompleteID &pubKeyID, bool isApplicant, CompleteID &currencyId, unsigned char status, CompleteID &minApplId,
                           unsigned short maxNum, list<CompleteID> &idsList);
    size_t getNotaryApplications(CompleteID &pubKeyID, bool isApplicant, unsigned char status, CompleteID &minApplId, unsigned short maxNum, list<CompleteID> &idsList);
    RefereeInfo* getRefereeInfo(CompleteID &pubKeyID, CompleteID &currencyId);
    NotaryInfo* getNotaryInfo(string *publicKeyStr, CompleteID &currencyId);
    size_t getEssentialEntries(CompleteID &minEntryId, list<CompleteID> &idsList);
    void registerConflicts(CompleteID &badEntryId);
    bool isConflicting(CompleteID &firstID);
    unsigned long getSignatureCount(CompleteID &signId);
    bool verifySignature(string &signedSequence, string &signature, unsigned long notaryNum, unsigned long long timeStamp);
    bool amModerator(CompleteID &firstID);
    Type12Entry* buildUnderlyingEntry(CompleteID &firstId, unsigned char l);
    bool loadType13EntryStr(CompleteID &entryId, unsigned char l, string &str);
    bool addToEntriesToSign(CompleteID &signatureId, unsigned short participationRank);
    bool amCurrentlyActingWithBuffer();
    bool amCurrentlyActing();
    unsigned long long actingUntil(TNtrNr tNtrNr);
    bool isActingNotary(unsigned long notaryNr, unsigned long long currentTime);
    bool isActingNotary(TNtrNr tNtrNr, unsigned long long currentTime);
    bool isActingNotaryWithBuffer(TNtrNr tNtrNr, unsigned long long currentTime);
    bool dbUpToDate(unsigned long long wellConnectedSince);
    CompleteID getFirstID(CompleteID &id);
    CompleteID getLatestID(CompleteID &firstID);
    bool loadContactsList(set<unsigned long>* notaries, list<string> &contacts);
    bool tryToStoreContactInfo(ContactInfo &contactInfo);
    ContactInfo* getContactInfo(TNtrNr &totalNotaryNr);
    bool isInGeneralList(CompleteID &entryID);
    Type1Entry* getType1Entry();
    CompleteID deleteSignatures(CompleteID &firstSignId, bool renot);
    void deleteTailSignatures(CIDsSet &ids, bool renot);
    void verifySchedules(CompleteID &entryID);
    void saveConfirmationEntry(CompleteID &firstSignId, Type13Entry *confEntry);
    CompleteID getConnectedTransfer(CompleteID &transferId, CompleteID &currentRefId);
protected:
private:
    mutex db_mutex;
    Type1Entry* type1entry;
    rocksdb::DB* notaries;
    rocksdb::DB* entriesInNotarization;
    rocksdb::DB* notarizationEntries;
    rocksdb::DB* publicKeys;
    rocksdb::DB* subjectToRenotarization;
    rocksdb::DB* notaryApplications;
    rocksdb::DB* currenciesAndObligations;
    rocksdb::DB* scheduledActions;
    rocksdb::DB* essentialEntries;
    rocksdb::DB* conflicts;
    rocksdb::DB* perpetualEntries; // currencies, obligations and public keys
    rocksdb::DB* transfersWithFees; // type 10 entries which are subject to notarization fees
    Util util;
    unsigned long ownNumber;
    TNtrNr correspondingNotary;
    CryptoPP::RSA::PublicKey* correspondingNotaryPublicKey;

    struct UpToDateCondition
    {
        CompleteID upToDateIDConditional;
        set<CompleteID, CompleteID::CompareIDs> missingEntries; // what we need to download to satisfy condition

        UpToDateCondition(CompleteID &id1, CompleteID &id2);
        ~UpToDateCondition();
    };

    struct UpToDateTimeInfo
    {
        CompleteID upToDateIDOverall; // might have the role of a time stamp or id of actual entry
        map<unsigned long, CompleteID> individualUpToDates; // highest id reported by the respective notary (and downloaded if applicable)
        set<pair<CompleteID,unsigned long>, CompleteID::CompareIDIntPairs> individualUpToDatesByID;
        map<unsigned long, UpToDateCondition*> conditionalUpToDates; // condition for individual upToDateID improvement

        UpToDateTimeInfo(CompleteID u);
        ~UpToDateTimeInfo();
    };

    struct DownloadStatus
    {
        set<pair<unsigned char,unsigned long>> neededFor; // pair(listType, notary)
        unsigned char attempts;
    };

    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesToDownload;
    map<CompleteID, DownloadStatus*, CompleteID::CompareIDs> *entriesInDownload;
    map<CompleteID, CIDsSet*, CompleteID::CompareIDs> missingPredecessors;
    map<CompleteID, set<unsigned long>*, CompleteID::CompareIDs> missingNotaries;
    map<CompleteID, unsigned long long, CompleteID::CompareIDs> lastDownloadAttempt;

    map<unsigned long, CompleteID> lastReportedIndividualUpToDate;

    CIDsSet conflictingEntries;

    set<pair<unsigned long long,CompleteID>, CompleteID::LLComparePairs> entriesToSign;

    UpToDateTimeInfo* listEssentials;
    UpToDateTimeInfo* listGeneral;
    UpToDateTimeInfo* listTerminations;
    UpToDateTimeInfo* listPerpetuals;
    UpToDateTimeInfo* listTransfers;

    UpToDateTimeInfo* getInfoFromType(unsigned char listType);
    static bool correctUpToDateTime(UpToDateTimeInfo* info);
    void saveUpToDateTime(unsigned char listType);

    void updateIndividualUpToDate(UpToDateTimeInfo* info, unsigned long notary, CompleteID id);
    void insertEntryToDownload(CompleteID &id, unsigned char listType, unsigned long notary);
    void addToMissingPredecessors(CompleteID &id, CompleteID &idMissing);
    void addToMissingNotaries(CompleteID &id, unsigned long notaryNr);
    CompleteID loadUpToDateID(unsigned char listType);

    CompleteID getNotaryId(TNtrNr &totalNotaryNr);
    bool latestApprovalInNotarization(CompleteID &pubKeyID, CompleteID &terminationId, CompleteID &currentRefId);
    CompleteID getOutflowEntry(CompleteID &claimEntryId, CompleteID &currentRefId);
    unsigned long long getClaimCount(CompleteID &pubKeyID, CompleteID &currencyId);
    CompleteID getClaim(CompleteID &pubKeyID, CompleteID &currencyId, unsigned long long k, CompleteID &currentRefId);
    bool loadInitialT13EntryStr(CompleteID &idFirst, string &str);
    bool loadPubKey(CompleteID &idFirst, string &str);
    TNtrNr getTotalNotaryNr(CompleteID &pubKeyId);
    TNtrNr getTotalNotaryNr(string* pubKey);
    CompleteID getFirstID(Type12Entry* type12entry);
    unsigned long long getValidityDate(CompleteID &pubKeyId);
    CompleteID getValidityDateEntry(CompleteID &pubKeyId);
    bool recentlyApprovedNotary(CompleteID &pubKeyId, CompleteID &terminationID, unsigned long long currentTime);
    bool isActingReferee(CompleteID &pubKeyId, CompleteID &terminationID, unsigned long long currentTime);
    bool possibleToAppointNewNotaries(unsigned long long currentTime);
    unsigned long getRefereeTenure(CompleteID &currencyID);
    CompleteID getRefereeCurrency(CompleteID &pubKeyId, CompleteID &terminationID);
    unsigned long long getRefereeTenureStart(CompleteID &pubKeyId, CompleteID &terminationID);
    unsigned long long getRefereeTenureEnd(CompleteID &pubKeyId, CompleteID &terminationID);
    unsigned long getNrOfNotariesInLineage(unsigned short lineageNr);
    bool hasTakenPartInThread(CompleteID &applicationId, CompleteID &pubKeyId, CompleteID &currentRefId);
    CompleteID getApplicationId(CompleteID &threadEntryId);
    CompleteID getThreadSuccessor(CompleteID &threadEntryId, CompleteID &currentRefId);
    bool hasThreadSuccessor(CompleteID &threadEntryId, CompleteID &currentRefId);
    int verifyConflict(CompleteID &prelimEntryID, CompleteID &currentRefId);
    bool isFreeClaim(CompleteID &claimEntryId, CompleteID &pubKeyID, CompleteID &liquiId, CompleteID &currentRefId);
    unsigned long getProcessingTime(CompleteID &applicationId);
    double getRefereeStake(CompleteID &applicationId);
    CompleteID getCurrencyId(CompleteID &applicationId);
    CompleteID getTerminationId(CompleteID &applicationId);
    double getRequestedLiquidity(CompleteID &applicationId);
    unsigned long long getTenureStart(CompleteID &applicationId);
    double getForfeit(CompleteID &applicationId);
    double getProcessingFee(CompleteID &applicationId);
    unsigned char getThreadStatus(CompleteID &applicationId);
    CompleteID getThreadParticipantId(CompleteID &threadEntryId);
    CompleteID getLiquidityClaimId(CompleteID &threadEntryId);
    CompleteID getRefTerminationEntryId(CompleteID &threadEntryId);
    double getNonTransferPartInStake(CompleteID &threadEntryId);
    CompleteID getFirstNoteId(CompleteID &threadEntryId);
    double getLiquidityLimitPerRef(CompleteID &currencyID);
    double getInitialLiquidity(CompleteID &currencyID);
    double getParentShare(CompleteID &currencyID);
    double getLowerTransferLimit(CompleteID &oblId);
    double getLiquidityCreatedSoFar(CompleteID &pubKeyID, CompleteID &terminationId);
    unsigned short getAppointmentLimitPerRef(CompleteID &currencyID);
    unsigned short getRefsAppointedSoFar(CompleteID &pubKeyID, CompleteID &terminationId);
    double getDiscountRate(CompleteID &currencyOrOblId);
    CompleteID getTransferCollectingClaim(CompleteID &transferId, CompleteID &currentRefId);
    CompleteID getCollectingClaim(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
                                  unsigned char scenario, CompleteID &currentRefId);
    double getCollectableLiquidity(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
                                   double discountrate, unsigned char scenario, CompleteID &currentRefId, unsigned long long systemTime);
    double getCollectableNonTransferLiqui(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
                                          double discountrate, unsigned char scenario, CompleteID &currentRefId, unsigned long long systemTime);
    double getCollectableTransferableLiqui(CompleteID &entryID, CompleteID &pubKeyId, CompleteID &currencyOrOblId,
                                           double discountrate, unsigned char scenario, CompleteID &currentRefId, unsigned long long systemTime);
    CompleteID getOblOwner(CompleteID &oblId);
    unsigned long getSignatureCount(CompleteID &signId, unsigned char l);
    CompleteID getFirstNotSignId(CompleteID &signId, bool renot);
    unsigned long long getNotTimeLimit(CompleteID &firstSignId, bool renot);
    bool integrateNewNotEntry(CompleteID &firstSignId, bool renot, Type13Entry *confEntry);
    unsigned long long getEarliestRenotTime(CompleteID &entryId);
    bool addInitialSignature(Type13Entry* entry, Type12Entry* type12entry);
    bool addNextSignature(Type13Entry* entry, bool renot);
    bool storeRenotarizationParameters(CompleteID &entryId);
    void deleteRenotarizationParameters(CompleteID &entryId);
    bool loadUnderlyingType12EntryStr(CompleteID &entryId, unsigned char l, string &str);
    CompleteID getEntryInRenotarization(CompleteID &firstNotSignId);
    bool checkForConsistency(Type12Entry* entry, CompleteID &currentRefId);
    double addToTTLiquidityNotClaimedYet(CompleteID &keyID, CompleteID &entryID, unsigned char scenario, bool totalLiqui, double amount);
    bool storeT9eCreationParameters(CompleteID &threadId, unsigned long long &terminationTime);
    void deleteT9eCreationParameters(CompleteID &threadId);
    bool loadSigningNotaries(CompleteID &notEntryId, list<unsigned long> &notariesList);
    unsigned long long getTerminationTime(CompleteID &threadId);
    unsigned long getNotariesListLength(CompleteID &entryId, bool renot);
    unsigned long getNotaryInSchedule(CompleteID &entryId, unsigned long c, bool renot);
    bool removeFromExchangeOffersList(CompleteID &offerId, CompleteID &ownerId, CompleteID &currencyOId, CompleteID &currencyRId);
    void addToExchangeOffersList(CompleteID &offerId, CompleteID &ownerId, CompleteID &currencyOId, CompleteID &currencyRId, double amountR);
    size_t loadOutgoingShares(TNtrNr &totalNotaryNr, map<unsigned long, double> &sharesMap);
    size_t loadIncomingShares(TNtrNr &totalNotaryNr, map<unsigned long, double> &sharesMap);
    double getShareToKeep(TNtrNr &totalNotaryNr);
    void decreaseShareToKeep(TNtrNr &totalNotaryNr, double penaltyFactor);
    double getCollectedFees(TNtrNr &totalNotaryNr, CompleteID &currencyID);
    void addToCollectedFees(TNtrNr &totalNotaryNr, CompleteID &currencyID, double amount);
    unsigned long long getNotaryTenureStart(TNtrNr tNtrNr);
    double getRedistributionMultiplier(TNtrNr &from, TNtrNr &to);
    void addToRedistributionMultiplier(TNtrNr &from, TNtrNr &to, double penaltyFactor);
    double getMultipliersSum(TNtrNr &from);
    CompleteID getLastEssentialEntryId(CompleteID &currentRefId);
    unsigned long long deduceTimeLimit(Type12Entry* type12entry, CompleteID &firstId, CompleteID &predNotId);
    unsigned short getInitiatedThreadsCount(TNtrNr &tNtrNr);
    void setInitiatedThreadsCount(TNtrNr &tNtrNr, unsigned short newCount);
    void checkForInitiatedThreadConflict(TNtrNr &tNtrNr, CompleteID &currentRefId);
    bool eligibleForRenotarization(CompleteID &entryId);
    unsigned short getEntryType(CompleteID &firstID);
    Type5Entry* getTruncatedEntry(CompleteID &currencyIdFirst);
    set<unsigned long>* getFutureActingNotaries(unsigned long long futureTime);
    unsigned long getSignatureCount(CompleteID &signId, bool renot);
    unsigned short getScheduledLineage(CompleteID &entryId, bool renot);
    bool freshByDefi(CompleteID &id);
};

#endif // DATABASE_H

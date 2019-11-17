#include "OtherServersHandler.h"
#include "RequestBuilder.h"
#include "RequestProcessor.h"

#define reachOutRoutineSleepTimeInMcrS 200000
#define minAttempts 7
#define maxAttempts 60
#define attemptFreq 5
#define tolerableConnectionGapInMs 11000
#define maxConnectionDurationInMs 30000
#define maxRequestLength 524288
#define connectionTimeOutInMs 5000
#define connectionTimeOutCheckInMs 50

OtherServersHandler::OtherServersHandler(MessageBuilder *msgbuilder) : wellConnectedSince(0), msgBuilder(msgbuilder)
{
    answers=nullptr;
    reachOutRunning=false;
    reachOutStopped=true;
    allowNewContacts=true;
    if (msgBuilder!=nullptr) msgBuilder->setServersHandler(this);
    selectedNotaryPos=0;
    reachableNotariesVectorCorrect=true;
    // initialize random seed
    srand(time(NULL));
}

void OtherServersHandler::startConnector(RequestProcessor* r)
{
    contacts_mutex.lock();
    if (!allowNewContacts)
    {
        contacts_mutex.unlock();
        return;
    }
    answers=r;
    reachOutRunning=true;
    if(pthread_create(&connectorThread, NULL, reachOutRoutine, (void*) this) < 0)
    {
        puts("could not create server connector thread");
        exit(EXIT_FAILURE);
    }
    reachOutStopped=false;
    pthread_detach(connectorThread);
    contacts_mutex.unlock();
}

void OtherServersHandler::stopSafely()
{
    contacts_mutex.lock();
    allowNewContacts=false;
    reachOutRunning=false;
    contacts_mutex.unlock();

    bool locReachOutStopped=false;
    do
    {
        usleep(100000);
        contacts_mutex.lock();
        locReachOutStopped=reachOutStopped;
        contacts_mutex.unlock();
    }
    while (!locReachOutStopped);

    trashAllContacts();

    bool trashRemoved=false;
    do
    {
        usleep(100000);
        contacts_mutex.lock();
        trashRemoved=removeTrash();
        contacts_mutex.unlock();
    }
    while (!trashRemoved);

    puts("OtherServersHandler::stopSafely: clean up complete");
}

OtherServersHandler::~OtherServersHandler()
{

}

void OtherServersHandler::contactsReport()
{
    contacts_mutex.lock();
    string msg;
    map<unsigned long, ContactHandler*>::iterator it;
    msg.append("Contacts reachable: ");
    bool first=true;
    for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
    {
        if (!first) msg.append(", ");
        else first = false;
        msg.append(to_string(it->first));
    }
    if (reachableNotariesVectorCorrect) msg.append("\nreachableNotariesVectorCorrect: true");
    else msg.append("\nreachableNotariesVectorCorrect: false");
    msg.append("\nreachable Notaries: ");
    for (unsigned short i=0; i<reachableNotariesVector.size(); i++)
    {
        if (i!=0) msg.append(", ");
        msg.append(to_string(reachableNotariesVector[i]));
    }
    msg.append("\nselectedNotaryPos: ");
    msg.append(to_string(selectedNotaryPos));
    msg.append("\nContacts to reach: ");
    first=true;
    for (it=contactsToReach.begin(); it!=contactsToReach.end(); ++it)
    {
        if (!first) msg.append(", ");
        else first = false;
        msg.append(to_string(it->first));
    }
    msg.append("\nContacts unreachable: ");
    first=true;
    for (it=contactsUnreachable.begin(); it!=contactsUnreachable.end(); ++it)
    {
        if (!first) msg.append(", ");
        else first = false;
        msg.append(to_string(it->first));
    }
    msg.append("\nNr. of trashed contacts: ");
    msg.append(to_string(trashedContacts.size()));
    puts(msg.c_str());
    contacts_mutex.unlock();
}

void OtherServersHandler::trashAllContacts()
{
    contacts_mutex.lock();
    map<unsigned long, ContactHandler*>::iterator it;

    for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
    {
        trashContact(it->second);
    }
    contactsReachable.clear();
    reachableNotariesVectorCorrect=false;

    for (it=contactsUnreachable.begin(); it!=contactsUnreachable.end(); ++it)
    {
        trashContact(it->second);
    }
    contactsUnreachable.clear();

    for (it=contactsToReach.begin(); it!=contactsToReach.end(); ++it)
    {
        trashContact(it->second);
    }
    contactsToReach.clear();

    contacts_mutex.unlock();
}

void OtherServersHandler::addContact(unsigned long notary, string ip, int port, unsigned long long validSince, unsigned long long activeUntil)
{
    unsigned long long currentTime = systemTimeInMs();
    if (validSince > currentTime || activeUntil < currentTime)
    {
        return;
    }
    contacts_mutex.lock();
    if (!allowNewContacts)
    {
        contacts_mutex.unlock();
        return;
    }
    // check if contact info already available
    if (contactsReachable.count(notary)==1)
    {
        ContactHandler* ci=contactsReachable[notary];
        if (validSince <= ci->validSince)
        {
            contacts_mutex.unlock();
            return;
        }
        else
        {
            activeUntil = min(activeUntil, (unsigned long long) ci->actingUntil);
            if (ci->ip.compare(ip) == 0 && ci->port == port)
            {
                ci->actingUntil = activeUntil;
                ci->validSince = validSince;
                contacts_mutex.unlock();
                return;
            }
            contactsReachable.erase(notary);
            trashContact(ci);
            reachableNotariesVectorCorrect=false;
        }
    }
    else if (contactsUnreachable.count(notary)==1)
    {
        ContactHandler* ci=contactsUnreachable[notary];
        if (validSince <= ci->validSince)
        {
            contacts_mutex.unlock();
            return;
        }
        else
        {
            activeUntil = min(activeUntil, (unsigned long long) ci->actingUntil);
            contactsUnreachable.erase(notary);
            trashContact(ci);
        }
    }
    else if (contactsToReach.count(notary)==1)
    {
        ContactHandler* ci=contactsToReach[notary];
        if (validSince <= ci->validSince)
        {
            contacts_mutex.unlock();
            return;
        }
        else
        {
            activeUntil = min(activeUntil, (unsigned long long) ci->actingUntil);
            if (ci->ip.compare(ip) == 0 && ci->port == port)
            {
                ci->actingUntil = activeUntil;
                ci->validSince = validSince;
                contacts_mutex.unlock();
                return;
            }
            contactsToReach.erase(notary);
            trashContact(ci);
        }
    }

    // add contact to the list of connections to establish
    ContactHandler* contact = new ContactHandler(notary, ip, port, validSince, activeUntil, this);
    if (contact->ip.length()>3)
    {
        if (contactsToReach.count(notary) > 0)
        {
            puts("OtherServersHandler::addContact: pre-existing contact in contactsToReach");
            exit(EXIT_FAILURE);
        }
        else contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
    }
    else
    {
        if (contactsUnreachable.count(notary) > 0)
        {
            puts("OtherServersHandler::addContact: pre-existing contact in contactsUnreachable");
            exit(EXIT_FAILURE);
        }
        else contactsUnreachable.insert(pair<unsigned long, ContactHandler*>(notary, contact));
    }
    contacts_mutex.unlock();
}

void OtherServersHandler::connectToNotaryRoutine(ContactHandler *contactHandler)
{
    ContactHandler* cHandler = (ContactHandler*) contactHandler;
    OtherServersHandler* serversHandler = (OtherServersHandler*) cHandler->serversHandler;
    const unsigned long notary = cHandler->notaryNr;

    serversHandler->contacts_mutex.lock();

    if (!serversHandler->reachOutRunning)
    {
        cHandler->connectToThreadStopped = true;
        serversHandler->contacts_mutex.unlock();
        return;
    }

    // exit if existing connection / connection is being set up
    if (!cHandler->listenerThreadStopped || !cHandler->attemptInterrupterStopped)
    {
        cHandler->connectToThreadStopped = true;
        serversHandler->contacts_mutex.unlock();
        return;
    }

    // delete notary if not acting
    const unsigned long long currentTime = systemTimeInMs();
    if (cHandler->actingUntil <= currentTime)
    {
        serversHandler->contactsReachable.erase(notary);
        serversHandler->reachableNotariesVectorCorrect=false;
        serversHandler->contactsToReach.erase(notary);
        serversHandler->trashContact(cHandler);
        cHandler->connectToThreadStopped = true;
        serversHandler->contacts_mutex.unlock();
        return;
    }

    const string ip = cHandler->ip;
    const int port = cHandler->port;

    cHandler->attemptInterrupterStopped = false;
    cHandler->listenerThreadStopped = false;

    serversHandler->contacts_mutex.unlock(); // unlock for the connection build up

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        puts("OtherServersHandler::connectToNotaryRoutine: failed to open socket");
        serversHandler->contacts_mutex.lock();
        cHandler->attemptInterrupterStopped = true;
        goto registerFail;
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    // start attemptInterruptionThread
    cHandler->socketNrInAttempt = sock;
    if (cHandler->attemptInterruptionThread != nullptr) delete cHandler->attemptInterruptionThread;
    cHandler->attemptInterruptionThread = new thread(attemptInterrupter, cHandler);
    ((thread*)cHandler->attemptInterruptionThread)->detach();

    // connecting ...
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        cHandler->socketNrInAttempt = -1;
        closeconnection(sock);
        serversHandler->contacts_mutex.lock();
        goto registerFail;
    }
    else // connection established
    {
        cHandler->socketNrInAttempt = -1;
        serversHandler->contacts_mutex.lock();

        // start listener thread
        cHandler->socket=sock;
        if (cHandler->answerBuilder != nullptr)
        {
            puts("OtherServersHandler::connectToNotaryRoutine: unexpected cHandler->answerBuilder != nullptr");
            exit(EXIT_FAILURE);
        }
        cHandler->answerBuilder = new RequestBuilder(maxRequestLength, serversHandler->answers, sock);
        if (cHandler->listenerThread != nullptr) delete cHandler->listenerThread;
        cHandler->listenerThread = new thread(socketReader, cHandler);
        ((thread*)cHandler->listenerThread)->detach();

        // mark notary as reachable
        if (serversHandler->contactsToReach.count(notary)==1)
        {
            serversHandler->contactsToReach.erase(notary);
            if (serversHandler->contactsReachable.count(notary) > 0)
            {
                puts("OtherServersHandler::connectToNotaryRoutine: pre-existing contact in contactsReachable");
                exit(EXIT_FAILURE);
            }
            else serversHandler->contactsReachable.insert(pair<unsigned long, ContactHandler*>(notary, cHandler));
            serversHandler->reachableNotariesVectorCorrect=false;
        }

        // send outstanding messages + flush buffer:
        cHandler->message_buffer_mutex.lock();
        // append connection close msg
        string msg;
        byte type = 20;
        msg.push_back((char)type);
        serversHandler->msgBuilder->packMessage(&msg);
        ((string*)&cHandler->messagesStr)->append(msg);
        // send and exit
        send(cHandler->socket,
             ((string*)&cHandler->messagesStr)->c_str(), ((string*)&cHandler->messagesStr)->length(), MSG_NOSIGNAL);
        ((string*)&cHandler->messagesStr)->clear();
        cHandler->message_buffer_mutex.unlock();

        cHandler->connectToThreadStopped = true;
        serversHandler->contacts_mutex.unlock();
        return;
    }
registerFail: // contacts_mutex must be locked for this section

    cHandler->failedAttempts++;
    if (serversHandler->contactsToReach.count(notary)==1 && cHandler->failedAttempts > maxAttempts)
    {
        serversHandler->contactsToReach.erase(notary);
        if (serversHandler->contactsUnreachable.count(notary) > 0)
        {
            puts("OtherServersHandler::connectToNotaryRoutine: pre-existing contact in contactsUnreachable");
            exit(EXIT_FAILURE);
        }
        else serversHandler->contactsUnreachable.insert(pair<unsigned long, ContactHandler*>(notary, cHandler));
    }
    else if (serversHandler->contactsReachable.count(notary)==1 && cHandler->failedAttempts > minAttempts)
    {
        ContactHandler* contact = new ContactHandler(notary, cHandler->ip, cHandler->port, cHandler->validSince,
                cHandler->actingUntil, serversHandler);
        serversHandler->trashContact(cHandler);
        serversHandler->contactsReachable.erase(notary);
        serversHandler->reachableNotariesVectorCorrect=false;
        if (serversHandler->contactsToReach.count(notary) > 0)
        {
            puts("OtherServersHandler::connectToNotaryRoutine: pre-existing contact in contactsToReach");
            exit(EXIT_FAILURE);
        }
        else serversHandler->contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
    }

    cHandler->listenerThreadStopped = true;
    cHandler->connectToThreadStopped = true;
    serversHandler->contacts_mutex.unlock();
    return;
}

void OtherServersHandler::loadContactsReachable(list<unsigned long> &notariesList)
{
    contacts_mutex.lock();
    map<unsigned long, ContactHandler*>::iterator it;
    for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
    {
        notariesList.push_back(it->first);
    }
    contacts_mutex.unlock();
}

unsigned long long OtherServersHandler::getWellConnectedSince()
{
    contacts_mutex.lock();
    unsigned long long out=0;
    if (!reachOutRunning)
    {
        contacts_mutex.unlock();
        return out;
    }
    const bool success = (contactsReachable.size() >= contactsToReach.size()+contactsUnreachable.size());
    if (success)
    {
        if (wellConnectedSince==0) wellConnectedSince=systemTimeInMs();
    }
    else
    {
        wellConnectedSince=0;
    }
    out = wellConnectedSince;
    contacts_mutex.unlock();
    return out;
}

bool OtherServersHandler::wellConnected(size_t notariesListLength)
{
    contacts_mutex.lock();
    if (!reachOutRunning)
    {
        contacts_mutex.unlock();
        return false;
    }
    const bool success = (contactsReachable.size() == notariesListLength) &&
                         (contactsReachable.size() >= contactsToReach.size()+contactsUnreachable.size());
    contacts_mutex.unlock();
    return success;
}

bool OtherServersHandler::wellConnected()
{
    contacts_mutex.lock();
    if (!reachOutRunning)
    {
        contacts_mutex.unlock();
        return false;
    }
    const bool success = (contactsReachable.size() >= contactsToReach.size()+contactsUnreachable.size());
    if (success)
    {
        if (wellConnectedSince==0) wellConnectedSince=systemTimeInMs();
    }
    else
    {
        wellConnectedSince=0;
    }
    contacts_mutex.unlock();
    return success;
}

unsigned long OtherServersHandler::getSomeReachableNotary()
{
    contacts_mutex.lock();
    if (!reachOutRunning)
    {
        contacts_mutex.unlock();
        return 0;
    }
    if (contactsReachable.size()==0)
    {
        contacts_mutex.unlock();
        return 0;
    }
    auto it = contactsReachable.begin();
    advance(it, rand() % contactsReachable.size());
    const unsigned long notaryNr = it->first;
    contacts_mutex.unlock();
    return notaryNr;
}

bool OtherServersHandler::sendMessage(unsigned long notaryNr, string &msg)
{
    contacts_mutex.lock();
    if (!reachOutRunning)
    {
        contacts_mutex.unlock();
        return false;
    }
    if (contactsReachable.count(notaryNr)<=0)
    {
        contacts_mutex.unlock();
        return false;
    }
    ContactHandler *ch = contactsReachable[notaryNr];
    ch->addMessage(msg);
    if (ch->connectToThreadStopped)
    {
        ch->connectToThreadStopped = false;
        if (ch->connectToThread != nullptr) delete ch->connectToThread;
        ch->connectToThread = new thread(connectToNotaryRoutine, ch);
        ((thread*)ch->connectToThread)->detach();
    }
    contacts_mutex.unlock();
    return true;
}

void OtherServersHandler::requestNotarization(Type12Entry* t12e, unsigned long notaryNr)
{
    if (t12e == nullptr || notaryNr==0) return;
    string *type12entryStr = t12e->getByteSeq();
    if (type12entryStr == nullptr) return;
    string message;
    byte type = 0;
    message.push_back(type);
    message.append(*type12entryStr);
    msgBuilder->packMessage(&message);
    if (sendMessage(notaryNr, message))
    {
        //puts("request for notarization sent successfully");
    }
    else
    {
        puts("requestNotarization unsuccessful");
    }
}

void OtherServersHandler::askForInitialType13Entry(CompleteID &id, unsigned long notaryNr)
{
    string message;
    byte type = 13;
    message.push_back(type);
    message.append(id.to20Char());
    msgBuilder->packMessage(&message);
    if (sendMessage(notaryNr, message))
    {
        //puts("request for initial Type13Entry sent successfully");
    }
    else
    {
        puts("askForInitialType13Entry unsuccessful");
    }
}

void OtherServersHandler::sendSignatureToAll(string *t13eStr)
{
    // build message
    string msg;
    byte type = 12;
    Util u;
    msg.push_back((char)type);
    msg.append(u.UllAsByteSeq(t13eStr->length()));
    msg.append(*t13eStr);
    msgBuilder->packMessage(&msg);

    // get reachableNotaries
    set<unsigned long> reachableNotaries;
    contacts_mutex.lock();
    map<unsigned long, ContactHandler*>::iterator it;
    for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
    {
        reachableNotaries.insert(it->first);
    }
    contacts_mutex.unlock();

    // send to reachable notaries
    for (set<unsigned long>::iterator iter=reachableNotaries.begin(); iter!=reachableNotaries.end(); ++iter)
    {
        sendMessage(*iter, msg);
    }
}

void OtherServersHandler::sendConsiderNotarizationEntryToAll(CompleteID &firstID)
{
    if (!msgBuilder->getTNotaryNr().isGood()) return;

    // build message
    string msg;
    byte type = 16;
    msg.push_back((char)type);
    Util u;
    string sequenceToSign;
    sequenceToSign.append(u.UcAsByteSeq(1));
    sequenceToSign.append(u.UlAsByteSeq(msgBuilder->getTNotaryNr().getNotaryNr()));
    sequenceToSign.append(firstID.to20Char());
    sequenceToSign.append(firstID.to20Char());
    string *signature = msgBuilder->signString(sequenceToSign);
    msg.append(sequenceToSign);
    msg.append(*signature);
    delete signature;
    msgBuilder->packMessage(&msg);

    // get reachableNotaries
    set<unsigned long> reachableNotaries;
    contacts_mutex.lock();
    map<unsigned long, ContactHandler*>::iterator it;
    for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
    {
        reachableNotaries.insert(it->first);
    }
    contacts_mutex.unlock();

    // send to reachable notaries
    for (set<unsigned long>::iterator iter=reachableNotaries.begin(); iter!=reachableNotaries.end(); ++iter)
    {
        sendMessage(*iter, msg);
    }
}

void OtherServersHandler::sendNotarizationEntryToAll(list<Type13Entry*> &t13eList)
{
    if (!msgBuilder->getTNotaryNr().isGood()) return;

    // build message
    string msg;
    byte type = 17;
    msg.push_back((char)type);
    string sequenceToSign;
    if (!msgBuilder->addToString(t13eList, sequenceToSign)) return;
    string *signature = msgBuilder->signString(sequenceToSign);
    msg.append(sequenceToSign);
    Util u;
    msg.append(u.UllAsByteSeq(signature->length()+12));
    msg.append(*signature);
    delete signature;
    msg.append(u.UlAsByteSeq(msgBuilder->getTNotaryNr().getNotaryNr()));
    msg.append(u.UllAsByteSeq(systemTimeInMs()));
    msgBuilder->packMessage(&msg);

    // get reachableNotaries
    set<unsigned long> reachableNotaries;
    contacts_mutex.lock();
    map<unsigned long, ContactHandler*>::iterator it;
    for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
    {
        reachableNotaries.insert(it->first);
    }
    contacts_mutex.unlock();

    // send to reachable notaries
    for (set<unsigned long>::iterator iter=reachableNotaries.begin(); iter!=reachableNotaries.end(); ++iter)
    {
        sendMessage(*iter, msg);
    }
}

void OtherServersHandler::sendContactsRqst()
{
    unsigned long notaryNr = getSomeReachableNotary();
    if (notaryNr==0) return;
    string msg;
    byte type = 19;
    msg.push_back(type);
    msgBuilder->packMessage(&msg);
    sendMessage(notaryNr, msg);
}

void OtherServersHandler::sendContactsList(list<string> &contacts, set<unsigned long> &notaries)
{
    if (contacts.size()==0 || notaries.size()==0) return;
    set<unsigned long> remainingNotaries(notaries);
    while (remainingNotaries.size()>0)
    {
        const unsigned long notaryNr = *remainingNotaries.begin();
        remainingNotaries.erase(notaryNr);

        // send contacts via socket
        list<string>::iterator it;
        for (it=contacts.begin(); it!=contacts.end(); ++it)
        {
            string msg;
            byte type = 18;
            msg.push_back((char)type);
            msg.append(*it);
            msgBuilder->packMessage(&msg);

            sendMessage(notaryNr, msg);
        }
    }
}

// used by the moderating notary only
void OtherServersHandler::sendNewSignature(Type13Entry *entry, set<unsigned long> &notaries)
{
    if (entry==nullptr) return;
    if (!wellConnected())
    {
        return;
    }
    if (notaries.size()==0)
    {
        return;
    }
    set<unsigned long> remainingNotaries(notaries);
    unsigned short participationRank = 0;
    while (remainingNotaries.size()>0)
    {
        auto it = remainingNotaries.begin();
        advance(it, rand() % remainingNotaries.size());
        const unsigned long notaryNr = *it;
        remainingNotaries.erase(notaryNr);

        contacts_mutex.lock();
        if (contactsReachable.count(notaryNr)<=0)
        {
            contacts_mutex.unlock();
            continue;
        }
        contacts_mutex.unlock();

        sendNewSignature(entry->getByteSeq(), notaryNr, participationRank);
        participationRank++;
    }
}

// used by the moderating notary only
void OtherServersHandler::sendNewSignature(string *t13eStr, unsigned long notaryNr, unsigned short participationRank)
{
    if (!wellConnected()) return;
    if (!msgBuilder->getTNotaryNr().isGood()) return;

    string msg;
    byte type = 12;
    msg.push_back((char)type);
    Util u;
    msg.append(u.UllAsByteSeq(t13eStr->length()));
    msg.append(*t13eStr);
    msg.push_back(0x04);
    string rankAsString = u.UsAsByteSeq(participationRank);
    msg.append(rankAsString);
    // build sequence to sign
    string stringToSign(*t13eStr);
    stringToSign.append(rankAsString);
    // create confirmation signature
    string* signature = msgBuilder->signString(stringToSign);
    unsigned long signatureLength = signature->length();
    msg.append(u.UlAsByteSeq(signatureLength));
    msg.append(*signature);
    delete signature;
    // pack and send
    msgBuilder->packMessage(&msg);
    if (sendMessage(notaryNr, msg))
    {
        //puts("NewSignature sent successfully");
    }
    else
    {
        puts("sendNewSignature unsuccessful");
    }
}

void OtherServersHandler::checkNewerEntry(unsigned char listType, unsigned long notaryNr, CompleteID lastEntryID)
{
    // build message
    string message;
    byte type = 15;
    message.push_back(type);
    Util u;
    message.append(u.UcAsByteSeq(listType));
    message.append(lastEntryID.to20Char());
    msgBuilder->packMessage(&message);

    // send message to notary
    sendMessage(notaryNr, message);
}

void OtherServersHandler::checkNewerEntry(unsigned char listType, CompleteID lastEntryID, unsigned short maxNotaries, bool changePos)
{
    contacts_mutex.lock();

    // rebuild reachableNotariesVector if necessary
    if (!reachableNotariesVectorCorrect)
    {
        reachableNotariesVector.clear();
        map<unsigned long, ContactHandler*>::iterator it;
        for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
        {
            reachableNotariesVector.push_back(it->first);
        }
        reachableNotariesVectorCorrect=true;
    }
    unsigned short vectorSize = (unsigned short) reachableNotariesVector.size();
    maxNotaries = min(maxNotaries, vectorSize);

    // select notaries
    set<unsigned long> selectedNotaries;
    unsigned short pos = selectedNotaryPos;
    for (int i=0; i<maxNotaries; i++)
    {
        pos = pos % vectorSize;
        selectedNotaries.insert(reachableNotariesVector[pos]);
        pos++;
    }
    if (changePos) selectedNotaryPos = pos;

    contacts_mutex.unlock();

    // build message
    string message;
    byte type = 15;
    message.push_back(type);
    Util u;
    message.append(u.UcAsByteSeq(listType));
    message.append(lastEntryID.to20Char());
    msgBuilder->packMessage(&message);

    // send message to selected notaries
    for (set<unsigned long>::iterator iter=selectedNotaries.begin(); iter!=selectedNotaries.end(); ++iter)
    {
        sendMessage(*iter, message);
    }
}

void OtherServersHandler::requestEntry(CompleteID entryID, unsigned long notaryNr)
{
    if (notaryNr<1) return;
    string message;
    byte type = 14;
    message.push_back(type);
    message.append(entryID.to20Char());
    msgBuilder->packMessage(&message);
    if (sendMessage(notaryNr, message))
    {
        //puts("request for entry sent successfully");
    }
    else
    {
        puts("requestEntry unsuccessful");
    }
}

void OtherServersHandler::trashContact(ContactHandler* ch)
{
    closeconnection(ch->socketNrInAttempt);
    ch->socketNrInAttempt=-1;
    closeconnection(ch->socket);
    ch->socket=-1;
    if (!ch->listenerThreadStopped || !ch->attemptInterrupterStopped || !ch->connectToThreadStopped)
    {
        trashedContacts.insert(trashedContacts.end(), ch);
    }
    else
    {
        delete ch;
    }
}

bool OtherServersHandler::removeTrash()
{
    list<ContactHandler*> removableContacts;
    list<ContactHandler*>::iterator it;
    ContactHandler* ch;
    for (it=trashedContacts.begin(); it!=trashedContacts.end(); ++it)
    {
        ch=*it;
        if (ch->listenerThreadStopped && ch->attemptInterrupterStopped && ch->connectToThreadStopped)
        {
            removableContacts.insert(removableContacts.end(), ch);
        }
    }
    for (it=removableContacts.begin(); it!=removableContacts.end(); ++it)
    {
        ch=*it;
        trashedContacts.remove(ch);
        delete ch;
    }
    removableContacts.clear();
    const bool success = (trashedContacts.size() == 0);
    return success;
}

void* OtherServersHandler::reachOutRoutine(void* servers)
{
    OtherServersHandler* serversHandler=(OtherServersHandler*) servers;
    serversHandler->reachOutStopped=false;
    unsigned short attemptCount=attemptFreq;

    do
    {
        usleep(reachOutRoutineSleepTimeInMcrS);

        serversHandler->contacts_mutex.lock();

        // reconnect + remove dead connections
        set<unsigned long> reachableList;
        map<unsigned long, ContactHandler*> *m = &serversHandler->contactsReachable;
        map<unsigned long, ContactHandler*>::iterator it;
        for (it=m->begin(); it!=m->end(); ++it)
        {
            reachableList.insert(it->first);
        }
        const unsigned long long currentTime = serversHandler->systemTimeInMs();
        set<unsigned long>::iterator iter;
        for (iter=reachableList.begin(); iter!=reachableList.end(); ++iter)
        {
            unsigned long notary = *iter;
            ContactHandler* ch = serversHandler->contactsReachable[notary];

            if (ch->actingUntil <= currentTime)
            {
                serversHandler->contactsReachable.erase(notary);
                serversHandler->reachableNotariesVectorCorrect=false;
                serversHandler->trashContact(ch);
            }
            else if (ch->listenerThreadStopped)
            {
                if (ch->lastListeningTime + tolerableConnectionGapInMs < currentTime || ch->failedAttempts > minAttempts)
                {
                    puts("OtherServersHandler::reachOutRoutine: no connection for too long");
                    ContactHandler* contact = new ContactHandler(notary, ch->ip, ch->port, ch->validSince, ch->actingUntil, serversHandler);
                    serversHandler->trashContact(ch);
                    serversHandler->contactsReachable.erase(notary);
                    serversHandler->reachableNotariesVectorCorrect=false;
                    if (serversHandler->contactsToReach.count(notary) > 0)
                    {
                        puts("OtherServersHandler::reachOutRoutine: pre-existing contact in contactsToReach");
                        exit(EXIT_FAILURE);
                    }
                    else serversHandler->contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
                }
                else if (ch->lastListeningTime + tolerableConnectionGapInMs/2 < currentTime || ch->msgStrLength()>0)
                {
                    if (ch->connectToThreadStopped)
                    {
                        ch->connectToThreadStopped = false;
                        if (ch->connectToThread != nullptr) delete ch->connectToThread;
                        ch->connectToThread = new thread(connectToNotaryRoutine, ch);
                        ((thread*)ch->connectToThread)->detach();
                    }
                }
            }
            else if (ch->lastConnectionTime + maxConnectionDurationInMs < currentTime)
            {
                string out = "OtherServersHandler::reachOutRoutine: connection time out, notary ";
                out.append(to_string(notary));
                puts(out.c_str());
                ContactHandler* contact = new ContactHandler(notary, ch->ip, ch->port, ch->validSince, ch->actingUntil, serversHandler);
                serversHandler->trashContact(ch);
                serversHandler->contactsReachable.erase(notary);
                serversHandler->reachableNotariesVectorCorrect=false;
                if (serversHandler->contactsToReach.count(notary) > 0)
                {
                    puts("OtherServersHandler::reachOutRoutine: pre-existing contact in contactsToReach");
                    exit(EXIT_FAILURE);
                }
                else serversHandler->contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
            }
        }

        // remove trash
        serversHandler->removeTrash();

        // try to connect to servers
        if (attemptCount >= attemptFreq)
        {
            set<unsigned long> toReachList;
            m = &serversHandler->contactsToReach;
            for (it=m->begin(); it!=m->end() && serversHandler->reachOutRunning; ++it)
            {
                toReachList.insert(it->first);
            }
            for (iter=toReachList.begin(); iter!=toReachList.end() && serversHandler->reachOutRunning; ++iter)
            {
                unsigned long notary = *iter;
                ContactHandler* ch = serversHandler->contactsToReach[notary];

                if (ch->actingUntil <= currentTime)
                {
                    serversHandler->contactsToReach.erase(notary);
                    serversHandler->trashContact(ch);
                }
                else
                {
                    if (ch->connectToThreadStopped)
                    {
                        ch->connectToThreadStopped = false;
                        if (ch->connectToThread != nullptr) delete ch->connectToThread;
                        ch->connectToThread = new thread(connectToNotaryRoutine, ch);
                        ((thread*)ch->connectToThread)->detach();
                    }
                }
            }

            attemptCount=0;

            // trash unreachable that are not acting
            set<unsigned long> unreachableList;
            m = &serversHandler->contactsUnreachable;
            for (it=m->begin(); it!=m->end() && serversHandler->reachOutRunning; ++it)
            {
                unreachableList.insert(it->first);
            }
            for (iter=unreachableList.begin(); iter!=unreachableList.end() && serversHandler->reachOutRunning; ++iter)
            {
                unsigned long notary = *iter;
                ContactHandler* ch = serversHandler->contactsUnreachable[notary];

                if (ch->actingUntil <= currentTime)
                {
                    serversHandler->contactsUnreachable.erase(notary);
                    serversHandler->trashContact(ch);
                }
            }
        }
        else attemptCount++;

        serversHandler->contacts_mutex.unlock();
    }
    while(serversHandler->reachOutRunning);

    serversHandler->reachOutStopped=true;
    puts("OtherServersHandler::reachOutRoutine stopped");
    return NULL;
}

void OtherServersHandler::attemptInterrupter(ContactHandler* contactHandler)
{
    ContactHandler* ci = (ContactHandler*) contactHandler;
    ci->attemptInterrupterStopped = false;
    int sock = ci->socketNrInAttempt;
    if (sock == -1) goto close;
    for(int counter = 0; counter*connectionTimeOutCheckInMs < connectionTimeOutInMs; counter++)
    {
        usleep(connectionTimeOutCheckInMs * 1000);
        if (sock != ci->socketNrInAttempt)
        {
            goto close;
        }
    }
    closeconnection(sock); // timeout
close:
    ci->socketNrInAttempt=-1;
    ci->attemptInterrupterStopped = true;
    return;
}

void OtherServersHandler::socketReader(ContactHandler* contactHandler)
{
    ContactHandler* ci = (ContactHandler*) contactHandler;
    ci->listenerThreadStopped=false;
    ci->failedAttempts=0;
    ci->lastConnectionTime = systemTimeInMs();
    ci->lastListeningTime = ci->lastConnectionTime;
    byte* buffer = new byte[1024];
    while(ci->socket!=-1)
    {
        int n = recv(ci->socket, buffer, 1024, 0);
        if(n<=0 || n>1024) goto close;
        else
        {
            for (int i=0; i<n; i++)
            {
                if (ci->socket==-1 || ci->answerBuilder==nullptr
                        || !((RequestBuilder*)ci->answerBuilder)->addByte(buffer[i])) goto close;
            }
        }
    }
close:
    delete[] buffer;
    ci->lastListeningTime=systemTimeInMs();
    closeconnection(ci->socket);
    ci->socket=-1;
    if (ci->answerBuilder==nullptr)
    {
        puts("OtherServersHandler::socketReader: unexpected ci->answerBuilder==nullptr");
        exit(EXIT_FAILURE);
    }
    delete ci->answerBuilder;
    ci->answerBuilder=nullptr;
    ci->listenerThreadStopped=true;
    return;
}

OtherServersHandler::ContactHandler::ContactHandler(unsigned long n, string i, int p, unsigned long long v, unsigned long long au,
        OtherServersHandler* sh) : notaryNr(n), ip(i), port(p), validSince(v), actingUntil(au), serversHandler(sh)
{
    answerBuilder=nullptr;
    socket=-1;
    listenerThreadStopped=true;
    attemptInterrupterStopped=true;
    connectToThreadStopped=true;
    connectToThread=nullptr;
    listenerThread=nullptr;
    attemptInterruptionThread=nullptr;
    socketNrInAttempt=-1;
    failedAttempts=0;
    lastConnectionTime=0;
    lastListeningTime=0;
    ((string*)&messagesStr)->clear();
}

void OtherServersHandler::ContactHandler::addMessage(string &msg)
{
    message_buffer_mutex.lock();
    ((string*)&messagesStr)->append(msg);
    message_buffer_mutex.unlock();
}

size_t OtherServersHandler::ContactHandler::msgStrLength()
{
    message_buffer_mutex.lock();
    size_t out = ((string*)&messagesStr)->length();
    message_buffer_mutex.unlock();
    return out;
}

OtherServersHandler::ContactHandler::~ContactHandler()
{
    if (answerBuilder!=nullptr)
    {
        puts("OtherServersHandler::ContactHandler: unexpected answerBuilder!=nullptr");
        delete answerBuilder;
        answerBuilder=nullptr;
    }
}

set<unsigned long>* OtherServersHandler::genNotariesList(set<unsigned long> &actingNotaries)
{
    set<unsigned long>* out = new set<unsigned long>();
    contacts_mutex.lock();
    if (!reachOutRunning)
    {
        contacts_mutex.unlock();
        return out;
    }

    for (set<unsigned long>::iterator it=actingNotaries.begin(); it!=actingNotaries.end(); ++it)
    {
        const unsigned long notaryNr = *it;
        if (contactsReachable.count(notaryNr)>0)
        {
            out->insert(notaryNr);
        }
        else
        {
            //puts("OtherServersHandler::genNotariesList: acting notary not reachable");
        }
    }
    contacts_mutex.unlock();

    return out;
}

void OtherServersHandler::closeconnection(int sock)
{
    if (sock==-1) return;
    shutdown(sock,2);
    close(sock);
}

unsigned long long OtherServersHandler::systemTimeInMs()
{
    using namespace std::chrono;
    milliseconds ms = duration_cast< milliseconds >(
                          system_clock::now().time_since_epoch()
                      );
    return ms.count();
}

#include "OtherServersHandler.h"
#include "RequestBuilder.h"
#include "RequestProcessor.h"

#define reachOutRoutineSleepTimeInMcrS 200000
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
    attemptInterrupterStopped=true;
    if (msgBuilder!=nullptr) msgBuilder->setServersHandler(this);

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
    contacts_mutex.unlock();

    reachOutRunning=false;
    while (!reachOutStopped) usleep(100000);
    while (!attemptInterrupterStopped) usleep(100000);

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
}

OtherServersHandler::~OtherServersHandler()
{

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
            activeUntil = min(activeUntil, ci->actingUntil);
            if (ci->ip.compare(ip) == 0 && ci->port == port)
            {
                ci->actingUntil = activeUntil;
                ci->validSince = validSince;
                contacts_mutex.unlock();
                return;
            }
            contactsReachable.erase(notary);
            trashContact(ci);
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
            activeUntil = min(activeUntil, ci->actingUntil);
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
            activeUntil = min(activeUntil, ci->actingUntil);
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
    ContactHandler* contact = new ContactHandler(notary, ip, port, validSince, activeUntil);
    if (contact->ip.length()>3) contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
    else contactsUnreachable.insert(pair<unsigned long, ContactHandler*>(notary, contact));
    contacts_mutex.unlock();
}

// mutex must be locked before start of this (and will be locked after):
void OtherServersHandler::connectTo(unsigned long notary)
{
    if (!reachOutRunning) return;

    ContactHandler* ci;
    if (contactsToReach.count(notary)!=1)
    {
        if (contactsReachable.count(notary)!=1) return;
        else ci=contactsReachable[notary];
    }
    else ci=contactsToReach[notary];

    // exit if connection still open
    if (ci->socket!=-1 && ci->listen) return;

    // exit if not fully stopped
    if (!ci->threadStopped)
    {
        ci->stopListener();
        return;
    }

    // delete notary if not acting
    const unsigned long long currentTime = systemTimeInMs();
    if (ci->actingUntil <= currentTime)
    {
        contactsReachable.erase(notary);
        contactsToReach.erase(notary);
        trashContact(ci);
        return;
    }

    if (ci->answerBuilder != nullptr)
    {
        puts("OtherServersHandler::connectTo: Error: answerBuilder already exists");
        return;
    }

    const string ip = ci->ip;
    const int port = ci->port;

    socketNrInAttempt=-1;
    contacts_mutex.unlock(); // unlock for the connection build up
    usleep(connectionTimeOutCheckInMs * 3 * 1000);

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) goto fail;

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    // start attemptInterruptionThread
    socketNrInAttempt=sock;
    if(pthread_create(&attemptInterruptionThread, NULL, attemptInterrupter, (void*) this) < 0)
    {
        goto fail;
    }
    pthread_detach(attemptInterruptionThread);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) goto fail;
    else // connection established
    {
        contacts_mutex.lock();
        socketNrInAttempt=-1;

        // check that everything is still the same with ci
        ContactHandler* ci2;
        if (contactsToReach.count(notary)!=1)
        {
            if (contactsReachable.count(notary)!=1)
            {
                closeconnection(sock);
                return;
            }
            else ci2=contactsReachable[notary];
        }
        else ci2=contactsToReach[notary];
        // exit if necessary
        if (ci!=ci2)
        {
            closeconnection(sock);
            return;
        }
        else if (ci->socket!=-1 && ci->listen)
        {
            if (ci->socket!=sock) closeconnection(sock);
            return;
        }
        else if (!ci->threadStopped)
        {
            closeconnection(sock);
            ci->stopListener();
            return;
        }
        if (ci->answerBuilder != nullptr)
        {
            puts("OtherServersHandler::connectTo: answerBuilder already exists");
            if (ci->socket!=sock) closeconnection(sock);
            return;
        }

        ci->socket=sock;
        ci->listen=true;
        ci->answerBuilder=new RequestBuilder(maxRequestLength, answers, sock);
        if(pthread_create(&ci->listenerThread, NULL, socketReader, (void*) ci) < 0)
        {
            puts("OtherServersHandler::connectTo: could not create socketReader thread");
            ci->listen=false;
            closeconnection(ci->socket);
            delete ci->answerBuilder;
            ci->answerBuilder=nullptr;
            ci->socket=-1;
            return;
        }
        pthread_detach(ci->listenerThread);

        // mark as reachable
        if (contactsToReach.count(notary)==1)
        {
            contactsToReach.erase(notary);
            contactsReachable.insert(pair<unsigned long, ContactHandler*>(notary, ci));
        }

        // send outstanding messages + flush buffer:
        ci->message_buffer_mutex.lock();
        // append connection close msg
        string msg;
        byte type = 20;
        msg.push_back((char)type);
        msgBuilder->packMessage(&msg);
        ci->messagesStr.append(msg);
        // send and exit
        send(ci->socket, ci->messagesStr.c_str(), ci->messagesStr.length(), 0);
        ci->messagesStr="";
        ci->message_buffer_mutex.unlock();
        return;
    }
fail:
    closeconnection(sock);

    contacts_mutex.lock();

    if (contactsToReach.count(notary)!=1)
    {
        if (contactsReachable.count(notary)!=1) return;
        else ci=contactsReachable[notary];
    }
    else ci=contactsToReach[notary];

    if (ci->ip.compare(ip)==0 && ci->port==port)
    {
        ci->failedAttempts++;
        if (contactsToReach.count(notary)==1 && ci->failedAttempts > maxAttempts)
        {
            contactsToReach.erase(notary);
            contactsUnreachable.insert(pair<unsigned long, ContactHandler*>(notary, ci));
        }
        else if (contactsReachable.count(notary)==1)
        {
            ContactHandler* contact = new ContactHandler(notary, ci->ip, ci->port, ci->validSince, ci->actingUntil);
            trashContact(ci);
            contactsReachable.erase(notary);
            contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
        }
    }
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
    if (!reachOutRunning) return 0;
    unsigned long long out;
    contacts_mutex.lock();
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
    if (!reachOutRunning) return false;
    contacts_mutex.lock();
    const bool success = (contactsReachable.size() == notariesListLength) &&
                         (contactsReachable.size() >= contactsToReach.size()+contactsUnreachable.size());
    contacts_mutex.unlock();
    return success;
}

bool OtherServersHandler::wellConnected()
{
    if (!reachOutRunning) return false;
    contacts_mutex.lock();
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
    if (!reachOutRunning) return 0;
    contacts_mutex.lock();
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
    if (!reachOutRunning) return false;
    contacts_mutex.lock();
    if (contactsReachable.count(notaryNr)<=0)
    {
        contacts_mutex.unlock();
        return false;
    }
    ContactHandler *ch = contactsReachable[notaryNr];
    ch->addMessage(msg);
    connectTo(notaryNr);
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
    if (contacts.size()==0 || notaries.size()==0 || !wellConnected()) return;
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

void OtherServersHandler::checkNewerEntry(unsigned char listType, CompleteID lastEntryID)
{
    set<unsigned long> reachableNotaries;

    contacts_mutex.lock();
    map<unsigned long, ContactHandler*>::iterator it;
    for (it=contactsReachable.begin(); it!=contactsReachable.end(); ++it)
    {
        reachableNotaries.insert(it->first);
    }
    contacts_mutex.unlock();

    // build message
    string message;
    byte type = 15;
    message.push_back(type);
    Util u;
    message.append(u.UcAsByteSeq(listType));
    message.append(lastEntryID.to20Char());
    msgBuilder->packMessage(&message);

    // send message to everyone
    for (set<unsigned long>::iterator iter=reachableNotaries.begin(); iter!=reachableNotaries.end(); ++iter)
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
    ch->stopListener();
    if (!ch->threadStopped) trashedContacts.insert(trashedContacts.end(), ch);
    else delete ch;
}

bool OtherServersHandler::removeTrash()
{
    list<ContactHandler*> removableContacts;
    list<ContactHandler*>::iterator it;
    ContactHandler* ch;
    for (it=trashedContacts.begin(); it!=trashedContacts.end(); ++it)
    {
        ch=*it;
        if (ch->threadStopped) removableContacts.insert(removableContacts.end(), ch);
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
                serversHandler->trashContact(ch);
            }
            else if (!ch->listen)
            {
                if (ch->lastListeningTime + tolerableConnectionGapInMs < currentTime || ch->failedAttempts>0)
                {
                    puts("OtherServersHandler::reachOutRoutine: no connection for too long");
                    ContactHandler* contact = new ContactHandler(notary, ch->ip, ch->port, ch->validSince, ch->actingUntil);
                    serversHandler->trashContact(ch);
                    serversHandler->contactsReachable.erase(notary);
                    serversHandler->contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
                }
                else if (ch->lastListeningTime + tolerableConnectionGapInMs/2 < currentTime || ch->msgStrLength()>0)
                {
                    serversHandler->connectTo(notary);
                }
            }
            else if (ch->lastConnectionTime + maxConnectionDurationInMs < currentTime)
            {
                string out = "OtherServersHandler::reachOutRoutine: connection time out, notary ";
                out.append(to_string(notary));
                puts(out.c_str());
                ContactHandler* contact = new ContactHandler(notary, ch->ip, ch->port, ch->validSince, ch->actingUntil);
                serversHandler->trashContact(ch);
                serversHandler->contactsReachable.erase(notary);
                serversHandler->contactsToReach.insert(pair<unsigned long, ContactHandler*>(notary, contact));
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
                    serversHandler->connectTo(notary);
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
    pthread_exit(NULL);
}

void* OtherServersHandler::attemptInterrupter(void *servers)
{
    OtherServersHandler* serversHandler = (OtherServersHandler*) servers;
    serversHandler->attemptInterrupterStopped = false;
    int sock = serversHandler->socketNrInAttempt;
    if (sock == -1) goto close;
    for(int counter = 0; serversHandler->reachOutRunning
            && counter*connectionTimeOutCheckInMs < connectionTimeOutInMs; counter++)
    {
        usleep(connectionTimeOutCheckInMs * 1000);
        if (sock != serversHandler->socketNrInAttempt)
        {
            // connection must have been established
            goto close;
        }
    }
    serversHandler->closeconnection(sock); // timeout
close:
    serversHandler->attemptInterrupterStopped = true;
    pthread_exit(NULL);
}

void* OtherServersHandler::socketReader(void* contactInfo)
{
    ContactHandler* ci = (ContactHandler*) contactInfo;
    ci->threadStopped=false;
    ci->failedAttempts=0;
    ci->lastConnectionTime = systemTimeInMs();
    ci->lastListeningTime = ci->lastConnectionTime;

    int n;
    byte buffer[1024];
    while(ci->listen)
    {
        n=recv(ci->socket, buffer, 1024, 0);
        if(n<=0) goto close;
        else
        {
            for (int i=0; i<n; i++)
            {
                if (!ci->listen || !ci->answerBuilder->addByte(buffer[i])) goto close;
            }
        }
    }
close:
    ci->lastListeningTime=systemTimeInMs();
    ci->listen=false;
    closeconnection(ci->socket);
    ci->socket=-1;
    delete ci->answerBuilder;
    ci->answerBuilder=nullptr;
    ci->threadStopped=true;

    pthread_exit(NULL);
}

OtherServersHandler::ContactHandler::ContactHandler(unsigned long n, string i, int p, unsigned long long v, unsigned long long au)
{
    notaryNr=n;
    ip=i;
    port=p;
    validSince=v;
    actingUntil=au;
    answerBuilder=nullptr;
    socket=-1;
    listen=false;
    threadStopped=true;
    failedAttempts=0;
    lastConnectionTime=0;
    lastListeningTime=0;
    messagesStr="";
}

void OtherServersHandler::ContactHandler::addMessage(string &msg)
{
    message_buffer_mutex.lock();
    messagesStr.append(msg);
    message_buffer_mutex.unlock();
}

size_t OtherServersHandler::ContactHandler::msgStrLength()
{
    message_buffer_mutex.lock();
    size_t out = messagesStr.length();
    message_buffer_mutex.unlock();
    return out;
}

void OtherServersHandler::ContactHandler::stopListener()
{
    listen=false;
    closeconnection(socket);
    socket=-1;
}

OtherServersHandler::ContactHandler::~ContactHandler()
{
    if (answerBuilder!=nullptr)
    {
        delete answerBuilder;
        answerBuilder=nullptr;
    }
}

set<unsigned long>* OtherServersHandler::genNotariesList(set<unsigned long> &actingNotaries)
{
    if (!reachOutRunning) return nullptr;
    set<unsigned long>* out = new set<unsigned long>();

    contacts_mutex.lock();
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

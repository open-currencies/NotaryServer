#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <mutex>
#include <thread>
#include <list>
#include "rsa.h"
#include "files.h"
#include "osrng.h"
#include "rocksdb/db.h"
#include "Database.h"
#include "TNtrNr.h"
#include "Util.h"
#include "RequestBuilder.h"
#include "RequestProcessor.h"
#include "OtherServersHandler.h"
#include "MessageBuilder.h"
#include "InternalThread.h"

#define maxRequestLength 524288
#define timeOutCheckFreq 5
#define timeOutTimeInMs 90000
#define maxClientsNum 50000

typedef uint8_t byte;
using namespace std;

void deleteTrashedClients();
void *socketListener(void *);
volatile bool socketListenerRunning;
void *timeOutCheckRoutine(void *);
void closeconnection(int);
void clientsReport();
bool toString(CryptoPP::RSA::PublicKey &, string &);

Database * db;
OtherServersHandler * servers;
RequestProcessor * requests;
MessageBuilder * msgBuilder;

struct Client
{
    thread* clientThread;
    volatile bool threadStopped;
    const int sock;
    Client(int s) : sock(s)
    {
        threadStopped=false;
        clientThread=nullptr;
    }
    ~Client()
    {
        if (clientThread != nullptr) delete clientThread;
    }
};

void clientHandler(Client* client);

mutex clients_mutex;
set<Client*> clients;
map<Client*, unsigned long long> connectionTimeByClient;
map<unsigned long long, set<Client*>*> clientsByConnectionTime;
set<Client*> trashedClients;

volatile bool running;

int main()
{
    running=true;
    puts("Loading server.conf ...");
    Util util;
    Util::ServerConf conf;
    string fileName = "server.conf";
    if (!util.loadServerConf(fileName, conf))
    {
        puts("could not load server.conf");
        exit(EXIT_FAILURE);
    }

    CryptoPP::RSA::PrivateKey privateKey;
    if (conf.privateKeyFile.length()>0)
    {
        puts("Loading private key ...");
        CryptoPP::ByteQueue byteQueue;
        CryptoPP::FileSource file(conf.privateKeyFile.c_str(), true);
        file.TransferTo(byteQueue);
        byteQueue.MessageEnd();
        privateKey.Load(byteQueue);
        CryptoPP::AutoSeededRandomPool rng;
        if(!privateKey.Validate(rng, 3))
        {
            puts("could not validate key");
            exit(EXIT_FAILURE);
        }
        puts("Private key validated successfully.");

        msgBuilder = new MessageBuilder(conf.ownNotaryNr, &privateKey);
    }
    else
    {
        msgBuilder = new MessageBuilder(conf.ownNotaryNr, nullptr);
    }

    // load other server public key
    CryptoPP::RSA::PublicKey publicKeyOther;
    puts("Loading public key of other server...");
    CryptoPP::ByteQueue byteQueue;
    CryptoPP::FileSource file(conf.otherServerPublicKeyFile.c_str(), true);
    file.TransferTo(byteQueue);
    byteQueue.MessageEnd();
    publicKeyOther.Load(byteQueue);

    puts("Loading database ...");
    db = new Database(conf.dbDir);

    if (!db->init(conf.ownNotaryNr.getNotaryNr(), conf.otherServerNotaryNr, &publicKeyOther))
    {
        puts("db initialization failed");
        exit(EXIT_FAILURE);
    }

    // set db and add own contact info to it
    msgBuilder->setDB(db);
    string ciStr;
    msgBuilder->addOwnContactInfoToDB(conf.ownIP, conf.ownPort, &ciStr);
    // store own contact info to ContactInfo.ci
    if (ciStr.length()>0)
    {
        ofstream outfile("ContactInfo.ci", ios::binary);
        outfile << ciStr;
        outfile.close();
    }

    if (!conf.ownNotaryNr.isZero())
    {
        // check if there is such an acting notary in the database
        // and that the public key is correct
        string str;
        if (!db->loadNotaryPubKey(conf.ownNotaryNr, str) || str.size() < 3)
        {
            puts("could not find notary public key in database");
            exit(EXIT_FAILURE);
        }
        CryptoPP::RSA::PublicKey publicKey(privateKey);
        string str2;
        if (!toString(publicKey, str2) || str2.size() < 3)
        {
            puts("could not retrieve public key from private key");
            exit(EXIT_FAILURE);
        }
        if (str.compare(str2) != 0)
        {
            puts("public key from private key does not match the db");
            exit(EXIT_FAILURE);
        }
        puts("public key found in db, notary verified");
    }

    puts("Connecting to other servers ...");
    servers=new OtherServersHandler(msgBuilder);
    db->addContactsToServers(servers, conf.ownNotaryNr.getNotaryNr());
    if (conf.otherServerIP.size() > 3 && conf.ownNotaryNr.getNotaryNr() != conf.otherServerNotaryNr.getNotaryNr() &&
            (conf.ownIP.compare(conf.otherServerIP) != 0 || conf.ownPort != conf.otherServerPort))
    {
        servers->addContact(conf.otherServerNotaryNr.getNotaryNr(), conf.otherServerIP, conf.otherServerPort,
                            conf.otherServerValidSince, db->actingUntil(conf.otherServerNotaryNr));
    }
    requests=new RequestProcessor(db, servers, msgBuilder);
    servers->startConnector(requests);

    puts("Starting internal thread ...");
    InternalThread internal(db, servers, msgBuilder);
    internal.start();

    string msg("Starting socket listening on port ");
    msg.append(to_string(conf.ownPort));
    puts(msg.c_str());
    socketListenerRunning = false;
    pthread_t socketListenerThread;
    if(pthread_create(&socketListenerThread, NULL, socketListener, (void*) &conf.ownPort) < 0)
    {
        puts("could not create socket listener thread");
        exit(EXIT_FAILURE);
    }
    pthread_detach(socketListenerThread);

    // start time out check thread
    puts("starting time out check thread");
    pthread_t timeOutThread;
    if(pthread_create(&timeOutThread, NULL, timeOutCheckRoutine, (void*) &clients) < 0)
    {
        puts("could not create time out check thread");
        exit(EXIT_FAILURE);
    }
    pthread_detach(timeOutThread);

    puts("Server started, waiting for commands.");
    string command;
    for (int k=0; k<100 && command.compare("stop")!=0; k++)
    {
        command.clear();
        getline(cin, command);
        string msg("Command: ");
        msg.append(command);
        puts(msg.c_str());
        if (command.compare("stop")==0)
        {
        }
        else if (command.compare("rocksdb")==0)
        {
            db->rocksdbReport();
        }
        else if (command.compare("clients")==0)
        {
            clientsReport();
        }
        else if (command.compare("servers")==0)
        {
            servers->contactsReport();
        }
        else if (command.compare("uptodate")==0)
        {
            db->upToDateReport();
        }
        else
        {
            puts("unknown command");
        }
    }

    // shutdown routine:
    running=false;
    internal.stopSafely();
    clients_mutex.lock();
    if (socketListenerRunning) pthread_cancel(socketListenerThread);
    set<Client*>::iterator it;
    for (it=clients.begin(); it!=clients.end(); ++it)
    {
        Client* client=*it;
        closeconnection(client->sock);
    }
    clients_mutex.unlock();
    bool noclients=false;
    do
    {
        clients_mutex.lock();
        noclients=(clients.size()==0);
        deleteTrashedClients();
        clients_mutex.unlock();
        if (!noclients) sleep(1);
    }
    while (!noclients);
    servers->stopSafely();

    delete servers;
    delete requests;
    delete db;
    delete msgBuilder;

    exit(EXIT_SUCCESS);
}

void clientsReport()
{
    clients_mutex.lock();
    string msg;
    msg.append("Clients: ");
    msg.append(to_string(clients.size()));
    msg.append("\nSize of connectionTimeByClient: ");
    msg.append(to_string(connectionTimeByClient.size()));
    msg.append("\nSize of clientsByConnectionTime: ");
    msg.append(to_string(clientsByConnectionTime.size()));
    msg.append("\ntrashed Clients: ");
    msg.append(to_string(trashedClients.size()));
    puts(msg.c_str());
    clients_mutex.unlock();
}

void closeconnection(int sock)
{
    if (sock==-1) return;
    shutdown(sock,2);
    close(sock);
}

bool toString(CryptoPP::RSA::PublicKey &publicKey, string &str)
{
    CryptoPP::ByteQueue bqueue;
    publicKey.Save(bqueue);
    CryptoPP::byte runningByte;
    string out;
    size_t nr;
    do
    {
        nr = bqueue.Get(runningByte);
        if (nr != 1) break;
        out.push_back(runningByte);
    }
    while (nr == 1);
    if (out.size()<3) return false;
    str.append(out);
    return true;
}

void deleteTrashedClients()
{
    set<Client*> removableClients;
    set<Client*>::iterator it;
    for (it=trashedClients.begin(); it!=trashedClients.end(); ++it)
    {
        if (*it!=nullptr && ((Client*)*it)->threadStopped) removableClients.insert(*it);
    }
    for (it=removableClients.begin(); it!=removableClients.end(); ++it)
    {
        trashedClients.erase(*it);
        delete *it;
    }
    removableClients.clear();
}

// open socket and wait for clients
void *socketListener(void *portnr)
{
    socketListenerRunning = true;

    int server_fd = -1;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    bool listening = false;
    bool wantToListen = true;

    while(running)
    {
        // stop listening if applicable
        if (listening && !wantToListen)
        {
            puts("shutting down socket listening for now");
            shutdown(server_fd,2);
            close(server_fd);

            listening = false;
            sleep(4);
            continue;
        }
        else wantToListen = true;

        // pause if too many clients already
        clients_mutex.lock();
        if (clients.size() >= maxClientsNum)
        {
            clients_mutex.unlock();

            wantToListen = false;
            sleep(1);
            continue;
        }
        else clients_mutex.unlock();

        // pause if this notary is not acting
        db->lock();
        if (!wantToListen || !db->amCurrentlyActing())
        {
            db->unlock();

            wantToListen = false;
            sleep(1);
            continue;
        }
        else db->unlock();

        // build up connection
        if (!listening)
        {
            // creating socket file descriptor
            if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
            {
                puts("socket failed");
                exit(EXIT_FAILURE);
            }

            if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
            {
                puts("setsockopt");
                exit(EXIT_FAILURE);
            }
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(*(int*)portnr);

            if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
            {
                puts("bind failed");
                exit(EXIT_FAILURE);
            }
            if (listen(server_fd, 3) < 0)
            {
                puts("listen failed");
                exit(EXIT_FAILURE);
            }

            // accept incoming connections
            string msg("Waiting for incoming connections on sin_port ");
            msg.append(to_string(address.sin_port));
            puts(msg.c_str());

            listening = true;
            sleep(1);
            continue;
        }

        // accept new client
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (!running || new_socket<0)
        {
            if (new_socket!=-1) closeconnection(new_socket);
            continue;
        }

        // check if this notary is banned
        unsigned long long wellConnectedSince = servers->getWellConnectedSince();
        db->lock();
        bool amBanned = !db->amCurrentlyActingWithBuffer() || !db->dbUpToDate(wellConnectedSince);
        db->unlock();
        if (amBanned) msgBuilder->sendAmBanned(new_socket);

        // create and start client thread
        Client* newClient = new Client(new_socket);

        clients_mutex.lock();
        clients.insert(newClient);
        const unsigned long long currentTime = msgBuilder->systemTimeInMs();
        connectionTimeByClient.insert(pair<Client*, unsigned long long>(newClient, currentTime));
        if (clientsByConnectionTime.count(currentTime)<=0)
        {
            set<Client*>* emptyList = new set<Client*>();
            clientsByConnectionTime.insert(pair<unsigned long long, set<Client*>*>(currentTime, emptyList));
        }
        clientsByConnectionTime[currentTime]->insert(newClient);
        clients_mutex.unlock();

        newClient->clientThread = new thread(clientHandler, newClient);
        ((thread*)newClient->clientThread)->detach();

        // clean up trash
        clients_mutex.lock();
        deleteTrashedClients();
        clients_mutex.unlock();
    }
    puts("socketListener not running");

    socketListenerRunning = false;
    return NULL;
}

// handle connection for each client
void clientHandler(Client* client)
{
    const int sock = client->sock;
    byte* buffer = new byte[1024];
    RequestBuilder builder(maxRequestLength, requests, sock);

    // listen
    while(running)
    {
        int n = recv(sock, buffer, 1024, 0);
        if(n<=0 || n>1024) goto close;
        else
        {
            for (int i=0; i<n; i++)
            {
                if (!running || !builder.addByte(buffer[i])) goto close;
            }
        }
    }
close:
    delete[] buffer;
    clients_mutex.lock();
    closeconnection(client->sock);
    trashedClients.insert(client);
    clients.erase(client);
    if (connectionTimeByClient.count(client)>0)
    {
        unsigned long long connectionTime = connectionTimeByClient[client];
        connectionTimeByClient.erase(client);
        if (clientsByConnectionTime.count(connectionTime)>0)
        {
            clientsByConnectionTime[connectionTime]->erase(client);
            if (clientsByConnectionTime[connectionTime]->size()<=0)
            {
                delete clientsByConnectionTime[connectionTime];
                clientsByConnectionTime.erase(connectionTime);
            }
        }
    }
    client->threadStopped=true;
    clients_mutex.unlock();
    return;
}

void *timeOutCheckRoutine(void *clientsList)
{
    int counter = 0;
    while(running)
    {
        if (counter < timeOutCheckFreq)
        {
            counter++;
            sleep(1);
            continue;
        }
        counter = 0;

        unsigned long long currentTime = msgBuilder->systemTimeInMs();
        clients_mutex.lock();
        map<unsigned long long, set<Client*>*>::iterator it;
        for (it=clientsByConnectionTime.begin(); it!=clientsByConnectionTime.end(); ++it)
        {
            unsigned long long connectionTime = it->first;
            if (connectionTime + timeOutTimeInMs < currentTime)
            {
                set<Client*>* clientsSet = it->second;
                set<Client*>::iterator it2;
                for (it2=clientsSet->begin(); it2!=clientsSet->end(); ++it2)
                {
                    puts("timeOutCheckRoutine: closing due to time out");
                    closeconnection((*it2)->sock);
                }
            }
            else break;
        }
        clients_mutex.unlock();
    }
}

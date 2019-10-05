#ifndef REQUESTBUILDER_H
#define REQUESTBUILDER_H
#include <stdint.h>
#include <time.h>

#define maxLong 4294967295
typedef unsigned char byte;

class RequestProcessor;

class RequestBuilder
{
public:
    RequestBuilder(const unsigned long maxLength, RequestProcessor* r, const int s);
    ~RequestBuilder();
    bool addByte(byte b);
    unsigned long getLastDataTime();
protected:
private:
    volatile unsigned long lastDataTime;
    const unsigned long maxRequestLength;
    RequestProcessor* requests;
    const int socket;

    byte *request;
    unsigned long requestLength;
    unsigned long p;
    unsigned long checkSum;
    unsigned long targetCheckSum;
    int countToFour;

    static unsigned long addModulo(unsigned long a, unsigned long b);
};

#endif // REQUESTBUILDER_H

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
    const RequestProcessor* requests;
    const int socket;

    volatile byte *request;
    volatile unsigned long requestLength;
    volatile unsigned long p;
    volatile unsigned long checkSum;
    volatile unsigned long targetCheckSum;
    volatile int countToFour;

    static unsigned long addModulo(unsigned long a, unsigned long b);
};

#endif // REQUESTBUILDER_H

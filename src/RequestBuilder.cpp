#include "RequestBuilder.h"
#include "RequestProcessor.h"

RequestBuilder::RequestBuilder(const unsigned long maxLength, RequestProcessor* r, const int s)
    : maxRequestLength(maxLength), requests(r), socket(s)
{
    requestLength=0;
    p=0;
    checkSum=0;
    countToFour=0;
    targetCheckSum=0;
    lastDataTime=static_cast<unsigned long>(time(NULL));
}

RequestBuilder::~RequestBuilder()
{
    if (p>0) delete[] request;
}

bool RequestBuilder::addByte(byte b)
{
    lastDataTime=static_cast<unsigned long>(time(NULL));
    if (p==0 && countToFour<4) // we have a new message
    {
        requestLength=requestLength*256+b;
        if (requestLength>maxRequestLength) goto error;
        countToFour++;
        return true;
    }
    else if (requestLength>0 && p==0 && countToFour==4) // start new request
    {
        if (requestLength>maxRequestLength) goto error;
        request = new byte[requestLength];
        request[p]=b;
        p++;
        countToFour = 0;
        checkSum = addModulo(checkSum, b);
        return true;
    }
    else if (p>0 && p<requestLength && countToFour==0) // continue to build message
    {
        request[p]=b;
        p++;
        checkSum = addModulo(checkSum, b);
        return true;
    }
    else if (p>0 && p==requestLength) // end of message
    {
        if (countToFour<4)
        {
            targetCheckSum=targetCheckSum*256+b;
            countToFour++;
        }
        if (countToFour<4) return true;
        else  // checking the checksum
        {
            const bool success = (targetCheckSum==checkSum);
            if (success && requests!=nullptr) ((RequestProcessor*)requests)->process(requestLength, (byte*)request, socket);
            delete[] request;
            requestLength=0;
            countToFour=0;
            targetCheckSum=0;
            checkSum=0;
            p=0;
            return success;
        }
    }
error:
    requestLength=0;
    countToFour=0;
    targetCheckSum=0;
    checkSum=0;
    if (p>0) delete[] request;
    p=0;
    return false;
}

unsigned long RequestBuilder::addModulo(unsigned long a, unsigned long b)
{
    const unsigned long diff = maxLong - a;
    if (b>diff) return b-diff-1;
    else return a+b;
}

unsigned long RequestBuilder::getLastDataTime()
{
    return lastDataTime;
}

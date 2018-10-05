#ifndef R_RDMAIMPL_HPP_
#define R_RDMAIMPL_HPP_

#include <infinity/core/Context.h>
#include <infinity/memory/Buffer.h>
#include <infinity/memory/RegionToken.h>
#include <infinity/queues/QueuePair.h>
#include <infinity/queues/QueuePairFactory.h>
#include <infinity/requests/RequestToken.h>
using namespace infinity;

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "fuckhust.hpp"

/////////////////////// current time
#include <string>
#include <stdio.h>
#include <time.h>

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
inline const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    return buf;
}
//////////////////// current time end

#ifdef RDEBUG
#define rdma_debug std::cerr << currentDateTime() << "|RdmaNative Debug: "
#else
#define rdma_debug                                                                                                             \
    if (false)                                                                                                                 \
    std::cerr
#endif
#define rdma_error std::cerr << currentDateTime() << "|RdmaNative: "

#ifndef uint32_t
#define uint32_t unsigned int
#endif
#ifndef uint64_t
#define uint64_t unsigned long long
#endif

#define INIT_BUFFER_SIZE 4096

template <typename T> inline void checkedDelete(T *&ptr) {
    if (ptr)
        delete ptr;
    ptr = nullptr;
}

extern core::Context *context;
extern queues::QueuePairFactory *qpFactory;

#if HUST
#define magic_t uint32_t
enum {
#else
enum magic_t : uint32_t {
#endif
    MAGIC_CONNECTED = 0x00000000,
    MAGIC_SERVER_BUFFER_READY = 0xffffffff,
    MAGIC_QUERY_WROTE = 0xaaaaaaaa,
    MAGIC_QUERY_READ = 0xaaaa5555,
    MAGIC_RESPONSE_READY = 0x55555555
};
#if HUST
namespace std {
extern std::string to_string(magic_t &m);
extern std::string to_string(int &m);
}
#endif
struct ServerStatusType {
    magic_t magic;
    volatile uint64_t currentQueryLength;
    uint64_t currentResponseLength;
    memory::RegionToken dynamicBufferToken;
};
typedef memory::RegionToken DynamicBufferTokenBufferTokenType;

class CRdmaServerConnectionInfo {
  private:
    queues::QueuePair *pQP; // must delete

    memory::Buffer *pDynamicBufferTokenBuffer;           // must delete
    memory::RegionToken *pDynamicBufferTokenBufferToken; // must delete
#define pServerStatus ((ServerStatusType *)pDynamicBufferTokenBuffer->getData())

    memory::Buffer *pDynamicBuffer;           // must delete
    uint64_t currentBufferSize;                        // Default 4K

    void initFixedLocalBuffer() {
        pDynamicBufferTokenBuffer = new memory::Buffer(context, sizeof(ServerStatusType));
        pDynamicBufferTokenBufferToken = pDynamicBufferTokenBuffer->createRegionToken();
        pDynamicBuffer = new memory::Buffer(context, currentBufferSize);
        pDynamicBuffer->createRegionTokenAt(&pServerStatus->dynamicBufferToken);
        pServerStatus->magic = MAGIC_CONNECTED;
        pServerStatus->currentQueryLength = 0;
    }

public:
    CRdmaServerConnectionInfo() : pQP(nullptr), pDynamicBuffer(nullptr), 
    pDynamicBufferTokenBuffer(nullptr), pDynamicBufferTokenBufferToken(nullptr), currentBufferSize(INIT_BUFFER_SIZE) {

    }
    ~CRdmaServerConnectionInfo() {
        checkedDelete(pQP);
        checkedDelete(pDynamicBuffer);
        checkedDelete(pDynamicBufferTokenBuffer);
    }

    void waitAndAccept() {
        initFixedLocalBuffer();
        pQP = qpFactory->acceptIncomingConnection(pDynamicBufferTokenBufferToken, sizeof(DynamicBufferTokenBufferTokenType));
    }

    std::string getClientIp() {
        return pQP->peerAddr;
    }

    bool isQueryReadable() {
        // Use this chance to check if client has told server its query size and allocate buffer.
        if (pServerStatus->magic == MAGIC_CONNECTED && pServerStatus->currentQueryLength != 0) {
        // Warning: pServerStatus->currentQueryLength may be under editing! The read value maybe broken!
        // So I set currentQueryLength as volatile and read it again.
        broken_value_read_again:
            uint64_t queryLength = pServerStatus->currentQueryLength;
            if (queryLength != pServerStatus->currentQueryLength)
                goto broken_value_read_again;

            if (queryLength > currentBufferSize) {
                rdma_debug << "allocating new buffer rather than reusing old one... new length " << queryLength << std::endl;
                checkedDelete(pDynamicBuffer);
                pDynamicBuffer = new memory::Buffer(context, queryLength);
                pDynamicBuffer->createRegionTokenAt(&pServerStatus->dynamicBufferToken);
                //pDynamicBuffer->resize(queryLength);
                currentBufferSize = queryLength;
            }
            pServerStatus->magic = MAGIC_SERVER_BUFFER_READY;
        }
        return pServerStatus->magic == MAGIC_QUERY_WROTE;
    }

    void readQuery(void *&dataPtr, uint64_t &dataSize) {
        rdma_debug << "SERVER " << (long)this << ": readQuery called" << std::endl;
        if (pServerStatus->magic != MAGIC_QUERY_WROTE)
            throw std::runtime_error("Query is not readable while calling readQuery");
        dataPtr = pDynamicBuffer->getData();
        dataSize = pServerStatus->currentQueryLength;
        pServerStatus->magic = MAGIC_QUERY_READ;
    }

    void writeResponse(const void *dataPtr, uint64_t dataSize) {
        rdma_debug << "SERVER " << (long)this << ": writeResponse called. dataPtr is " << (long)dataPtr << ",dataSize is " << dataSize << std::endl;
        if (pServerStatus->magic != MAGIC_QUERY_READ)
            throw std::runtime_error(std::string("write response: wrong magic. Want 0xaaaaaaaa, got ") +
                                     std::to_string(pServerStatus->magic));
        if(dataSize > currentBufferSize) {
            checkedDelete(pDynamicBuffer);
            pDynamicBuffer = new memory::Buffer(context, dataSize);
            //pDynamicBuffer->resize(dataSize);
            pDynamicBuffer->createRegionTokenAt(&pServerStatus->dynamicBufferToken);
            currentBufferSize = dataSize;
        }
        // TODO: here's an extra copy. use dataPtr directly and jni global reference to avoid it!
        pServerStatus->currentResponseLength = dataSize;
        std::memcpy(pDynamicBuffer->getData(), dataPtr, dataSize);

        if (pServerStatus->magic != MAGIC_QUERY_READ)
            throw std::runtime_error("write response: magic is changed while copying memory data.");
        pServerStatus->magic = MAGIC_RESPONSE_READY;
    }
};

class CRdmaClientConnectionInfo {
    queues::QueuePair *pQP;                                    // must delete
    memory::RegionToken *pRemoteDynamicBufferTokenBufferToken; // must not delete
    uint64_t remoteBufferCurrentSize; // If this query is smaller than last, do not wait for the server to allocate space.

    void rdmaSetServerCurrentQueryLength(uint64_t queryLength) {
        requests::RequestToken reqToken(context);
        memory::Buffer queryLenBuffer(context, sizeof(uint64_t));
        *(uint64_t *)queryLenBuffer.getData() = queryLength;
        pQP->write(&queryLenBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, currentQueryLength),sizeof(uint64_t), queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
    }
    uint64_t rdmaGetServerCurrentResponseLength() {
        memory::Buffer serverResponseLenBuffer(context, sizeof(uint64_t));
        requests::RequestToken reqToken(context);
        pQP->read(&serverResponseLenBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, currentResponseLength), sizeof(uint64_t), queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
        return *(uint64_t *)serverResponseLenBuffer.getData();
    }
    void rdmaSetServerMagic(magic_t magic) {
        requests::RequestToken reqToken(context);
        memory::Buffer serverMagicBuffer(context, sizeof(magic_t));
        *(magic_t *)serverMagicBuffer.getData() = magic;
#if !HUST
        static_assert(offsetof(ServerStatusType, magic) == 0, "Use read with more arg if offsetof(magic) is not 0.");
#endif
        pQP->write(&serverMagicBuffer, pRemoteDynamicBufferTokenBufferToken, sizeof(magic_t), &reqToken);
        reqToken.waitUntilCompleted();
    }

    magic_t rdmaGetServerMagic() {
        memory::Buffer serverMagicBuffer(context, sizeof(magic_t));
        requests::RequestToken reqToken(context);
        pQP->read(&serverMagicBuffer, pRemoteDynamicBufferTokenBufferToken, sizeof(magic_t), &reqToken);
        reqToken.waitUntilCompleted();
        return *(magic_t *)serverMagicBuffer.getData();
    }

    std::string jAddrToAddr(const char *jAddr) {
        // inode112/10.10.0.112:16020
        std::string tmp(jAddr);
        size_t slashLoc = tmp.find('/');
        if(slashLoc == std::string::npos)
            return tmp; // not jaddr
        size_t colonLoc = tmp.find(':');
        if(colonLoc == std::string::npos)
            throw std::invalid_argument(std::string("illegal jAddr `") + jAddr + "`. Example: host/1.2.3.4:1080");
        std::string realAddr = tmp.substr(slashLoc+1, colonLoc-slashLoc-1);
        return realAddr;
    }

  public:
    CRdmaClientConnectionInfo() : pQP(nullptr), pRemoteDynamicBufferTokenBufferToken(nullptr),
    remoteBufferCurrentSize(INIT_BUFFER_SIZE) {

    }
    ~CRdmaClientConnectionInfo() { checkedDelete(pQP); }

    void connectToRemote(const char *serverAddr, int serverPort) {
        pQP = qpFactory->connectToRemoteHost(jAddrToAddr(serverAddr).c_str(), serverPort);
        pRemoteDynamicBufferTokenBufferToken = reinterpret_cast<memory::RegionToken *>(pQP->getUserData());
    }

    void writeQuery(void *dataPtr, uint64_t dataSize) {
        rdma_debug << "CLIENT " << (long)this << ": writeQuery called. dataSize is " << dataSize << std::endl;
        memory::Buffer wrappedDataBuffer(context, dataPtr, dataSize);
        memory::Buffer wrappedSizeBuffer(context, &dataSize, sizeof(dataSize));
#if !HUST
#if __cplusplus < 201100L
        static_assert(std::is_pod<ServerStatusType>::value == true, "ServerStatusType must be pod to use C offsetof.");
#else
        static_assert(std::is_standard_layout<ServerStatusType>::value == true,
                      "ServerStatusType must be standard layout in cxx11 to use C offsetof.");
#endif
#endif
        requests::RequestToken reqToken(context);
        // write data size.
        pQP->write(&wrappedSizeBuffer, 0, pRemoteDynamicBufferTokenBufferToken, offsetof(ServerStatusType, currentQueryLength),
                   sizeof(dataSize), queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();

        // Wait for the server allocating buffer...
        if (dataSize > remoteBufferCurrentSize) {
            rdma_debug << "server is allocating new buffer rather than reusing old one..." << std::endl;
            while (true) {
                static_assert(offsetof(ServerStatusType, magic) == 0, "Use read with more arg if offsetof(magic) is not 0.");
                if (MAGIC_SERVER_BUFFER_READY == rdmaGetServerMagic())
                    break; // Remote buffer is ready. Fire!
            }
            remoteBufferCurrentSize = dataSize;
        }
        // write the real data
        memory::Buffer tempTokenBuffer(context, sizeof(ServerStatusType));
        pQP->read(&tempTokenBuffer, pRemoteDynamicBufferTokenBufferToken, &reqToken);
        reqToken.waitUntilCompleted();
        memory::RegionToken remoteDynamicBufferToken = ((ServerStatusType *)tempTokenBuffer.getData())->dynamicBufferToken;
        rdma_debug << "real data write ---" << std::endl;
        // workaround for buffer to large unknown bug(at most 2048 byte per read/write):
        for(size_t cter = 0; cter < dataSize / 2048; ++cter) {
            pQP->write(&wrappedDataBuffer, cter*2048, &remoteDynamicBufferToken, cter*2048, 2048, queues::OperationFlags(), &reqToken);
        }
        if(dataSize % 2048 != 0)
            pQP->write(&wrappedDataBuffer, dataSize/2048*2048, &remoteDynamicBufferToken, dataSize/2048*2048, dataSize%2048, queues::OperationFlags(), &reqToken);
        reqToken.waitUntilCompleted();
        rdma_debug << "real data write done ---" << std::endl;

        rdmaSetServerMagic(MAGIC_QUERY_WROTE);
    }

    bool isResponseReady() { return rdmaGetServerMagic() == MAGIC_RESPONSE_READY; }

    void readResponse(memory::Buffer *&pResponseDataBuf) {
        rdma_debug << "CLIENT " << (long)this << ": readResponse called." << std::endl;
        // Undefined behavior if the response is not ready.
        requests::RequestToken reqToken(context);
        memory::Buffer tempTokenBuffer(context, sizeof(ServerStatusType));
        pQP->read(&tempTokenBuffer, pRemoteDynamicBufferTokenBufferToken, &reqToken);
        reqToken.waitUntilCompleted();
        memory::RegionToken remoteDynamicBufferToken = ((ServerStatusType *)tempTokenBuffer.getData())->dynamicBufferToken;
        uint64_t realResponseLen = rdmaGetServerCurrentResponseLength();
        memory::Buffer *pResponseData = new memory::Buffer(context, realResponseLen);
        // workaround for buffer to large unknown bug(at most 2048 byte per read/write):
        //pQP->read(pResponseData, &remoteDynamicBufferToken, realResponseLen, &reqToken);
        for(size_t cter = 0; cter < realResponseLen / 2048; ++cter) {
            pQP->read(pResponseData, cter*2048, &remoteDynamicBufferToken, cter*2048, 2048, queues::OperationFlags(), &reqToken);
        }
        if(realResponseLen % 2048 != 0)
            pQP->read(pResponseData, realResponseLen/2048*2048, &remoteDynamicBufferToken,  realResponseLen/2048*2048, realResponseLen%2048, queues::OperationFlags(), &reqToken);
 
        // Set the server status to initial status after used!
        rdmaSetServerCurrentQueryLength(0); // will wait for complete
        rdmaSetServerMagic(MAGIC_CONNECTED);

        pResponseDataBuf = pResponseData;
        if(pResponseData->getSizeInBytes() > remoteBufferCurrentSize) {
            remoteBufferCurrentSize = pResponseData->getSizeInBytes();
        }
        // WARNING: You must delete the pResponseDataBuf after using it!!!
    }
};

#endif

#ifndef _NETCONN_H_
#define _NETCONN_H_

#include <vector>
#include <set>
#include <stdint.h>

#include "uv.h"
#include "PktItem.h"

//网络层conn

struct NetHandler;
struct PktItem;
class NetLoop;

struct NetConn
{
public:
    NetConn(uint32_t sid, PktItemPool *pktpool, NetHandler *phandler) : sessionId(sid), m_pktItemPool(pktpool), m_handler(phandler) {}
    ~NetConn();

public:
    void OnReadAfter(size_t nread);
    PktItem *OnReadAlloc()
    {
        this->m_currReadPkt = this->m_pktItemPool->Get();
        return this->m_currReadPkt;
    }

public:
    void Cycle(PktItem *pkt)
    {
        for (auto *ptmp = pkt; ptmp != nullptr; ptmp = ptmp->nextPkt)
            this->m_pktItemPool->Cycle(ptmp);
    }

//for packet process
public:
    inline uint32_t GetReadLen() { return this->m_readcount; }
    //copy len data to buf
    void CopyOut(uint32_t len, uint8_t *buf);
    //越过offset长度copy len到buf
    void CopyOut(uint32_t len, uint32_t offset, uint8_t *buf);
    void CopyAndDrain(uint32_t len, uint8_t *buf);
    void Drain(uint32_t len);


public:
    uint32_t remoteAddr;
    uint32_t sessionId;
    PktItem *m_head = nullptr;
    PktItem *m_tail = nullptr;
    PktItem *m_currReadPkt = nullptr;

    uint32_t m_readcount = 0;

    PktItemPool *m_pktItemPool;
    NetHandler *m_handler;
};
typedef MemPool<NetConn, 1024> ConnPool;
#endif // _NETCONN_H_
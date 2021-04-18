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
    NetConn(uint32_t sid, PktItemPool *pktpool, NetHandler* phandler) : sessionId(sid), m_pktItemPool(pktpool), m_handler(phandler){}
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
        for(auto* ptmp = pkt; ptmp != nullptr; ptmp = ptmp->nextPkt)
            this->m_pktItemPool->Cycle(ptmp);
    }

public:
    uint32_t remoteAddr;
    uint32_t sessionId;
    PktItem *m_head = nullptr;
    PktItem *m_tail = nullptr;
    PktItem* m_currReadPkt = nullptr;

    uint32_t m_readcount = 0;

    //缓存当前的包头 有那么点性能优化
    uint16_t m_currmsglen = 0;
    uint16_t m_currheaderlen = 0;

    PktItemPool *m_pktItemPool;
    NetHandler  *m_handler;
};
typedef MemPool<NetConn, 1024> ConnPool;
#endif	// _NETCONN_H_
#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <strings.h>
#include <iostream> 

#include "PktItem.h"
#include "NetConn.h"

void NetConn::OnReadAfter(size_t nread)
{
    this->m_currReadPkt->write += nread;
    if(m_tail != nullptr)
    {
        this->m_tail->nextPkt = m_currReadPkt;
        this->m_tail = m_currReadPkt;
    }
    else
    {
        this->m_head = m_currReadPkt;
        this->m_tail = m_currReadPkt;
    }

    this->m_readcount += nread;
}


NetConn::~NetConn()
{
    for(auto* tmp = this->m_head; tmp != nullptr;)
    {
        auto* pkt = tmp;
        tmp = tmp->nextPkt;
        this->m_pktItemPool->Cycle(pkt);
    }
}

uint32_t pkt_len(PktItem *pkt)
{
    uint32_t len = 0;
    for (auto *ptmp = pkt; ptmp != nullptr; ptmp = ptmp->nextPkt)
        len += ptmp->GetCanReadCount();
    return len;
}

void NetConn::CopyAndDrain(uint32_t len, uint8_t *buf)
{
    assert(this->GetReadLen() >= len);
    uint32_t count = len;
    uint32_t hasread = 0;
    auto *pwalker = this->m_head;
    for (; pwalker != nullptr;)
    {
        if (pwalker->GetCanReadCount() >= count)
        {
            memcpy(buf + hasread, pwalker->GetReadAddr(), count);
            pwalker->read += count;
            break;
        }
        else
        {
            auto *ptmp = pwalker->nextPkt;
            memcpy(buf + hasread, pwalker->GetReadAddr(), pwalker->GetCanReadCount());
            hasread += pwalker->GetCanReadCount();
            count -= pwalker->GetCanReadCount();
            this->m_pktItemPool->Cycle(pwalker);
            pwalker = ptmp;
        }
    }

    if (pwalker->GetCanReadCount() == 0)
    {
        this->m_head = pwalker->nextPkt;
        if(m_tail == pwalker)
            m_tail = nullptr;
        this->m_pktItemPool->Cycle(pwalker);
    }
    else
    {
        this->m_head = pwalker;
    }
    this->m_readcount -= len;
    assert(this->m_readcount == pkt_len(m_head));
}

void NetConn::CopyOut(uint32_t len, uint8_t* buf)
{
    assert(this->GetReadLen() >= len);

    uint32_t count = len;
    uint32_t hasread = 0;
    auto *pwalker = this->m_head;
    for (; pwalker != nullptr; )
    {
        if (pwalker->GetCanReadCount() >= count)
        {
            memcpy(buf + hasread, pwalker->GetReadAddr(), count);
            break;
        }
        else
        {
            auto* ptmp = pwalker->nextPkt;
            memcpy(buf + hasread, pwalker->GetReadAddr(), pwalker->GetCanReadCount());
            hasread += pwalker->GetCanReadCount();
            count -= pwalker->GetCanReadCount();
            pwalker = ptmp;
        }
    }
}

void NetConn::Drain(uint32_t len)
{
    assert(this->GetReadLen() >= len);

    uint32_t count = len;
    auto *pwalker = this->m_head;
    for (; pwalker != nullptr;)
    {
        if (pwalker->GetCanReadCount() > count)
        {
            pwalker->read += count;
            break;
        }
        else if (pwalker->GetCanReadCount() < count)
        {
            auto *ptmp = pwalker->nextPkt;
            count -= pwalker->GetCanReadCount();
            this->m_pktItemPool->Cycle(pwalker);
            pwalker = ptmp;
        }
        else
        {
            m_head = pwalker->nextPkt;
            if(m_tail == pwalker)
                m_tail = nullptr;
            this->m_pktItemPool->Cycle(pwalker);
            break;
        }        
    }

    this->m_readcount -= len;
    assert(this->m_readcount == pkt_len(m_head));
}

#include <stdint.h>
#include <assert.h>
#include <memory.h>
#include <strings.h>

#include "PktItem.h"
#include "NetConn.h"

void NetConn::OnReadAfter()
{
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
}


NetConn::~NetConn()
{
    for(auto* tmp = this->m_head; tmp != nullptr;)
    {
        auto* pkt = tmp;
        this->m_pktItemPool->Cycle(pkt);
    }
}


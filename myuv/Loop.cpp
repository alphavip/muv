#include <vector>
#include <iostream>
#include <map>

#include "NetConn.h"
#include "PktItem.h"
#include "NetHandler.h"

#include "uv.h"
#include "Loop.h"

void WriteData::DecRef()
{
    assert(ref > 0);
    if (--ref == 0)
    {
        delete buff;
        ploop->writeDataPool.Cycle(this);
    }
}

bool NetLoop::Init()
{
    this->uvloop = uv_loop_new();
    this->uvloop->data = this;

    this->uvconns.resize(InitTcpConns);
    for (uint32_t i = 0; i < this->uvconns.size(); ++i)
    {
        this->uvconns[i] = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        freeslots.insert(i);
    }

    return 0;
}

void NetLoop::UnInit()
{

}

void NetLoop::Start()
{
    uv_run(this->uvloop, UV_RUN_DEFAULT);
}

void NetLoop::Stop()
{
    uv_stop(this->uvloop);
}

void allocBuf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    NetConn *pconn = (NetConn *)handle->data;
    PktItem* pkt = pconn->OnReadAlloc();

    buf->base = (char *)pkt->GetWriteAddr();
    buf->len = pkt->GetCanWriteCount();
}

void writeCallBack(uv_write_t *req, int status)
{
    WriteData* pwd = (WriteData*)req->data;
    pwd->ploop->writeReqPool.Cycle(req);
    pwd->DecRef();
}

void closeCallBack(uv_handle_t *handle)
{
    NetConn *pconn = (NetConn *)handle->data;
    NetLoop* ploop = (NetLoop *)handle->loop->data;
    ploop->Cycle(pconn);
}

void afterRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    NetConn *nc = reinterpret_cast<NetConn *>(stream->data);
    if (nread < 0)
    {
        if (nc != nullptr)
        {
            nc->m_handler->OnClose(*nc, nread);
        }

        uv_close((uv_handle_t *)stream, closeCallBack);
        return;
    }
    if (nc != nullptr)
    {
        //从开头开始写入的,还没加
        assert(buf->base == (char *)nc->m_currReadPkt->GetWriteAddr());
        nc->OnReadAfter((size_t)nread);
        while (nc->m_handler->OnData(*nc));
    }
}

void onConnections(uv_stream_t *server, int status)
{
    if (status != 0)
    {
        fprintf(stderr, "Connect error %s\n", uv_err_name(status));
        return;
    }

    NetLoop *ploop = (NetLoop *)server->loop->data;
    uint32_t session;
    uv_tcp_t *uvclient;
    if (ploop->freeslots.empty())
    {
        session = static_cast<uint32_t>(ploop->uvconns.size());
        uvclient = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
        ploop->uvconns.push_back(uvclient);
    }
    else
    {
        auto it = ploop->freeslots.begin();
        session = static_cast<uint32_t>(*it);
        assert(session < ploop->uvconns.size());
        uvclient = ploop->uvconns[session];
        ploop->freeslots.erase(it);
    }
    if (uv_tcp_init(server->loop, uvclient) != 0)
    {
        ploop->freeslots.insert(session);
        return;
    }

    if (uv_accept(server, (uv_stream_t *)uvclient) != 0)
        return;

    ++ploop->sessionSeq;
    if (ploop->sessionSeq == 0)
        ++ploop->sessionSeq;
    session |= session |= (ploop->sessionSeq << 20);

    SeverContext* sc = (SeverContext*)(server->data);
    auto* pktpool = &(ploop->pktPool);
    NetConn *pconn = ploop->connDataPool.Get(session, pktpool, sc->phandler);
    uvclient->data = pconn;
    pconn->m_handler->OnAccept(*pconn);
    

    if (uv_read_start((uv_stream_t *)uvclient, allocBuf, afterRead) != 0)
        return;
}

int32_t NetLoop::AddListener(const char *host, uint16_t port, NetHandler* pHandler)
{
    struct sockaddr_in addr;
    int32_t r = uv_ip4_addr(host, port, &addr);
    if (r != 0)
        return r;

    auto &svrContext = this->servers[port];
    if (svrContext.uvserver != nullptr)
        return -1;

    svrContext.phandler = pHandler;
    svrContext.uvserver = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    svrContext.uvserver->data = &svrContext;

    r = uv_tcp_init(this->uvloop, svrContext.uvserver);
    if (r != 0)
    {
        fprintf(stderr, "Socket creation error\n");
        return 1;
    }

    r = uv_tcp_bind(svrContext.uvserver, (const struct sockaddr *)&addr, 0);
    if (r != 0)
    {
        fprintf(stderr, "Bind error\n");
        return r;
    }

    r = uv_listen((uv_stream_t *)(svrContext.uvserver), SOMAXCONN, onConnections);

    if (r != 0)
    {
        fprintf(stderr, "Listen error %s\n", uv_err_name(r));
        return r;
    }

    return 0;
}

#define SessionIndex(sessionId)  (sessionId & 0xFFFFF)

int32_t NetLoop::Send(uint32_t sesionId, uint8_t* data, uint16_t len)
{
    uint32_t sindex = SessionIndex(sesionId);
    if(sindex < this->uvconns.size())
    {
        uv_tcp_t *uvconn = this->uvconns[sindex];

        NetConn *nc = reinterpret_cast<NetConn *>(uvconn->data);
        if (nc == nullptr || nc->sessionId != sesionId)
            return -1;
        uv_write_t *wrq = writeReqPool.Get();
        uv_buf_t uvbuf = uv_buf_init((char*)data, len);

        auto* ploop = this;
        WriteData* pwd = this->writeDataPool.Get(ploop, data);
        wrq->data = pwd;

        pwd->AddRef();
        uv_write(wrq, (uv_stream_t *)uvconn, &uvbuf, 1, writeCallBack);
        pwd->DecRef();
    }

    return -1;
}
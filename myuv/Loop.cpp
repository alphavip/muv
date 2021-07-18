#include <vector>
#include <iostream>
#include <map>
#include <memory.h>

#include "NetConn.h"
#include "PktItem.h"
#include "NetHandler.h"

#include "uv.h"
#include "Loop.h"

#define SessionIndex(sessionId) (sessionId & 0xFFFFF)

bool WriteData::DecRef()
{
    assert(ref > 0);
    return (--ref == 0);
}

void closeCallBack(uv_handle_t *handle)
{
    NetConn *pconn = (NetConn *)handle->data;
    if (pconn != nullptr)
    {
        NetLoop *ploop = (NetLoop *)handle->loop->data;
        ploop->freeslots.insert(SessionIndex(pconn->sessionId));
        ploop->Cycle(pconn);
    }
}

void closeFree(uv_handle_t *handle)
{
    free(handle);
}

NetLoop::NetLoop()
{
    this->uvloop = uv_loop_new();
    this->uvloop->data = this;

    this->uvconns.resize(InitTcpConns);
    for (uint32_t i = 0; i < this->uvconns.size(); ++i)
    {
        this->uvconns[i] = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
        freeslots.insert(i);
    }
}

NetLoop::~NetLoop()
{
    for(auto& pair : this->servers)
    {
        uv_close(reinterpret_cast<uv_handle_t *>(pair.second.uvserver), closeFree);
    }


    for(auto& index : this->freeslots)
    {
        free(this->uvconns[index]);
    }
    
    uv_close((uv_handle_t*)(curlTimer), closeFree);
    
    curl_multi_cleanup(this->multiHandler);

    uv_run(this->uvloop, UV_RUN_DEFAULT);
    uv_print_all_handles(this->uvloop, stderr);
    uv_loop_delete(this->uvloop);
}

void NetLoop::Start()
{
    uv_run(this->uvloop, UV_RUN_DEFAULT);
}

void NetLoop::Stop()
{
    uv_stop(this->uvloop);
}

size_t writeCallback( char *ptr, size_t size, size_t nmemb, void *userdata)  
{  
    CurlData& data = *(static_cast<CurlData*>(userdata));  
    size_t len = size * nmemb;  
    data.resData = (char*)realloc(data.resData, len + data.resSize);
    memcpy(data.resData+data.resSize, ptr, len);
    data.resSize += len;

    return len;  
}  

int32_t NetLoop::AddCurlReq(const char* url, const char* header, const char* post, CurlReqCB&& cb, void* userp)
{
    CURL* handle = curl_easy_init();
    CurlData* cec = this->curlDataPool.Get();
    cec->cb = cb;
    cec->userData = userp;

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_PRIVATE, cec);

    if(post != nullptr && strlen(post) > 0) 
    {
        cec->postData = strdup(post);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, cec->postData);
    }
    

    if(header != nullptr)
    {
        struct curl_slist *headers = curl_slist_append(headers, header);
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, cec);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 5000);
    
    curl_multi_add_handle(this->multiHandler, handle);

    return 0;
}
 
static void check_multi_info(NetLoop* ploop)
{
    CURLMsg *message;
    int pending;
    CURL *easy_handle;
    CurlData* curlData;
    
    while((message = curl_multi_info_read(ploop->multiHandler, &pending))) 
    {
        switch(message->msg) 
        {
        case CURLMSG_DONE:
            easy_handle = message->easy_handle;  
       
            curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &curlData);
            curl_multi_remove_handle(ploop->multiHandler, easy_handle);
            curl_easy_cleanup(easy_handle);
            if(curlData->cb)
                curlData->cb(message->data.result, *curlData);
            if(curlData->postData != nullptr)
                free(curlData->postData);
            if(curlData->resData != nullptr)
                free(curlData->resData);;
            ploop->curlDataPool.Cycle(curlData);

            break;    
        default:
            fprintf(stderr, "CURLMSG default\n");
            break;
        }
    }
}

static void closePollCB(uv_handle_t* handle)
{
    ((NetLoop*)handle->loop->data)->curlContextPool.Cycle((CurlContext*)handle->data);
    std::cout << "close poll" << std::endl;
}
 
static void curlPerform(uv_poll_t *req, int status, int events)
{
    NetLoop* ploop = (NetLoop *) req->loop->data;

    int running_handles;
    int flags = 0;
    CurlContext *context = (CurlContext*)req->data;
    
    if(events & UV_READABLE)
        flags |= CURL_CSELECT_IN;
    if(events & UV_WRITABLE)
        flags |= CURL_CSELECT_OUT;
    

    
    curl_multi_socket_action(ploop->multiHandler, context->sockfd, flags, &running_handles);
    
    check_multi_info(ploop);
}
 
static void curlUVTimeOut(uv_timer_t *req)
{
    NetLoop* ploop = (NetLoop*) req->loop->data;

    int running_handles;
    curl_multi_socket_action(ploop->multiHandler, CURL_SOCKET_TIMEOUT, 0, &running_handles);
    check_multi_info(ploop);
}
 
static int startTimeOut(CURLM *multi, long timeout_ms, void *userp)
{
    NetLoop* ploop = (NetLoop*)(userp);
    if(timeout_ms < 0)
    {
        uv_timer_stop(ploop->curlTimer);
        std::cout << "stop timer" << std::endl;
    }
    else 
    {
        if(timeout_ms == 0)
            timeout_ms = 1; /* 0 means directly call socket_action, but we'll do it in a bit */
        uv_timer_start(ploop->curlTimer, curlUVTimeOut, timeout_ms, 0);
        std::cout << "start timer" << std::endl;
    }
    return 0;
}
 

static int32_t handleSocket(CURL *easyhandler, curl_socket_t s, int action, void *userp, void *socketp)
{
    CurlContext *context;
    int events = 0;
    NetLoop* ploop = (NetLoop*)(userp);

    switch(action) 
    {
    case CURL_POLL_IN:
    case CURL_POLL_OUT:
    case CURL_POLL_INOUT:
        if(socketp == nullptr)
        {
            context = ploop->curlContextPool.Get();
            uv_poll_init_socket(ploop->uvloop, &context->poll_handle, s);
            context->poll_handle.data = context;
            std::cout << "poll start " << s << ":" << action << std::endl;
        }
        else
        {
            context = (CurlContext*)(socketp);
        }
    
        curl_multi_assign(ploop->multiHandler, s, (void *)context);
    
        if(action != CURL_POLL_IN)
            events |= UV_WRITABLE;
        if(action != CURL_POLL_OUT)
            events |= UV_READABLE;
        //开始polling文件描述符，一旦检测到读写事件，则调用curlPerform函数，参数status设置为0
        uv_poll_start(&context->poll_handle, events, curlPerform);

        break;
    case CURL_POLL_REMOVE:
        if(socketp) 
        {
            context = (CurlContext*)(socketp);
            uv_poll_stop(&(context->poll_handle));
            uv_close((uv_handle_t*)&context->poll_handle, closePollCB);
            curl_multi_assign(ploop->multiHandler, s, NULL);
        }
        break;
    default:
        abort();
    }
 
    return 0;
}


int32_t NetLoop::InitMulitCulr()
{
    if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Could not init curl\n");
        return -1;
    }
    
    this->curlTimer = (uv_timer_t*)malloc(sizeof(uv_timer_t));
    uv_timer_init(this->uvloop, this->curlTimer);
    
    this->multiHandler = curl_multi_init();
    //调用handleSocket回调函数，传入新建的sockfd，根据传入的action状态添加到相应的事件管理器，如封装epoll的libev或libevent。
    curl_multi_setopt(this->multiHandler, CURLMOPT_SOCKETFUNCTION, handleSocket);
    //当使用curl_multi_add_handle(g->multi, conn->easy)添加请求时会回调startTimeOut，然后调用
	//curl_multi_socket_action(curl_handle, CURL_SOCKET_TIMEOUT, 0, &running_handles)初始化请求并得到一个socket(fd)
    curl_multi_setopt(this->multiHandler, CURLMOPT_TIMERFUNCTION, startTimeOut);

    curl_multi_setopt(this->multiHandler, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(this->multiHandler, CURLMOPT_TIMERDATA, this);

    return 0;
}

void allocBuf(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    NetConn *pconn = (NetConn *)handle->data;
    PktItem *pkt = pconn->OnReadAlloc();

    buf->base = (char *)pkt->GetWriteAddr();
    buf->len = pkt->GetCanWriteCount();
}

void writeCallBack(uv_write_t *req, int status)
{
    WriteData *pwd = (WriteData *)req->data;
    auto *ploop = (NetLoop*)req->handle->loop->data;
    NetConn *pconn = (NetConn *)req->handle->data;
    ploop->writeReqPool.Cycle(req);

    assert(pconn != nullptr);

    if(pwd->DecRef())
    {
        if(pwd->userdata != nullptr)
            pconn->m_handler->OnWrited(pwd->userdata);
        ploop->writeDataPool.Cycle(pwd);
    }
}


void afterRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    NetConn *nc = reinterpret_cast<NetConn *>(stream->data);
    if (nread < 0)
    {
        if (nc != nullptr)
        {
            nc->m_handler->OnClose(nc->sessionId, nread);
        }

        uv_close((uv_handle_t *)stream, closeCallBack);
        return;
    }
    if (nc != nullptr)
    {
        //从开头开始写入的,还没加
        assert(buf->base == (char *)nc->m_currReadPkt->GetWriteAddr());
        nc->OnReadAfter((size_t)nread);
        while (nc->m_handler->OnData(*nc))
            ;
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
    {
        ploop->freeslots.insert(session);
        return;
    }
    ++ploop->sessionSeq;
    if (ploop->sessionSeq == 0)
        ++ploop->sessionSeq;
    session |= (ploop->sessionSeq << 20);

    SeverContext *sc = (SeverContext *)(server->data);
    auto *pktpool = &(ploop->pktPool);
    NetConn *pconn = ploop->connDataPool.Get(session, pktpool, sc->phandler);
    uvclient->data = pconn;

    if (uv_read_start((uv_stream_t *)uvclient, allocBuf, afterRead) != 0)
    {
        uv_close((uv_handle_t *)uvclient, closeCallBack);
        return;
    }

    pconn->m_handler->OnAccept(*pconn);
}

int32_t NetLoop::AddListener(const char *host, uint16_t port, NetHandler *pHandler)
{
    struct sockaddr_in addr;
    int32_t r = uv_ip4_addr(host, port, &addr);
    if (r != 0)
        return r;

    auto it = this->servers.find(port);
    if (it != this->servers.end())
    {
        auto &ll = it->second;
        if (ll.uvserver != nullptr)
            uv_close(reinterpret_cast<uv_handle_t *>(ll.uvserver), closeFree);
        this->servers.erase(it);
    }

    auto &svrContext = this->servers[port];
    svrContext.phandler = pHandler;
    svrContext.uvserver = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
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

void onconnect(uv_connect_t *req, int status)
{
    uv_tcp_t *uvclient = (uv_tcp_t *)(req->handle);
    NetHandler *phandler = (NetHandler *)(req->data);
    auto *ploop = (NetLoop *)uvclient->loop->data;
    uint32_t session = (uint64_t)(uvclient->data);
    if (status != 0)
    {
        uvclient->data = nullptr;
        phandler->OnClose(session, status);
        ploop->freeslots.insert(SessionIndex(session));
        return;
    }

    auto *pktpool = &(ploop->pktPool);
    uvclient->data = ploop->connDataPool.Get(session, pktpool, phandler);

    if (uv_read_start((uv_stream_t *)uvclient, allocBuf, afterRead) != 0)
    {
        uv_close((uv_handle_t *)uvclient, closeCallBack);
        return;
    }

    phandler->OnConnect(session);
}

int32_t NetLoop::Connect(const char *host, uint16_t port, NetHandler *phandler)
{
    uint32_t session;
    uv_tcp_t *uvclient;
    if (this->freeslots.empty())
    {
        session = static_cast<uint32_t>(this->uvconns.size());
        uvclient = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
        this->uvconns.push_back(uvclient);
    }
    else
    {
        auto it = this->freeslots.begin();
        session = static_cast<uint32_t>(*it);
        assert(session < this->uvconns.size());
        uvclient = this->uvconns[session];
        this->freeslots.erase(it);
    }
    uv_tcp_init(this->uvloop, uvclient);

    uv_connect_t *connect = (uv_connect_t *)malloc(sizeof(uv_connect_t));

    struct sockaddr_in dest;
    uv_ip4_addr(host, port, &dest);
    connect->data = phandler;
    uvclient->data = (void *)(uint64_t)session;
    uv_tcp_connect(connect, uvclient, (const struct sockaddr *)&dest, onconnect);

    return 0;
}

void ontimer(uv_timer_t *handle)
{
    TimerData* td = (TimerData*)(handle->data);
    td->calback(td->data);
    //对于不repeat也由外部remove吧，底层不管理
}



int32_t NetLoop::AddTimer(std::function<void(void *data)> &callback, uint64_t firstinterval, uint64_t repeat, void *data)
{
    ++this->timerIndex;
    uv_timer_t* uvtimer = (uv_timer_t*)malloc(sizeof(uv_timer_t));
    uv_timer_init(this->uvloop, uvtimer);
 
    uv_timer_start(uvtimer, ontimer, firstinterval, repeat);
    this->uvtimers[++this->timerIndex] = uvtimer;
    auto* td = this->timerDataPool.Get(data, callback);

    uvtimer->data = td;

    return this->timerIndex;
}

void NetLoop::RemoveTimer(uint32_t timerId)
{
    auto it = this->uvtimers.find(timerId);
    if(it != this->uvtimers.end())
    {
        uv_timer_stop(it->second);
        TimerData* td = (TimerData*)(it->second->data);
        this->timerDataPool.Cycle(td);
    }
}

void NetLoop::Send(uint32_t sesionId, uint8_t *data, uint16_t len, void* userData)
{
    if(userData == nullptr)
        userData = data;

    auto *ploop = this;
    WriteData *pwd = this->writeDataPool.Get(ploop, userData);

    do 
    {    uint32_t sindex = SessionIndex(sesionId);
        if (sindex < this->uvconns.size())
        {
            uv_tcp_t *uvconn = this->uvconns[sindex];

            NetConn *nc = reinterpret_cast<NetConn *>(uvconn->data);
            if (nc == nullptr || nc->sessionId != sesionId)
            {
                break;
            }
            uv_write_t *wrq = writeReqPool.Get();
            uv_buf_t uvbuf = uv_buf_init((char *)data, len);

            wrq->data = pwd;

            pwd->AddRef();
            uv_write(wrq, (uv_stream_t *)uvconn, &uvbuf, 1, writeCallBack);
        }
    }while (false);
    
    pwd->DecRef();
    return;
}

void NetLoop::CloseConn(uint32_t sessionId)
{
    uint32_t sindex = SessionIndex(sessionId);
    if (sindex < this->uvconns.size())
    {
        uv_tcp_t *uvconn = this->uvconns[sindex];

        NetConn *nc = reinterpret_cast<NetConn *>(uvconn->data);
        if (nc == nullptr || nc->sessionId != sessionId)
            return;
        uv_read_stop((uv_stream_t*)uvconn);
        uv_close((uv_handle_t *)uvconn, closeCallBack);
    }
}

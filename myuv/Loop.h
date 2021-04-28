#ifndef _LOOP_H_ 
#define _LOOP_H_

#include <stdint.h>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>
#include <functional>


#include "uv.h"
#include "NetConn.h"
#include "PktItem.h"

struct NetConn;
class NetLoop;
struct NetHandler;


//fot write req call back data
struct WriteData
{
    NetLoop *ploop = nullptr;
    void *userdata = nullptr;
    uint32_t ref = 1;

    WriteData(NetLoop *l, void *data) : ploop(l), userdata(data) {}

    inline void AddRef() { ++ref; }
    inline bool DecRef();
};

struct TimerData
{
    void *data = nullptr;
    std::function<void(void *data)> calback;
    TimerData(void *d, std::function<void(void *data)>&f) : data(d), calback(f){}
};


//for listener user data
struct SeverContext
{
    uv_tcp_t *uvserver;
    uint16_t port;
    NetHandler* phandler;

    inline void Init()
    {
        uvserver = nullptr;
        port = 0;
        
    }
};

#define InitTcpConns 1024

class NetLoop
{
public:
    NetLoop();
    ~NetLoop();

public:
    int32_t AddListener(const char* host, uint16_t port, NetHandler* phandler);
    int32_t Connect(const char *host, uint16_t port, NetHandler *phandler);

public:
    //return timerId 0:error 
    int32_t AddTimer(std::function<void(void* data)>& callback, uint64_t firstinterval, uint64_t repeat, void* data = nullptr);
    void RemoveTimer(uint32_t timerId);


public:
    void Start();
    void Stop();

public:
    uv_loop_t* GetLoop() { return this->uvloop; }

public:
    void Send(uint32_t sessionId, uint8_t* buff, uint16_t len, void* userData = nullptr);
    void CloseConn(uint32_t sessionId);

public:
    void Cycle(NetConn* conn) { this->connDataPool.Cycle(conn); }
    //void SetConnDataPool(ConnPool& pool) { this->connDataPool = &pool; }


public:
    uv_loop_t *uvloop = nullptr;
    //listeners
    std::unordered_map<uint16_t, SeverContext> servers;

    //连接对象池
    ConnPool connDataPool;
    //read buffer对象池
    PktItemPool pktPool;

    //写请求对象池
    MemPoolC<uv_write_t, 1024*4> writeReqPool;
    //写对象池,存放了写请求的一些userdata
    MemPool<WriteData, 1024> writeDataPool;

    //连接对象 session
    uint32_t sessionSeq = 0;
    std::vector<uv_tcp_t*> uvconns;
    std::set<uint32_t> freeslots;

    //timer相关
    uint32_t timerIndex = 0;
    std::unordered_map<uint32_t, uv_timer_t *> uvtimers; //先用hash表存吧，是不是像tcp那样管理呢
    MemPool<TimerData, 256> timerDataPool;
};


#endif	// _LOOP_H_
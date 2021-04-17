#ifndef _LOOP_H_ 
#define _LOOP_H_

#include <stdint.h>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>


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
    uint8_t *buff = nullptr;
    uint32_t ref = 1;

    WriteData(NetLoop* l, uint8_t* data) : ploop(l), buff(data){}

    inline void AddRef() { ++ref; }
    inline void DecRef();
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
    NetLoop(){}
    ~NetLoop(){}

public:
    int32_t AddListener(const char* host, uint16_t port, NetHandler* phandler);

public:
    bool Init();
    void UnInit();

public:
    void Start();
    void Stop();

public:
    uv_loop_t* GetLoop() { return this->uvloop; }

public:
    int32_t Send(uint32_t sessionId, uint8_t* data, uint16_t len);


public:
    void Cycle(NetConn* conn) { this->connDataPool.Cycle(conn); }
    //void SetConnDataPool(ConnPool& pool) { this->connDataPool = &pool; }


public:
    uv_loop_t *uvloop = nullptr;
    std::unordered_map<uint16_t, SeverContext> servers;

    ConnPool connDataPool;
    PktItemPool pktPool;

    MemPoolC<uv_write_t, 1024*4> writeReqPool;
    MemPool<WriteData, 1024> writeDataPool;

    uint32_t sessionSeq = 0;
    std::vector<uv_tcp_t*> uvconns;
    std::set<uint32_t> freeslots;
};


#endif	// _LOOP_H_
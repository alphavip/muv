#include <iostream>
#include <memory.h>

#include "Loop.h"
#include "NetHandler.h"

NetLoop loop;

class TestHandler : public NetHandler
{
public:
    TestHandler(){}
    virtual ~TestHandler(){}

public:
    virtual bool OnAccept(NetConn &conn) { std::cout << "new client:" << conn.sessionId << std::endl; return true; }
    virtual bool OnData(NetConn &conn) { 
        uint8_t* buf = new uint8_t[13];
        memcpy(buf, "Hello World!\n", 13);
        loop.Send(conn.sessionId, buf, 13);
        return false;
    }
    virtual void OnClose(NetConn &conn, long error) {
        std::cout << "client close:" << conn.sessionId << "->" << error << std::endl;
    }
};

int main()
{

    loop.Init();
    loop.AddListener("127.0.0.1", 8888, new TestHandler());
    loop.Start();
}
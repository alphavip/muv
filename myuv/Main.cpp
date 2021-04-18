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
    void OnAccept(NetConn &conn) override { std::cout << "new client:" << conn.sessionId << std::endl; }
    bool OnData(NetConn &conn) override { 
        uint8_t* buf = new uint8_t[13];
        memcpy(buf, "Hello World!\n", 13);
        loop.Send(conn.sessionId, buf, 13);
        return false;
    }
    void OnClose(uint32_t sessionId, int error) override {
        std::cout << "client close:" << sessionId << "->" << error << std::endl;
    }
};

int main()
{

    loop.Init();
    loop.AddListener("127.0.0.1", 8888, new TestHandler());
    loop.Start();
}
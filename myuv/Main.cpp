#include <iostream>
#include <memory.h>

#include "Loop.h"
#include "NetConn.h"
#include "NetHandler.h"



class TestHandler : public NetHandler
{
public:
    TestHandler(){}
    virtual ~TestHandler(){}

public:
    void OnAccept(NetConn &conn) override { std::cout << "new client:" << conn.sessionId << std::endl; }
    bool OnData(NetConn &conn) override {
        const static char* strWelCome = "Hello World!\n";
        uint32_t num = strlen(strWelCome) + conn.GetReadLen();
        uint8_t *buf = new uint8_t[num];
        memcpy(buf, strWelCome, strlen(strWelCome));
        conn.CopyAndDrain(conn.GetReadLen(), buf + strlen(strWelCome));
        loop.Send(conn.sessionId, buf, num);

        return false;
    }
    void OnClose(uint32_t sessionId, int error) override {
        std::cout << "client close:" << sessionId << "->" << error << std::endl;
    }

    void OnWrited(void* data) {
        delete (uint8_t*)data;
    }
};

int main()
{
    NetLoop loop;
    loop.AddListener("0.0.0.0", 8888, new TestHandler());

    

    loop.Start();
    //loop.Stop();

    return 0;
}

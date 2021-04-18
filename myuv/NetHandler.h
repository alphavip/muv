#ifndef _NETHANDLER_H_ 
#define _NETHANDLER_H_ 

struct NetConn;

//for net event calback
class NetHandler
{
public:
    NetHandler() {}
    virtual ~NetHandler() {}
    virtual void OnConnect(uint32_t sessionId){}
    virtual void OnAccept(NetConn &conn) {  }
    //这个版本先把conn裸漏出去吧
    virtual bool OnData(NetConn& conn) { return false; }
    virtual void OnBuffRelease(uint8_t* data) {}
    virtual void OnClose(uint32_t sessionid, int error) {}
};

#endif // _NETHANDLER_H_
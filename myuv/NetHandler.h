#ifndef _NETHANDLER_H_ 
#define _NETHANDLER_H_ 

struct NetConn;

//for net event calback
class NetHandler
{
public:
    NetHandler() {}
    virtual ~NetHandler() {}
    virtual bool OnAccept(NetConn &conn) { return true; }
    //这个版本先把conn裸漏出去吧
    virtual bool OnData(NetConn& conn) { return false; }
    virtual void OnBuffRelease(uint8_t* data) {}
    virtual void OnClose(NetConn &conn, long error) {}
};

#endif // _NETHANDLER_H_
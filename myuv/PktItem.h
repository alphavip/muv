#ifndef __PKT_ITEM_H__
#define __PKT_ITEM_H__

#include "MemPool.h"
#include <string>

#define PKT_ITEM_SIZE 4076

//不能
struct PktItem
{
public:
    // 获取读取地址
    uint8_t* GetReadAddr() {
        assert(read < PKT_ITEM_SIZE);
        return &(data[read]);
    }

    // 获取可读数量
    uint16_t GetCanReadCount()const {
        assert(write >= read);
        return write - read;
    }

    // 获取写取地址
    uint8_t* GetWriteAddr() {
        assert(write <= PKT_ITEM_SIZE);
        return &data[write];
    }

    // 获取可写数量
    uint16_t GetCanWriteCount()const {
        assert(write <= PKT_ITEM_SIZE);
        return PKT_ITEM_SIZE - write;
    }


public:
    PktItem* nextPkt = nullptr;          // 下一个节点

private:
    uint8_t data[PKT_ITEM_SIZE];        // 数据
    uint16_t read = 0;                  // 可读点
    uint16_t write = 0;                 // 可写点
};

//增加了阅读代码难度，没啥用
#define walkpktbegin(ptmp, pkt) \
    for (auto *ptmp = pkt; ptmp != nullptr; ptmp = ptmp->nextPkt) \
    {
#define walkpktend() \
    }

// uint32_t pkt_count(PktItem *pkt)
// {
//     uint32_t count = 0;
//     for (auto *ptmp = pkt; ptmp != nullptr; ptmp = ptmp->nextPkt)
//         ++count;
//     return count;
// }

// uint32_t pkt_len(PktItem *pkt)
// {
//     uint32_t len = 0;
//     for (auto *ptmp = pkt; ptmp != nullptr; ptmp = ptmp->nextPkt)
//         len += ptmp->GetCanReadCount();
//     return len;
// }

typedef MemPool<PktItem, 1024> PktItemPool;

#endif

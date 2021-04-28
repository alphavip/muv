#ifndef _SYS_MEMPOOL_H_
#define _SYS_MEMPOOL_H_

#include <cassert>
#include <stdint.h>
#include <stddef.h>
#include <array>
#include <mutex>
#include <set>


template<typename T, int MF=128>
class MemPoolC 
{
public:
    inline MemPoolC() { objs_.fill(nullptr); }
    virtual ~MemPoolC()
    {
        for(auto *&p: objs_) 
        {
            if(p != nullptr) 
            {
                Free(p);
                p = nullptr;
            }
        }
    }
    inline T *Get(size_t sz = sizeof(T))
    {
        if(count_ == 0) 
        {
            return Alloc(sz);
        } 
        else
        {
            -- count_;
            T *r = objs_[count_];
            objs_[count_] = nullptr;
            return r;
        }
    }
    inline void Cycle(T *obj) 
    {
        if(count_ < MF) {
            objs_[count_ ++] = obj;
        } else {
            Free(obj);
        }
    }

public:
    static inline T *Alloc(size_t sz)
    {
        return static_cast<T*>(malloc(sz));
    }
    static inline void Free(T *obj)
    {
        free(obj);
    }

private:
    int count_ = 0;
    std::array<T*, MF> objs_;
};



template<typename T, int MF=128>
class MemPoolCSafe: public MemPoolC<T, MF>
{
public:
    //using MemPoolC<T, MF>::MemPoolC;
    template<typename... TR>
    inline T *Get(size_t sz) 
    {
        std::unique_lock<std::mutex> lk(mutex_);
        return MemPoolC<T, MF>::Get(sz);
    }
    inline void Cycle(T *obj) 
    {
        std::unique_lock<std::mutex> lk(mutex_);
        MemPoolC<T, MF>::Cycle(obj);
    }
private:
    std::mutex mutex_;
};

template <typename T, int MF = 128>
class MemPool
{
public:
    inline MemPool() { objs_.fill(nullptr); }
    virtual ~MemPool()
    {
        for (auto *&p : objs_)
        {
            if (p != nullptr)
            {
                Free(p);
                p = nullptr;
            }
        }
    }
    template <typename... TR>
    inline T *Get(TR &... params)
    {
        if (count_ == 0)
        {
            return Alloc(params...);
        }
        else
        {
            --count_;
            T *r = new (objs_[count_]) T(params...);
            objs_[count_] = nullptr;
            return r;
        }
    }
    inline void Cycle(T *obj)
    {
        if (count_ < MF)
        {
            obj->~T();
            objs_[count_++] = obj;
        }
        else
        {
            Free(obj);
        }
    }
    template <typename... TR>
    static inline T *Alloc(TR &... params)
    {
        char* p = new char[sizeof(T) + sizeof(int)];
        return new (p) T(params...);
    }
    static inline void Free(T *obj)
    {
        char* p = (char*)obj;
        delete p;
    }

private:
    int count_ = 0;
    std::array<T *, MF> objs_;
};


#endif // _SYS_MEMPOOL_H_

#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    extern __thread int t_cachedTid;    //每个线程独立拥有 t_cachedTid 的副本，互不干扰。

    void cacheTid();    //获取当前线程的 TID 并缓存到 t_cachedTid 中。

    inline int tid()
    {
        if(__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}
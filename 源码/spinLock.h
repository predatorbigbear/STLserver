#pragma once


#include <atomic>

//原子自旋锁，只适合在极短时间内完成操作的场景下使用
//长时间操作请使用std::mutex


//20250706  解决自旋锁在高竞争下性能下降查到网上有更好的解决思路
//
//另外我思考过，决定做一些http字段解析完善之后再进行qps测试
class SpinLock 
{
public:

    SpinLock() = default;
    
    SpinLock(const SpinLock&) = delete;
    
    SpinLock& operator=(const SpinLock&) = delete;

    void lock() 
    {
        while (flag.test_and_set(std::memory_order_acquire))
        {
#ifdef __x86_64__
            __builtin_ia32_pause();
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#endif
        }
    }

    void unlock() 
    {
        flag.clear(std::memory_order_release);
    }

    ~SpinLock() = default;

private:
    std::atomic_flag flag{ ATOMIC_FLAG_INIT };
};

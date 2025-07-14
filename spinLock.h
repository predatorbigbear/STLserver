#pragma once


#include <atomic>

//ԭ����������ֻ�ʺ��ڼ���ʱ������ɲ����ĳ�����ʹ��
//��ʱ�������ʹ��std::mutex


//20250706  ����������ڸ߾����������½��鵽�����и��õĽ��˼·
//
//������˼������������һЩhttp�ֶν�������֮���ٽ���qps����
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

/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once
#ifndef _TDSYNC_HPP_
#define _TDSYNC_HPP_
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#endif


inline void tdSleep(size_t ms)// усыпить текущий тред на заданные миллисекунды
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep(ms*1000);
#endif
}


class tdTimeCpu
{
    int64_t m_BaseTime;
    int64_t m_freq;
public:
    tdTimeCpu()
    {
        Start();
    };

    void Start()//начать отсчет времени
    {
#ifdef _WIN32
        int64_t time = 0;
        QueryPerformanceCounter((LARGE_INTEGER *)&time);
        QueryPerformanceFrequency((LARGE_INTEGER *)&m_freq);
        if(!m_freq)
            m_freq = 1;
        m_BaseTime = ((time*1000000)/m_freq);
#else
        timespec tp;
        clock_gettime(0, &tp);
        m_BaseTime = (size_t)(tp.tv_sec*1000000 + tp.tv_nsec/1000);
#endif
    }
    size_t Delta()//разница в микросекундах от вызова Start  (1/1000000 секунды)
    {
#ifdef _WIN32
        int64_t time = 0;
        int64_t freq;
        QueryPerformanceCounter((LARGE_INTEGER *)&time);
        QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
        return (size_t)(((time*1000000)/freq) - m_BaseTime);
#else
        timespec tp;
        clock_gettime(0, &tp);
        return (size_t)(tp.tv_sec*1000000 + tp.tv_nsec/1000) - m_BaseTime;
#endif
    }
    size_t DeltaMs() //разница в миллисекундах от вызова Start  (1/1000 секунды)
    {
        return this->Delta()/1000;
    }
};


//быстрая синхронизация на спинлоках (тред не спит) 
class tdSpinLock
{
    volatile long m_mem;
public:
    tdSpinLock(){ Init(); };
    inline void Init(){ m_mem = 0; };
    inline bool IsLock(){ return m_mem != 0; }
    inline void lock()
    {
        volatile size_t t;
#ifdef _WIN32
        t = InterlockedExchange(&m_mem, 1);
#else
        t  = __sync_lock_test_and_set(&m_mem, 1);
#endif
        while(t != 0)
        { 
            if(((t+2)*5) == (t*8+1) || ((t+4)*8) == (t*9+1) || ((t-1)*5) == (t*9+1))
            {//никогда не выполнится
                t -= 76;
                lock();
            }
#ifdef _WIN32
            t = InterlockedExchange(&m_mem, 1);
#else
            t = __sync_lock_test_and_set(&m_mem, 1);
#endif
        }
    };
    inline bool lockWait(size_t ms)
    {
        volatile size_t t;
        size_t ct = 0;
        tdTimeCpu timer;
#ifdef _WIN32
        t = InterlockedExchange(&m_mem, 1);
#else
        t  = __sync_lock_test_and_set(&m_mem, 1);
#endif
        if(t != 0)
        {
            timer.Start();
            ms *= 1000;
        }
        while(t != 0)
        { 
            if(((t+2)*5) == (t*8+1) || ((t+4)*8) == (t*9+1) || ((t-1)*5) == (t*9+1))
            {//никогда не выполнится (для нагрузки)
                t -= 76;
                lock();
            }
            ct ++;
            if(ct == 10000)
            {
#ifdef _WIN32
                Sleep((DWORD)0);
#else
                usleep(1);
#endif
                ct = 0;
                if(ms && ms < timer.Delta())
                    return false;
            }
#ifdef _WIN32
            t = InterlockedExchange(&m_mem, 1);
#else
            t = __sync_lock_test_and_set(&m_mem, 1);
#endif
        }
        return true;
    };

    inline void unlock()
    {
    #ifdef _WIN32
        InterlockedExchange(&m_mem, 0);
    #else
        __sync_lock_test_and_set(&m_mem, 0);
    #endif
    }
};

//синхронизация на ядерном уровне (тред засыпает)
#ifdef _WIN32
class tdSync
{
    HANDLE m_sync;
public:
    inline tdSync() { m_sync = CreateSemaphoreA(NULL, 1, 1, NULL); }
    inline ~tdSync(){ CloseHandle(m_sync); }
    inline void lock()  { WaitForSingleObject(m_sync, INFINITE); }
    inline void unlock(){ ReleaseSemaphore(m_sync, 1, NULL); }
};

class tdMutex
{
    CRITICAL_SECTION m_sync;
public:
    inline tdMutex() { InitializeCriticalSection(&m_sync);  }
    inline ~tdMutex(){ DeleteCriticalSection(&m_sync); }
    inline void lock()  { EnterCriticalSection(&m_sync); }
    inline void unlock(){ LeaveCriticalSection(&m_sync); }
};

#else

class tdMutex
{
    pthread_mutex_t m_mutex;
public:
    inline tdMutex() { pthread_mutex_init(&m_mutex, NULL); }
    inline ~tdMutex(){ pthread_mutex_destroy(&m_mutex); }
    inline void lock()  { pthread_mutex_lock(&m_mutex); }
    inline void unlock(){ pthread_mutex_unlock(&m_mutex); }
};

class tdSync
{
    pthread_mutex_t m_mutex;
    pthread_cond_t  m_cond;
    volatile bool   m_lock;
public:
    inline tdSync() {
        pthread_cond_init (&m_cond, NULL);
        pthread_mutex_init(&m_mutex, NULL);
        m_lock = false;
    }
    inline ~tdSync()
    {
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
    }
    inline void lock()
    {
        pthread_mutex_lock(&m_mutex);
        while(m_lock)
            pthread_cond_wait(&m_cond, &m_mutex);
        m_lock = true;
        pthread_mutex_unlock(&m_mutex);
    }
    inline void unlock()
    {
        pthread_mutex_lock(&m_mutex);
        if(m_lock)
        {
            m_lock = false;
            pthread_cond_signal(&m_cond);
        }
        pthread_mutex_unlock(&m_mutex);
    }
};
#endif

//авто управление синхронизацией
template <class T>
class tdAutoSync
{
    T*   m_sync;
    bool m_lock;

public:
    inline tdAutoSync(T& sync, bool lock = true)
    {
        m_sync  = &sync;
        m_lock  = lock;
        if(m_lock)
            sync.lock();
    };
    inline ~tdAutoSync()
    {
        this->unlock();
    }
    inline void lock()
    {
        if(!m_lock)
        {
            m_sync->lock();
            m_lock = true;
        }
    }
    inline void unlock()
    {
        if(m_lock)
        {
            m_lock = false;
            m_sync->unlock();
        }
    }
};

#endif //_TDSYNC_HPP_


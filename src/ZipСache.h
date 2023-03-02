/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once
#include <tdSync.hpp>

class tdZipReader;

class ZipCache
{
    tdZipReader* m_zip;
    tdSpinLock   m_lock;

    struct CacheData
    {
        size_t m_fileID;
        size_t m_timeFree;
        size_t m_countUse;
        size_t m_size;
        void*  m_data;
    };
    CacheData* m_cache;
    size_t     m_cache_size;
    size_t     m_numQ;
public:
    ZipCache();
    ~ZipCache();
    bool Init(tdZipReader* zip);
    bool Load(size_t fileID, const char*& outData, size_t& outSize);
    bool Free(size_t fileID);
};


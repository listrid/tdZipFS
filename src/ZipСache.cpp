/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#include "Zip—ache.h"
#include <tdZip.h>


ZipCache::ZipCache()
{
    m_lock.Init();
    m_cache_size = 8;
    m_cache = (CacheData*)malloc(sizeof(CacheData)*m_cache_size);
    memset(m_cache, 0, sizeof(CacheData)*m_cache_size);
}


ZipCache::~ZipCache()
{
    for(size_t i = 0; i < m_cache_size; i++)
    {
        if(m_cache[i].m_data != NULL)
            free(m_cache[i].m_data);
    }
    free(m_cache);
}


bool ZipCache::Init(tdZipReader* zip)
{
    m_zip = zip;
    for(size_t i = 0; i < m_cache_size; i++)
    {
        if(m_cache[i].m_data != NULL)
            free(m_cache[i].m_data);
    }
    memset(m_cache, 0, sizeof(CacheData)*m_cache_size);
    return true;
}


bool ZipCache::Load(size_t fileID, const char*& outData, size_t& outSize)
{
    tdAutoSync<tdSpinLock> lock(m_lock);
    size_t numFree0 = ~0;
    size_t numFree1 = ~0;
    size_t numTimeF = ~0;
    m_numQ++;

    for(size_t i = 0; i < m_cache_size; i++)
    {
        if(m_cache[i].m_data == NULL)
        {
            numFree0 = i;
            continue;
        }
        if(m_cache[i].m_fileID == fileID)
        {
            m_cache[i].m_countUse++;
            outData = (char*)m_cache[i].m_data;
            outSize = m_cache[i].m_size;
            return true;
        }
        if(m_cache[i].m_countUse == 0)
        {
            if(numTimeF > m_cache[i].m_timeFree)
            {
                numFree1 = i;
                numTimeF = m_cache[i].m_timeFree;
            }
        }
    }
    if(numFree0 == ~0)
    {
        if(numFree1 != ~0)
        {
            free(m_cache[numFree1].m_data);
            m_cache[numFree1].m_data = NULL;
            numFree0 = numFree1;
        }else{
            size_t new_cache_size = m_cache_size + 8;
            CacheData* new_cache = (CacheData*)malloc(sizeof(CacheData)*new_cache_size);

            memcpy(new_cache, m_cache, sizeof(CacheData)*m_cache_size);
            memset(new_cache+m_cache_size, 0, sizeof(CacheData)*8);
            free(m_cache);
            m_cache  = new_cache;
            numFree0 = m_cache_size;
            m_cache_size = new_cache_size;
        }
    }
    m_cache[numFree0].m_countUse = 0;
    int size = m_zip->GetFileSize(fileID);
    if(size == -1)
        return false;
    m_cache[numFree0].m_data = malloc(size);
    if(m_cache[numFree0].m_data == NULL)
        return false;
    m_cache[numFree0].m_fileID   = fileID;
    m_cache[numFree0].m_size = size;
    if(!m_zip->ReadFile(fileID, m_cache[numFree0].m_data))
        return false;
    m_cache[numFree0].m_countUse = 1;

    outData = (char*)m_cache[numFree0].m_data;
    outSize = m_cache[numFree0].m_size;
    return true;
}


bool ZipCache::Free(size_t fileID)
{
    tdAutoSync<tdSpinLock> lock(m_lock);
    m_numQ++;

    for(size_t i = 0; i < m_cache_size; i++)
    {
        if(m_cache[i].m_fileID == fileID)
        {
            if(m_cache[i].m_countUse)
            {
                m_cache[i].m_countUse--;
                if(!m_cache[i].m_countUse)
                    m_cache[i].m_timeFree = m_numQ;
                return true;
            }
            return false;
        }
    }
    return false;
}


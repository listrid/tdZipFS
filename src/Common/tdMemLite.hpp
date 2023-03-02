/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once

#include <malloc.h>
#include <string.h>

class tdMemLite
{
private:
    void **m_chunk_bottom, **m_chunk_top;
    void **m_list_big;  //список выделенных больших блоков
    void **m_list_free; //список освобожденных кусков

    size_t m_chunk_size;
    size_t m_free_space;

    size_t m_chunk_use;
    size_t m_chunk_count;
public:
    tdMemLite(size_t chunk_size = 0)
    {
        m_chunk_bottom = m_list_big = NULL;
        Init(chunk_size);
    }
    ~tdMemLite(){ FreeStorage(); }
    void Init(size_t chunk_size)
    {
        FreeStorage();
        if(!chunk_size)
            chunk_size = ((1<<16) - sizeof(void*));//64kb
        m_chunk_size = (chunk_size + sizeof(void*) + 15)&(~15);
    }
    void Clear()//вернуть себе всю выданную память
    {
        m_chunk_use  = 0;
        m_chunk_top  = m_chunk_bottom;
        m_free_space = (m_chunk_bottom != NULL) ? m_chunk_size - sizeof(void*) : 0;
        while(m_list_big)
        {
            void** temp = m_list_big;
            m_list_big = (void**)(m_list_big[0]);
            free((void*)temp);
        }
        m_list_free = NULL;
    }
    void FreeStorage()//освободить всю память системе
    {
        while(m_chunk_bottom)
        {
            void** temp = m_chunk_bottom;
            m_chunk_bottom = (void**)(m_chunk_bottom[0]);
            free((void*)temp);
        }
        while(m_list_big)
        {
            void** temp = m_list_big;
            m_list_big = (void**)(m_list_big[0]);
            free((void*)temp);
        }
        m_list_free = m_chunk_top = m_chunk_bottom = NULL, m_free_space = m_chunk_use = m_chunk_count = 0;
    }
    void* Alloc(size_t size) //выделить память 
    {
        if(size < sizeof(void*)*2)
            size = sizeof(void*)*2;
        size = (size+7)&(~7);
        if(m_list_free)
        {//поиск в свободных
            void** prev = NULL;
            void** next = m_list_free;
            while(next)
            {
                if(next[1] == (void**)size)
                {
                    if(prev){ prev[0] = next[0]; }else{ m_list_free = (void**)next[0]; }
                    return next;
                }
                prev = next;
                next = (void**)(next[0]);
            }
        }
        if(size > (m_chunk_size - sizeof(void*)))
        {//выделить большой блок
            void** block = (void**)malloc(size + sizeof(void*));
            block[0] = m_list_big;
            m_list_big = block;
            return &block[1];
        }
        if(m_free_space < size)
        {
            if(m_free_space)
                this->Free(((char*)m_chunk_top) + (m_chunk_size - m_free_space), m_free_space);
            if(m_chunk_top == NULL || m_chunk_top[0] == NULL)
            {
                void **block = (void**)malloc(m_chunk_size);
                block[0] = NULL;
                if(m_chunk_top)
                    m_chunk_top[0] = block;
                else
                    m_chunk_top = m_chunk_bottom = block;
                m_chunk_count++;
            }
            if(m_chunk_top[0])
                m_chunk_top = (void**)(m_chunk_top[0]);
            m_free_space = m_chunk_size - sizeof(void*);
            m_chunk_use++;
        }
        void *ptr = ((char*)m_chunk_top) + (m_chunk_size - m_free_space);
        m_free_space -= size;
        return ptr;
    }
    void Free(void* ptr, size_t size) //вернуть в список свободных
    {
        ((void**)ptr)[0] = m_list_free;
        ((void**)ptr)[1] = (void*)size;
        m_list_free = (void**)ptr;
    }
    size_t AllocSize()//сколько памяти выделили
    {
        return m_chunk_use*m_chunk_size - m_free_space;
    }
    size_t StorageSize()//размер всей памяти в управлении
    {
        return m_chunk_count*m_chunk_size;
    };
};//tdMemLite


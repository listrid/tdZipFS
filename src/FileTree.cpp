/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#include <windows.h>
#include <tdMemLite.hpp>
#include "FileTree.h"



FileTree::FileTree()
{
    m_mem = new tdMemLite();
    m_mem->Init(0x1000);
    m_root = NULL;
}


FileTree::~FileTree()
{
    Close();
    delete m_mem;
}


FileTree::Node* FileTree::FindChildW(Node* parent, const wchar_t *name, uint32_t nameLen)
{
    if(!parent)
    {
        parent = m_root;
        if(nameLen == 0)
            return parent;
    }
    Node* node = parent->m_child;
    while(node)
    {
        if(nameLen == node->m_nameLen)
        {
            if(memcmp(name, node->m_nameW, nameLen*sizeof(wchar_t)) == 0)
                return node;
        }
        node = node->m_next;
    }
    return NULL;
}

FileTree::Node* FileTree::FindChildA(Node* parent, const char* name, uint32_t nameLen)
{
    if(!parent)
    {
        parent = m_root;
        if(nameLen == 0)
            return parent;
    }
    Node* node = parent->m_child;
    while(node)
    {
        if(nameLen == node->m_nameLen)
        {
            if(memcmp(name, node->m_nameA, nameLen) == 0)
                return node;
        }
        node = node->m_next;
    }
    return NULL;
}


FileTree::Node* FileTree::AddChildNode(Node* parent, const char *name, uint32_t nameLen)
{
    if(!parent)
        parent = m_root;
    Node* node = (Node *)m_mem->Alloc(sizeof(Node));
    memset(node, 0, sizeof(Node));
    node->m_nameLen = nameLen;
//    node->m_parent  = parent;
    node->m_next    = parent->m_child;
    parent->m_child = node;
    node->m_nameA = name;
    node->m_nameW = (wchar_t*)m_mem->Alloc(sizeof(wchar_t)*(nameLen+1));
    MultiByteToWideChar((UINT)m_code_page, 0, name, (int)nameLen, const_cast<wchar_t*>(node->m_nameW), nameLen+1);
    return node;
}


void FileTree::Close()
{
    m_mem->FreeStorage();
    m_root = NULL;
}


bool FileTree::Parse(tdZipReader* zip, size_t codePage)
{
    if(!zip)
        return false;
    m_code_page = codePage;
    m_root = (Node *)m_mem->Alloc(sizeof(Node));
    memset(m_root, 0, sizeof(Node));
    m_root->m_isDir = true;
    m_root->m_nameA = "/";
    m_root->m_nameW = L"/";
    m_root->m_nameLen = 1;

    int maxNum = zip->GetNumFiles();
    for(int n = 0; n < maxNum; n++)
    {
        tdZipReader::FileInfo info;
        if(!zip->GetFileInfo(n, info))
            continue;
        Node* dir = NULL;
        size_t start_pos = 0;
        for(size_t i = 0 ; i < info.m_nameLen; i++)
        {
            if(info.m_name[i] == '\\' || info.m_name[i] == '/')
            {
                Node* node = FindChildA(dir, &info.m_name[start_pos], uint32_t(i - start_pos));
                if(!node)
                {
                    node = AddChildNode(dir, &info.m_name[start_pos], uint32_t(i - start_pos));
                    node->m_isDir = true;
                }
                dir = node;
                start_pos = i+1;
                continue;
            }
        }
        if(start_pos == info.m_nameLen)
            continue;
        Node* node = FindChildA(dir, &info.m_name[start_pos], uint32_t(info.m_nameLen - start_pos));
        if(!node)
        {
            node = AddChildNode(dir, &info.m_name[start_pos], uint32_t(info.m_nameLen - start_pos));
        }
        node->m_fileID = n;
        node->m_sizeData = info.m_size;
        node->m_time     = info.m_time;
    }
    return true;
}


FileTree::Node* FileTree::Find(const wchar_t* path, size_t sizePath)
{
    size_t len = 0, pos = 0;
    Node* dir = NULL;
    if(!sizePath)
        sizePath = lstrlenW(path);
    for(len = 0; len< sizePath; len++)
    {
        if(path[len] == '\\' || path[len] == '/')
        {
            if((len - pos) == 0 && path[1] == 0)
               return m_root;
            Node* node = FindChildW(dir, &path[pos], uint32_t(len - pos));
            if(!node)
                return NULL;
            dir = node;
            pos = len+1;
            continue;
        }
    }
    if(pos == len)
        return dir;
    return FindChildW(dir, &path[pos], uint32_t(len - pos));
}


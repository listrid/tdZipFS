/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once
#include <stdint.h>
#include <tdZip.h>

class tdMemLite;
class tdZipReader;



class  FileTree
{
public:

    struct Node
    {
        struct Iterator
        {
            friend struct Node;
            friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_node == b.m_node; };
            friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_node != b.m_node; };
            Iterator operator++(int)
            {
                Iterator tmp = *this;
                if(m_node)
                    m_node = m_node->m_next;
                return tmp;
            }
            Iterator& operator++()
            {
                if(m_node)
                    m_node = m_node->m_next;
                return *this;
            }
            Node& operator*() const { return *m_node; }
            Node* operator->() { return m_node; }
        private:
            Iterator(Node* n){ m_node = n; }
            Node* m_node;
        };

        const char*    m_nameA;
        const wchar_t* m_nameW;
        uint32_t m_nameLen;
        bool     m_isDir;
        uint64_t m_time;
        size_t   m_sizeData;
        size_t   m_fileID;

        Iterator begin() const { return Iterator(m_child); };
        static Iterator end() { return Iterator(NULL); };
    private:

//        Node* m_parent; // на родитель
        Node* m_next;   // на следующий элемент родителя
        Node* m_child;  // для директории, на его элементы
        friend class FileTree;
    };

    FileTree();
    ~FileTree();
    bool Parse(tdZipReader* zip, size_t codePage);

    Node* Find(const wchar_t* path, size_t sizePath = 0);
    void Close();

private:
    tdMemLite* m_mem;
    Node*      m_root;
    size_t     m_code_page;
    Node* AddChildNode(Node* parent, const char *name, uint32_t nameLen);

    Node* FindChildA(Node* parent, const char *name, uint32_t nameLen);
    Node* FindChildW(Node* parent, const wchar_t* name, uint32_t nameLen);
};
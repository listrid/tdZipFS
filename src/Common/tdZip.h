/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once
#ifndef _TDZIP_H_
#define _TDZIP_H_

bool tdInflate(const void *source, unsigned int sourceLen, void *dest, unsigned int *destLen, unsigned int *realSizeData);
bool tdInflate_Test(const void *source, unsigned int sourceLen, unsigned int *destLen, unsigned int *realSizeData);

unsigned int tdAdler32(const unsigned char *ptr, unsigned int buf_len, unsigned int adler32);
bool tdZlibUncompress(const void *source, unsigned int sourceLen, void *dest, unsigned int *destLen);


class tdFile;



class tdZipReader
{
public:
    tdZipReader();
    ~tdZipReader();
    bool Open(const char* fileName);
    bool Open(const wchar_t* fileName);

    void Close();
    bool IsOpen() const;

    int  GetNumFiles() const { return m_nEntries; }
    int  GetFileID(const char* fileName);// -1 не найдено
    bool GetFileName(int fileID, char *outName300) const;
    int  GetFileSize(int fileID) const;// -1 ошибка
    bool ReadFile(int fileID, void *pBuf);

    struct FileInfo
    {
        const char* m_name;
        size_t      m_nameLen;
        
        size_t      m_size;
        uint64_t    m_time;
    };
    bool GetFileInfo(int fileID, FileInfo& info);
private:
    bool Parser();

    struct tdZipDirHeader;
    struct tdZipDirFileHeader;
    struct tdZipLocalHeader;

    tdFile* m_file;
    char*   m_pDirData;
    int     m_nEntries;
    const tdZipDirFileHeader **m_papDir;
};

#endif //_TDZIP_H_
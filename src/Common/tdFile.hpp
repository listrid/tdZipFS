/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once
#ifndef _TDFILE_HPP_
#define _TDFILE_HPP_
#ifdef _WIN32
  #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
  #endif
  #include <Windows.h>
  #include <io.h>
  #include <ktmw32.h>
  #pragma comment(lib, "KtmW32.lib")
#else
  #include <sys/statfs.h>
  #include <unistd.h>
  #include <dirent.h>
  #include <cerrno> 
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <ctype.h>

/*************************************************************
bool     tdFile::Open(const char* fileName, size_t flags) - открыть файл (return false - не удалось)
void     tdFile::CleanHandle();               - обнулить хентл файла (чтоб не закрыл деструктор)
bool     tdFile::Flush()                      - точно скинуть на диск
int64_t  tdFile::Size()                       - узнать размер файла
bool     tdFile::SetPos(uint64_t pos)         - изменить позицию в файле
int64_t  tdFile::GetPos()                     - узнать позицию в файле
size_t   tdFile::Write(void* buf, size_t len) - записать данные в файл
size_t   tdFile::Read(void* buf, size_t len)  - прочитать данные из файла
void     tdFile::Close()                      - закрыть файл
void     tdFile::IsOpen()                     - проверить что файл открыт
bool     tdFile::Stat(struct stat &t_stat)    - инфа об открытом файле
size_t   tdFile::GetError();                  - запросить состояние ошибок

bool tdFileFind::IsFind()
bool tdFileFind::Start(const char* fileMask, bool dir)  - начать поиск файла по маске (return false - не найдено)
bool tdFileFind::Find(char* outFileName)                - запросить найденый файл (return false - больше нет)
void tdFileFind::Close()                                - закрыть поиск

uint64_t tdFile_FreeSpace(const char* path, int64_t* total = NULL)            - сколько свободного места в дирректории осталось
bool     tdFile_CreateFolder(const char* path)                                - создать дирректорию
bool     tdFile_Delete(const char* fileName)                                  - удалить файл
bool     tdFile_ReName(const char* fileNameExisting, const char* fileNameNew) - переименовать файл (или переместить)
bool     tdFile_Existing(const char* name)                                    - проверить на существование файла
bool     tdFile_Copy(const char* fileNameSrc, const char* fileNameNew)        - сделать копию файла
FileInfo tdFile_Info(const char* fileName)                                    - инфа о файле (размер, время создания, чтения, записи)

bool   tdFile_SetCurDir(const char* dir)                                        - установить активную дирректорию
size_t tdFile_GetSelfPath(char* outPath, size_t maxlen)                         - путь к выполняемому модулю программы
size_t tdFile_GetRealPath(const char* inPath, char* outPath, size_t sizeOutBuf) - получить полный путь из относительного

**************************************************************/


class tdFile
{
#ifdef _WIN32
    HANDLE file;
#else
    int    file; 
#endif
    uint32_t m_error;
public:
    //флаги открытия файла
    const static size_t _OPEN_READ     = (1<<0);  //открыть для чтения
    const static size_t _OPEN_WRITE    = (1<<1);  //открыть для записи
//    const static size_t _OPEN_SHARED   = (1<<2);  //не блокировать файл
    const static size_t _OPEN_ALWAYS   = (1<<3);  //открыть, если нету файла то создать 
    const static size_t _CREATE_ALWAYS = (1<<4);  //всегда создавать заново
    const static size_t r  = 1;
    const static size_t rw = 3;
    const static size_t crw = 19;
    //флаги ошибки
    const static size_t _ERROR_READ   = (1<<1)+1;
    const static size_t _ERROR_WRITE  = (1<<2)+1;
    const static size_t _ERROR_SETPOS = (1<<3)+1;
    const static size_t _ERROR        = 1;

    void CleanHandle()
    {
#ifdef _WIN32
        file = NULL;
#else
        file = 0;
#endif
        m_error = 0;
    }
    tdFile()
    {
        CleanHandle();
    }
    ~tdFile()
    {
        Close();
    }
#ifdef _WIN32
    inline bool Open(const wchar_t* fileName, size_t flags)
    {
        Close();
        int access = 0, share = 0, createDescr = OPEN_EXISTING;
        if(flags&_OPEN_READ)
        {
            access |= GENERIC_READ;
            share  |= FILE_SHARE_READ;
        }
        if(flags&_OPEN_WRITE)
        {
            access |= GENERIC_WRITE;
            share  |= FILE_SHARE_WRITE;
        }
        if(flags&_CREATE_ALWAYS)
            createDescr = CREATE_ALWAYS;
        if(flags&_OPEN_ALWAYS)
            createDescr = OPEN_ALWAYS;
        HANDLE _file = CreateFileW(fileName, access, share, NULL, createDescr, FILE_ATTRIBUTE_NORMAL, NULL);
        if(_file == INVALID_HANDLE_VALUE)
            return false;
        this->file = _file;
        return true;
    }
#endif
    inline bool Open(const char* fileName, size_t flags)
    {
        Close();
#ifdef _WIN32
        int access = 0, share = 0, createDescr = OPEN_EXISTING;
        if(flags&_OPEN_READ)
        {
            access |= GENERIC_READ;
            share  |= FILE_SHARE_READ;
        }
        if(flags&_OPEN_WRITE)
        {
            access |= GENERIC_WRITE;
            share  |= FILE_SHARE_WRITE;
        }
        if(flags&_CREATE_ALWAYS)
            createDescr = CREATE_ALWAYS;
        if(flags&_OPEN_ALWAYS)
            createDescr = OPEN_ALWAYS;
        HANDLE _file = CreateFileA(fileName, access, share, NULL, createDescr, FILE_ATTRIBUTE_NORMAL, NULL);
        if(_file == INVALID_HANDLE_VALUE)
            return false;
#else
        int access = 0;
        if(flags&_OPEN_READ)
            access = O_RDONLY;
        if(flags&_OPEN_WRITE)
            access = O_WRONLY;
        if((flags&(_OPEN_WRITE|_OPEN_READ)) == (_OPEN_WRITE|_OPEN_READ))
            access = O_RDWR;
        if(flags&_OPEN_ALWAYS)
            access |= O_CREAT;
        if(flags&_CREATE_ALWAYS)
            access |= O_CREAT|O_TRUNC;
        int oldmask;
        if(access&O_CREAT)
            oldmask = umask(0);
        int _file = open64(fileName, access | O_CLOEXEC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);//O_CLOEXEC  не дублировать при fork
        if(access&O_CREAT)
            umask(oldmask);
        if(_file < 0)
            return false;
#endif
        this->file = _file;
        return true;
    }

    inline bool Flush()
    {
#ifdef _WIN32
        bool ret = FlushFileBuffers(file) != FALSE;
#else
        bool ret = fsync(file) == 0;
#endif
        if(!ret)
            m_error |= _ERROR;
        return ret;
    }

    inline int64_t Size()
    {
#ifdef _WIN32
        LARGE_INTEGER ret;
        if(!GetFileSizeEx(file, &ret))
            m_error |= _ERROR;
        return ret.QuadPart;
#else
        int64_t pos = lseek64(file, 0L, SEEK_CUR);
        int64_t ret = lseek64(file, 0L, SEEK_END);
        if((lseek64(file, pos, SEEK_SET)|ret) < 0)
            m_error |= _ERROR;
        return ret;
#endif
    }

    inline bool SetPos(uint64_t pos)
    {
#ifdef _WIN32
        LARGE_INTEGER p;
        p.QuadPart = pos;
        bool ret = SetFilePointerEx(file, p, NULL, FILE_BEGIN) == TRUE;
#else
        bool ret = lseek64(file, pos, SEEK_SET) == int64_t(pos);
#endif
        if(!ret)
            m_error |= _ERROR_SETPOS;
        return ret;
    }

    inline int64_t GetPos()
    {
#ifdef _WIN32
        LARGE_INTEGER ret, p;
        p.QuadPart = 0;
        if(!SetFilePointerEx(file, p, &ret, FILE_CURRENT))
            m_error |= _ERROR;
        return ret.QuadPart;
#else
        int64_t ret = lseek64(file, 0L, SEEK_CUR);
        if(ret < 0)
            m_error |= _ERROR;
        return ret;
#endif
    }

    inline size_t Write(const void* buf, size_t len)
    {
#ifdef _WIN32
        DWORD p = 0;
        if(WriteFile(file, buf, (DWORD)len, &p, NULL) == FALSE || p != len)
            m_error |= _ERROR_WRITE;
        return p;
#else
        int ret = write(file, buf, len);
        if(ret == -1 || ret != int(len))
        {
            m_error |= _ERROR_WRITE;
            ret = 0;
        }
        return ret;
#endif
    }

    inline size_t Read(void* buf, size_t len)
    {
#ifdef _WIN32
        DWORD p;
#else
        ssize_t p;
#endif
        char* _buf = (char*)buf;
        size_t ret = 0, r;
        do
        {
            r = len;
            if(len > 0x2000000)
                r = 0x2000000;
#ifdef _WIN32
            if(ReadFile(file, _buf, (DWORD)r, &p, NULL) == FALSE || p == 0)
            {
                if(!ret)
                    m_error |= _ERROR_READ;
                break;
            }
#else
            p = read(file, _buf, r);
            if(p <= 0)
            {
                if(!ret)
                    m_error |= _ERROR_READ;
                break;
            }
#endif
            _buf += p;
            ret  += p;
            len  -= p;
        }while(len > 0);
        return ret;
    }

    inline void Close()
    {
#ifdef _WIN32
        if(file == NULL)
            return;
        CloseHandle(file);
        file = NULL;
#else
        if(file == 0)
            return;
        close(file);
        file = 0;
#endif
        m_error = 0;
    }
    inline bool IsOpen()
    {
#ifdef _WIN32
        return file != NULL;
#else
        return file != 0;
#endif
    }
    inline bool Stat(struct stat &t_stat)
    {
#ifdef _WIN32
        return fstat(_open_osfhandle((intptr_t)file, (int)(O_RDONLY)), &t_stat) >= 0;
#else
        return fstat(file, &t_stat) >= 0;
#endif
    }
    inline size_t GetError()
    {
        return m_error;
    }
};




class tdFileFind
{
#ifdef _WIN32
    HANDLE m_hf;
#else
    DIR*   m_hf;
    char   m_base[260];
#endif
    bool   m_dir;
    char   m_mask[260];
    char   m_fileName[260];

    static bool CheckString(const char *name, const char *mask)
    {
        const char* last = NULL;
        for(;; mask++, name++) {
            char ch = tolower(*name);
            if(ch == '\\') ch = '/';
            if(*mask != '?' && tolower(*mask) != ch) break;
            if(ch == '\0') return (bool)!*mask;
        }
        if(*mask != '*') return false;
        for(;; mask++, name++) {
            while(*mask == '*') {
                last = mask++;
                if(*mask == '\0')return (bool)!*mask;
            }
            char ch = tolower(*name);
            if(ch == '\\') ch = '/';
            if(ch == '\0')  return (bool)!*mask;
            if(*mask != '?' && tolower(*mask) != ch){ name -= (size_t)(mask - last) - 1; mask = last; }
        }
    }
public:
    tdFileFind(){ m_hf = NULL; m_fileName[0] = 0;}
    ~tdFileFind(){ this->Close(); }
    bool IsFind(){ return m_hf != NULL && m_fileName[0] != 0;}
#ifdef _WIN32
    bool Start(const char* fileMask, bool dir)
    {
        this->Close();
        WIN32_FIND_DATAA find;
        char mask[MAX_PATH];
        HANDLE hf = FindFirstFileA(fileMask, &find);
        if(hf == INVALID_HANDLE_VALUE)
            return false;

        size_t i, indx = 0;
        for(i = 0; fileMask[i]; i++)
        {
            if(fileMask[i] == '/' || fileMask[i] == '\\')
                indx = i+1;
        }
        memcpy(mask, fileMask+indx, i - indx + 1);
        while(true)
        {
            if(((!dir && (find.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == 0)) ||
                (dir && (find.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != 0 && memcmp(find.cFileName, "..", 3) && memcmp(find.cFileName, ".", 2)))
            {
                if(CheckString(find.cFileName, mask))
                {
                    m_hf  = hf;
                    m_dir = dir;
                    strcpy(m_fileName, find.cFileName);
                    strcpy(m_mask, mask);
                    return true;
                }
            }
            if(!FindNextFileA(hf, &find))
                break;
        }
        FindClose(hf);
        return false;
    }
    bool Find(char* outFileName)
    {
        WIN32_FIND_DATAA find;
        if(!m_hf || m_fileName[0] == 0)
            return false;
        strcpy(outFileName, m_fileName);
        m_fileName[0] = 0;
        bool dir = m_dir;
        while(FindNextFileA(m_hf, &find))
        {
            if(((find.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) == 0 ) == dir)
                continue;
            if(!CheckString(find.cFileName, m_mask))
                continue;
            strcpy(m_fileName, find.cFileName);
            break;
        }
        return true;
    }
#else //Linux
    bool Start(const char* fileMask, bool bDir)
    {
        this->Close();
        char mask[260];
        char dir[260];
        int i, len = strlen(fileMask);
        i = len-1;
        while(i && fileMask[i] != '\\' && fileMask[i] != '/')
            i--;
        if(i)
        {
            memcpy(mask, &fileMask[i+1], len-i);
            memcpy(dir, fileMask, i+1);
            dir[i+1] = 0;
        }else{
            memcpy(mask, fileMask, len+1);
            dir[0] = '.';
            dir[1] = 0;
        }
        DIR *dirp = opendir(dir);

        if(dirp == NULL)
            return false;
        dirent* dp;
        while(true)
        {
            if ((dp = readdir(dirp)) != NULL)
            {
                if(memcmp(dp->d_name, "..", 3) == 0 || memcmp(dp->d_name, ".", 2) == 0)
                    continue;
                if(CheckString(dp->d_name, mask))
                {
                    struct stat buffer = {0};
                    char path[512];
                    sprintf(path, "%s/%s", dir, dp->d_name);
                    lstat(path, &buffer);
                    if((S_ISDIR(buffer.st_mode) != 0) == bDir)
                        break;
                }
            }else{
                closedir(dirp);
                return false;
            }
        }
        m_hf  = dirp;
        m_dir = bDir;
        strcpy(m_fileName, dp->d_name);
        strcpy(m_mask, mask);
        strcpy(m_base, dir);
        return true;
    }

    bool Find(char* outFileName)
    {
        if(!m_hf || m_fileName[0] == 0)
            return false;
        strcpy(outFileName, m_fileName);
        m_fileName[0] = 0;
        dirent* dp;
        while((dp = readdir(m_hf)) != NULL)
        {
            if(CheckString(dp->d_name, m_mask))
            {
                struct stat buffer;
                char path[512];
                sprintf(path, "%s/%s", m_base, dp->d_name);
                lstat(path, &buffer);
                if((S_ISDIR(buffer.st_mode) != 0) == m_dir)
                {
                    strcpy(m_fileName, dp->d_name);
                    break;
                }
            }
        }
        return true;
    }
#endif // _WIN32
    void Close()
    {
        if(m_hf == NULL)
            return;
#ifdef _WIN32
        FindClose(m_hf);
#else
        closedir(m_hf);
#endif
        m_hf = NULL;
    }
};


inline bool tdFile_DiskStat(const char* path, int64_t* total, int64_t* available)
{
    if(total)
        *total = 0;
#ifdef _WIN32
    {//по правильному путю
        ULARGE_INTEGER liFreeAvailable, liTotal;
        if(GetDiskFreeSpaceExA(path, &liFreeAvailable, &liTotal, NULL))
        {
            if(total)
                *total = liTotal.QuadPart;
            if(available)
                *available = liFreeAvailable.QuadPart;
            return true;
        }
    }
/*    {//по диску
        char szDrive[_MAX_DRIVE+1];
        if(path != NULL && path[0] != 0 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
        {
            szDrive[0] = path[0]; 
        }else{
            return 0;
            //  char szPath[MAX_PATH];
            //  GetCurrentDirectoryA(MAX_PATH, szPath);
            //  szDrive[0] = path[0];
        }
        szDrive[1] = ':', szDrive[2] = '\\', szDrive[3] = 0;
        DWORD dwSecPerClus, dwBytesPerSec, dwFreeClus, dwTotalClus;
        if(GetDiskFreeSpaceA(szDrive, &dwSecPerClus, &dwBytesPerSec, &dwFreeClus, &dwTotalClus))
            return uint64_t(dwFreeClus) * dwSecPerClus * dwBytesPerSec;
    }*/
    return false;
#else //Linux
    struct stat stst;
    struct statfs stfs;
    if(stat(path, &stst) == -1 )
        return false;
    if(statfs(path, &stfs) == -1)
        return false;
    if(total)
        *total = stfs.f_blocks * stst.st_blksize;
    if(available)
        *available = stfs.f_bavail * stst.st_blksize;
    return true;
#endif
}


inline bool tdFile_CreateFolder(const char* path)
{
#ifdef _WIN32
    char _path[512];
    _path[0] = path[0];
    _path[1] = path[1];
    _path[2] = path[2];
    for(int i = (path[1] ==':')?3:0; path[i]; i++)
    {
        _path[i] = path[i];
        if(path[i] =='\\' || path[i] == '/')
        {
            _path[i+1] = 0;
            if(!CreateDirectoryA(_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
                return false;
        }
    }
    return !((!CreateDirectoryA(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS));
#else
    char _path[512];
    _path[0] = path[0];
    int oldmask = umask(0);
    for(int i = 1; path[i-1]; i++)
    {
        _path[i] = path[i];
        if(path[i] =='\\' || path[i] == '/')
        {
            _path[i+1] = 0;
            mkdir(_path, S_IRUSR|S_IWUSR|S_IXUSR | S_IRGRP|S_IWGRP|S_IXGRP | S_IROTH|S_IWOTH|S_IXOTH);
        }
    }
    bool ret = mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR | S_IRGRP|S_IWGRP|S_IXGRP | S_IROTH|S_IWOTH|S_IXOTH) == 0;
    if(!ret)
        ret = (errno == EEXIST);
    umask(oldmask);
    return ret;
#endif
}


inline bool tdFile_Delete(const char* fileName)
{
#ifdef _WIN32
    return DeleteFileA(fileName) == TRUE;
#else
    return unlink(fileName) == 0;
#endif
}


inline bool tdFile_ReName(const char* fileNameExisting, const char* fileNameNew)
{
#ifdef _WIN32
    DWORD f = GetFileAttributesA(fileNameExisting);
    if(f == DWORD(-1))
        return false;
    DWORD mvF = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH ;
    if(!(f&FILE_ATTRIBUTE_DIRECTORY))
        mvF |= MOVEFILE_REPLACE_EXISTING;
    HANDLE hTransaction = CreateTransaction(NULL, 0, 0, 0, 0, INFINITE, NULL);
    bool ret = MoveFileTransactedA(fileNameExisting, fileNameNew, NULL, NULL, mvF, hTransaction) == TRUE;
    if(!ret)
    {
        RollbackTransaction(hTransaction);
    }else{
        CommitTransaction(hTransaction);
    }
    CloseHandle(hTransaction);
    return ret;
#else
    return rename(fileNameExisting, fileNameNew) == 0;
#endif
}


inline bool tdFile_IsExisting(const char* name)
{
#ifdef _WIN32
    return GetFileAttributesA(name) != DWORD(-1);
#else
    return access(name, 0) != -1;
#endif
}


inline bool tdFile_Copy(const char* fileNameSrc, const char* fileNameNew)
{
#ifdef _WIN32
    return CopyFileA(fileNameSrc, fileNameNew, FALSE) == TRUE;
#else
    char cmd[1024];
    sprintf(cmd, "/bin/cp -p \'%s\' \'%s\'", fileNameSrc, fileNameNew);
    return system(cmd) == 0;
#endif
}


inline bool tdFile_SetCurDir(const char* dir)
{
#ifdef _WIN32
    return SetCurrentDirectoryA(dir) != FALSE;
#else
    return chdir(dir) == 0;
#endif
}


inline bool tdFile_GetCurDir(char* outPath, size_t sizeOutBuf)
{
#ifdef _WIN32
    return GetCurrentDirectoryA((DWORD)sizeOutBuf, outPath) != FALSE;
#else
    return getcwd(outPath, sizeOutBuf) != NULL;
#endif
}

struct  tdFileInfo
{
    uint64_t size; 
    uint64_t creation_time; //localtime()
    uint64_t access_time;
    uint64_t modification_time;
};

inline bool tdFile_Info(const char* fileName, tdFileInfo& info)
{
#if defined(_WIN32)
    struct _stat64  s;
    if(_stat64(fileName, &s) < 0)
        return false;
#else
    struct stat64  s;
    if(stat64(fileName, &s) < 0)
        return false;
#endif
    info.size = s.st_size;
    info.creation_time = s.st_ctime;
    info.access_time   = s.st_atime;
    info.modification_time = s.st_mtime;
    return true;
}


inline size_t tdFile_GetSelfPath(char* outPath, size_t maxlen)
{
#if defined(_WIN32)
    size_t len = GetModuleFileNameA(NULL, outPath, (DWORD)maxlen);
    while(len && outPath[len]!= '\\')
        len --;
#else
    size_t len = readlink("/proc/self/exe", outPath, maxlen);
    while(len && outPath[len] != '/')
        len --;
#endif
    if(len)
        len++;
    outPath[len] = 0;
    return len;
}


inline size_t tdFile_GetRealPath(const char* inPath, char* outPath, size_t sizeOutBuf)
{
#if defined(_WIN32)
    return (size_t)GetFullPathNameA(inPath, (DWORD)sizeOutBuf, outPath, NULL);
#else
    char exepath[PATH_MAX + 1];
    char* tmp = realpath(inPath, exepath);
    (void)tmp;
    size_t len = strlen(exepath);
    if(len+2 > sizeOutBuf)
        len = sizeOutBuf-2;
    memcpy(outPath, exepath, len);
    outPath[len] = '/';
    outPath[len+1] = 0;
    return len;
#endif
}

#endif //_TDFILE_HPP_

/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/

#include <winfsp/winfsp.h>

#include <tchar.h>
#include "FileTree.h"
#include "ZipÑache.h"

#define PROGNAME        L"tdZipFS"
#define ALLOCATION_UNIT 512
#define FULLPATH_SIZE   (MAX_PATH + FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR))


typedef struct
{
    FSP_FILE_SYSTEM * m_FileSystem;

    tdZipReader* m_zip;
    FileTree*    m_tree;
    ZipCache*    m_cache;
} ZIPFS;


typedef struct
{
    const FileTree::Node* m_node;

    const char*  m_data;
    size_t m_size;
} ZIPFS_FILE_CONTEXT;


static NTSTATUS GetFileInfo(const FileTree::Node* node, FSP_FSCTL_FILE_INFO* FileInfo)
{
    FileInfo->FileAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE;
    if(node->m_isDir)
        FileInfo->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    FileInfo->ReparseTag     = 0;
    FileInfo->FileSize       = node->m_sizeData;
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime   = node->m_time;
    FileInfo->LastAccessTime = 0;
    FileInfo->LastWriteTime  = node->m_time;
    FileInfo->ChangeTime     = 0;
    FileInfo->IndexNumber    = node->m_fileID;
    FileInfo->HardLinks      = 0;
    return STATUS_SUCCESS;
}


static NTSTATUS ZIPFS_GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    ZIPFS *zipFS = (ZIPFS*)FileSystem->UserContext;

    VolumeInfo->TotalSize = 0x10000;
    VolumeInfo->FreeSize  = 00;
    memcpy(VolumeInfo->VolumeLabel, L"ZIP", 8);
    VolumeInfo->VolumeLabelLength = 8;
    return STATUS_SUCCESS;
}


static NTSTATUS ZIPFS_SetVolumeLabel(FSP_FILE_SYSTEM *FileSystem, PWSTR VolumeLabel, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}


static NTSTATUS ZIPFS_Create(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, UINT32 FileAttributes,
                             PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize, PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_FILE_INVALID;
}


static NTSTATUS ZIPFS_Open(FSP_FILE_SYSTEM *FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
    ZIPFS *zipFS = (ZIPFS *)FileSystem->UserContext;

    FileTree::Node* node = zipFS->m_tree->Find(FileName, 0);
    if(!node)
        return STATUS_OBJECT_NAME_NOT_FOUND;
    ZIPFS_FILE_CONTEXT* FileContext = (ZIPFS_FILE_CONTEXT*)malloc(sizeof(ZIPFS_FILE_CONTEXT));
    if(NULL == FileContext)
        return STATUS_INSUFFICIENT_RESOURCES;
    memset(FileContext, 0, sizeof(ZIPFS_FILE_CONTEXT));
    FileContext->m_node = node;
    
    if(!zipFS->m_cache->Load(node->m_fileID, FileContext->m_data, FileContext->m_size))
    {
        free(FileContext);
        return STATUS_FILE_INVALID;
    }
    *PFileContext = FileContext;
    return GetFileInfo(node, FileInfo);
}


static VOID ZIPFS_Close(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext0)
{
    ZIPFS* zipFS = (ZIPFS*)FileSystem->UserContext;
    ZIPFS_FILE_CONTEXT* FileContext = (ZIPFS_FILE_CONTEXT*)FileContext0;
    zipFS->m_cache->Free(FileContext->m_node->m_fileID);
    free(FileContext);
}


static NTSTATUS ZIPFS_Read(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext0, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred)
{
    ZIPFS* zipFS = (ZIPFS*)FileSystem->UserContext;
    ZIPFS_FILE_CONTEXT* FileContext = (ZIPFS_FILE_CONTEXT*)FileContext0;

    if(Offset >= FileContext->m_size)
        return STATUS_END_OF_FILE;
    UINT64 endOffset = Offset + Length;
    if(endOffset > FileContext->m_size)
        endOffset = FileContext->m_size;
    memcpy(Buffer, (PUINT8)FileContext->m_data + Offset, (size_t)(endOffset - Offset));
    *PBytesTransferred = (ULONG)(endOffset - Offset);
    return STATUS_SUCCESS;
}


static NTSTATUS ZIPFS_GetFileInfo(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
    return GetFileInfo(((ZIPFS_FILE_CONTEXT*)FileContext)->m_node, FileInfo);
}


static BOOLEAN AddDirInfo(const FileTree::Node* Node, const wchar_t* Name, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    size_t NameBytes;
    if(Name)
    {
        NameBytes = wcslen(Name) * sizeof WCHAR;
    }else{
        NameBytes = Node->m_nameLen * sizeof WCHAR;
        Name      = Node->m_nameW;
    }
    UINT8 DirInfoBuf[sizeof FSP_FSCTL_DIR_INFO + 1024 * sizeof WCHAR];
    FSP_FSCTL_DIR_INFO* DirInfo = (FSP_FSCTL_DIR_INFO*)DirInfoBuf;
    DirInfo->Size = (UINT16)(sizeof FSP_FSCTL_DIR_INFO + NameBytes);

    GetFileInfo(Node, &DirInfo->FileInfo);

    memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    memcpy(DirInfo->FileNameBuf, Name, NameBytes + sizeof WCHAR);

    return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
}


static NTSTATUS ZIPFS_ReadDirectory(FSP_FILE_SYSTEM *FileSystem, PVOID FileContext0, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG BufferLength, PULONG PBytesTransferred)
{
    ZIPFS *zipFS = (ZIPFS*)FileSystem->UserContext;
    ZIPFS_FILE_CONTEXT *FileContext = (ZIPFS_FILE_CONTEXT*)FileContext0;
    if(FileContext->m_node->m_isDir)
    {
        if(!Marker)
        {
            if(!AddDirInfo(FileContext->m_node, L".", Buffer, BufferLength, PBytesTransferred))
                return STATUS_SUCCESS;
        }
        if(!Marker || (L'.' == Marker[0] && L'\0' == Marker[1]))
        {
            if(!AddDirInfo(FileContext->m_node, L"..", Buffer, BufferLength, PBytesTransferred))
                return STATUS_SUCCESS;
            Marker = 0;
        }
    }
    for(FileTree::Node::Iterator it = FileContext->m_node->begin(); it != FileTree::Node::end(); it++)
    {
        if(!AddDirInfo(&*it, NULL, Buffer, BufferLength, PBytesTransferred))
            break;
    }
    FspFileSystemAddDirInfo(0, Buffer, BufferLength, PBytesTransferred);
    return STATUS_SUCCESS;
}


static NTSTATUS ZIPFS_Overwrite(FSP_FILE_SYSTEM* FileSystem, PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize, FSP_FSCTL_FILE_INFO* FileInfo)
{
    return STATUS_DISK_FULL;
}


static FSP_FILE_SYSTEM_INTERFACE ZipFS_Interface =
{
    ZIPFS_GetVolumeInfo,
    NULL,//SetVolumeLabel,
    NULL,//GetSecurityByName,
    ZIPFS_Create,
    ZIPFS_Open,
    ZIPFS_Overwrite,
    NULL,//Cleanup,
    ZIPFS_Close,
    ZIPFS_Read,
    NULL,//Write,
    NULL,//Flush,
    ZIPFS_GetFileInfo,
    NULL,//SetBasicInfo,
    NULL,//SetFileSize,
    NULL,//CanDelete
    NULL,//Rename,
    NULL,//GetSecurity,
    NULL,//SetSecurity,
    ZIPFS_ReadDirectory,
    NULL,//ResolveReparsePoints
    NULL,//GetReparsePoint
    NULL,//SetReparsePoint
    NULL,//DeleteReparsePoint
    NULL,//GetStreamInfo
    NULL,//GetDirInfoByName
    NULL,//Control
    NULL,//SetDelete
};


static void ZipFS_Delete(ZIPFS * zipFS)
{
    if(NULL != zipFS->m_FileSystem)
        FspFileSystemDelete(zipFS->m_FileSystem);
    if(NULL != zipFS->m_zip)
        delete zipFS->m_zip;
    if(NULL != zipFS->m_tree)
        delete zipFS->m_tree;
    if(NULL != zipFS->m_cache)
        delete zipFS->m_cache;
    free(zipFS);
}


static NTSTATUS ZipFS_Create(PWSTR zipFileName, size_t codePage, PWSTR MountPoint,  ZIPFS **PzipFS)
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    ZIPFS *zipFS = NULL;
    NTSTATUS Result;

    *PzipFS = NULL;

    tdZipReader* zip = new tdZipReader();
    if(!zip->Open(zipFileName))
    {
        delete zip;
        return STATUS_FILE_INVALID;
    }

    zipFS = (ZIPFS*)malloc(sizeof ZIPFS);
    if(NULL == zipFS)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memset(zipFS, 0, sizeof *zipFS);

    zipFS->m_zip = zip;
    zipFS->m_tree = new FileTree();
    zipFS->m_tree->Parse(zip, codePage);
    zipFS->m_cache = new ZipCache();
    zipFS->m_cache->Init(zip);

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.SectorSize = ALLOCATION_UNIT;
    VolumeParams.SectorsPerAllocationUnit = 1;
    VolumeParams.VolumeCreationTime  = 0;
    VolumeParams.VolumeSerialNumber  = 0;
    VolumeParams.FileInfoTimeout     = 10000;
    VolumeParams.CaseSensitiveSearch = 0;
    VolumeParams.CasePreservedNames  = 1;
    VolumeParams.UnicodeOnDisk  = 1;
    VolumeParams.PersistentAcls = 1;
    VolumeParams.PostCleanupWhenModifiedOnly = 1;
    VolumeParams.PassQueryDirectoryPattern   = 1;
    VolumeParams.FlushAndPurgeOnCleanup      = 1;
    VolumeParams.UmFileContextIsUserContext2 = 1;
    wcscpy_s(VolumeParams.FileSystemName, sizeof VolumeParams.FileSystemName / sizeof(WCHAR), L"" PROGNAME);

    Result = FspFileSystemCreate( VolumeParams.Prefix[0] ? L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME, &VolumeParams, &ZipFS_Interface,  &zipFS->m_FileSystem);
    if (!NT_SUCCESS(Result))
        goto exit;
    zipFS->m_FileSystem->UserContext = zipFS;

    Result = FspFileSystemSetMountPoint(zipFS->m_FileSystem, MountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    FspFileSystemSetDebugLog(zipFS->m_FileSystem, 0);

    Result = STATUS_SUCCESS;

exit:
    if (NT_SUCCESS(Result))
        *PzipFS = zipFS;
    else if (NULL != zipFS)
        ZipFS_Delete(zipFS);
    return Result;
}


static ULONG wcstol_deflt(wchar_t *w, ULONG deflt)
{
    wchar_t *endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}


static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
#define argtos(v) if (arge > ++argp) v = *argp; else goto usage
#define argtol(v)  if (arge > ++argp) v = wcstol_deflt(*argp, v); else goto usage

    wchar_t **argp, **arge;
    PWSTR zipFileName = NULL;
    PWSTR MountPoint = NULL;
    ZIPFS *zipFS = NULL;
    size_t codePage = 866;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'f':
            argtos(zipFileName);
            break;
        case L'm':
            argtos(MountPoint);
            break;
        case L'c':
            argtol(codePage);
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (NULL == zipFileName || NULL == MountPoint)
        goto usage;

    Result = ZipFS_Create(zipFileName, codePage, MountPoint, &zipFS);
    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE, L"cannot create file system");
        goto exit;
    }

    Result = FspFileSystemStartDispatcher(zipFS->m_FileSystem, 0);
    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE, L"cannot start file system");
        goto exit;
    }

    Service->UserContext = zipFS;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != zipFS)
        ZipFS_Delete(zipFS);

    return Result;

usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -f zip file \n"
        "    -m MountPoint      [X:|*|directory]\n"
        "    -c code page       [866, 1251, ...]\n";

    FspServiceLog(EVENTLOG_ERROR_TYPE , usage, L"" PROGNAME);
    return STATUS_UNSUCCESSFUL;

#undef argtos
#undef argtol
}


static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    ZIPFS *zipFS = (ZIPFS*)Service->UserContext;

    FspFileSystemStopDispatcher(zipFS->m_FileSystem);
    ZipFS_Delete(zipFS);

    return STATUS_SUCCESS;
}


int wmain(int argc, wchar_t **argv)
{
    if (!NT_SUCCESS(FspLoad(0)))
        return ERROR_DELAY_LOAD_FAILED;

    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}

#include <windows.h>
#include <string.h>

#define _S_IFDIR 0x4000
#define _S_IFREG 0x8000
#define _S_IREAD 0x0100
#define _S_IWRITE 0x0080
#define _S_IEXEC 0x0040

#pragma pack(push, 8)
struct _stat64 {
    unsigned int st_dev;
    unsigned short st_ino;
    unsigned short st_mode;
    short st_nlink;
    short st_uid;
    short st_gid;
    unsigned int st_rdev;
    __int64 st_size;
    __int64 st_atime;
    __int64 st_mtime;
    __int64 st_ctime;
};
#pragma pack(pop)

static __int64 file_time_to_unix(FILETIME value)
{
    ULARGE_INTEGER windows_time;
    windows_time.LowPart = value.dwLowDateTime;
    windows_time.HighPart = value.dwHighDateTime;
    return (__int64)(windows_time.QuadPart / 10000000ULL) - 11644473600LL;
}

int __cdecl _stat64(const char *path, struct _stat64 *result)
{
    WIN32_FILE_ATTRIBUTE_DATA data;

    if (path == NULL || result == NULL ||
        !GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
        errno = EINVAL;
        return -1;
    }

    memset(result, 0, sizeof(*result));
    result->st_mode = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
        (_S_IFDIR | _S_IREAD | _S_IEXEC) : (_S_IFREG | _S_IREAD);
    if (!(data.dwFileAttributes & FILE_ATTRIBUTE_READONLY)) result->st_mode |= _S_IWRITE;
    result->st_nlink = 1;
    result->st_size = ((__int64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    result->st_atime = file_time_to_unix(data.ftLastAccessTime);
    result->st_mtime = file_time_to_unix(data.ftLastWriteTime);
    result->st_ctime = file_time_to_unix(data.ftCreationTime);
    return 0;
}

#if defined(__i386__)
/* TinyCC's 32-bit PE linker canonicalizes the imported _stat64 spelling to
 * _stat even when the public header selects the 64-bit-time structure. */
int __cdecl _stat(const char *path, struct _stat64 *result)
{
    return _stat64(path, result);
}
#endif

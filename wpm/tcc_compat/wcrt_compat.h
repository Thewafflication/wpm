#ifndef WPM_WCRT_COMPAT_H
#define WPM_WCRT_COMPAT_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

/* WCRT uses a 64-bit time_t on every architecture. Prevent TinyCC's MinGW
 * compatibility headers from redeclaring it as 32-bit in x86 builds. */
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
#endif

#ifndef ULLONG_MAX
#define ULLONG_MAX 18446744073709551615ULL
#endif

__declspec(dllimport) extern int wcrt_errno;
#ifndef errno
#define errno wcrt_errno
#endif

#ifndef EAGAIN
#define EAGAIN 11
#endif
#ifndef EACCES
#define EACCES 13
#endif
#ifndef EFBIG
#define EFBIG 27
#endif
#ifndef EINTR
#define EINTR 4
#endif
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EIO
#define EIO 5
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef ENOSYS
#define ENOSYS 38
#endif
#ifndef ENXIO
#define ENXIO 6
#endif
#ifndef EPERM
#define EPERM 1
#endif

int _stricmp(const char *left, const char *right);
int _strnicmp(const char *left, const char *right, size_t count);
int snprintf(char *destination, size_t destination_size, const char *format, ...);
int fopen_s(FILE **stream, const char *file_name, const char *mode);
int strcpy_s(char *destination, size_t destination_size, const char *source);
int strncpy_s(char *destination, size_t destination_size,
              const char *source, size_t count);
int sscanf_s(const char *buffer, const char *format, ...);

#endif

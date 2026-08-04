/** @file wcrt_compat.h @brief WCRT-backed TinyCC compatibility definitions. */
#ifndef WPM_WCRT_COMPAT_H
#define WPM_WCRT_COMPAT_H

#include <sys/stat.h>
#include <sys/utime.h>
#include <time.h>

/* WCRT uses a 64-bit time_t on x86. Do not let TinyCC's Windows compatibility
 * headers subsequently redeclare it with the platform's legacy width. */
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
#endif

/* TinyCC-oriented dependencies use the POSIX spellings while WCRT exposes
 * the explicit-width Microsoft interfaces. Keep this as an API mapping; the
 * implementations come from WCRT. */
#define stat _stat64
#define utimbuf __utimbuf64
#define utime _utime64

/* Compatibility constants used by bundled dependencies but not yet part of
 * WCRT's public C89 errno surface. WCRT supplies all runtime functions. */
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

#endif

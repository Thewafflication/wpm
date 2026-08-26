#ifndef WPM_WINIOCTL_H
#define WPM_WINIOCTL_H

/* Minimal WinIoCtl definitions used by minizip-ng's symlink inspection. */
#ifndef FSCTL_GET_REPARSE_POINT
#define FSCTL_GET_REPARSE_POINT 0x000900A8UL
#endif
#ifndef MAXIMUM_REPARSE_DATA_BUFFER_SIZE
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE (16U * 1024U)
#endif
#ifndef IO_REPARSE_TAG_SYMLINK
#define IO_REPARSE_TAG_SYMLINK 0xA000000CUL
#endif
#ifndef IsReparseTagMicrosoft
#define IsReparseTagMicrosoft(tag) (((tag) & 0x80000000UL) != 0)
#endif

#endif

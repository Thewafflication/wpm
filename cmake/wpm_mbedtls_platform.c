/* Mbed TLS platform hooks using APIs available on Windows 2000. */
#include <windows.h>
#include <string.h>
#include "mbedtls/platform_util.h"

void mbedtls_platform_zeroize(void* buffer, size_t size)
{
    volatile unsigned char* bytes = (volatile unsigned char*)buffer;
    while (size--) *bytes++ = 0;
}

struct tm* mbedtls_platform_gmtime_r(const mbedtls_time_t* seconds, struct tm* result)
{
    static const int days[] = { 0,31,59,90,120,151,181,212,243,273,304,334 };
    unsigned long long ticks;
    FILETIME file_time;
    SYSTEMTIME system_time;
    int leap;
    if (!seconds || !result || *seconds < -11644473600LL || *seconds > 253402300799LL) return NULL;
    ticks = (unsigned long long)(*seconds + 11644473600LL) * 10000000ULL;
    file_time.dwLowDateTime = (DWORD)ticks;
    file_time.dwHighDateTime = (DWORD)(ticks >> 32);
    if (!FileTimeToSystemTime(&file_time, &system_time)) return NULL;
    memset(result, 0, sizeof(*result));
    result->tm_year = system_time.wYear - 1900;
    result->tm_mon = system_time.wMonth - 1;
    result->tm_mday = system_time.wDay;
    result->tm_hour = system_time.wHour;
    result->tm_min = system_time.wMinute;
    result->tm_sec = system_time.wSecond;
    result->tm_wday = system_time.wDayOfWeek;
    leap = system_time.wYear % 4 == 0 && (system_time.wYear % 100 != 0 || system_time.wYear % 400 == 0);
    result->tm_yday = days[result->tm_mon] + result->tm_mday - 1 + (leap && result->tm_mon > 1);
    return result;
}

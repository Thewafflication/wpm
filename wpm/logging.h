#ifndef WPM_LOGGING_H
#define WPM_LOGGING_H

#include <stdarg.h>

#ifndef va_copy
#define va_copy(destination, source) ((destination) = (source))
#endif

int wpm_log_initialize(void);
void wpm_log_close(void);
int wpm_printf(const char* format, ...);
int wpm_vprintf(const char* format, va_list arguments);

#ifndef WPM_LOGGING_IMPLEMENTATION
#define printf wpm_printf
#define vprintf wpm_vprintf
#endif

#endif

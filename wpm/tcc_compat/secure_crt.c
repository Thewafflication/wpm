#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int fopen_s(FILE **stream, const char *file_name, const char *mode)
{
    if (stream == NULL || file_name == NULL || mode == NULL) return EINVAL;
    *stream = fopen(file_name, mode);
    return *stream == NULL ? errno : 0;
}

int strcpy_s(char *destination, size_t destination_size, const char *source)
{
    size_t source_size;
    if (destination == NULL || destination_size == 0 || source == NULL) return EINVAL;
    source_size = strlen(source) + 1;
    if (source_size > destination_size) {
        destination[0] = '\0';
        return ERANGE;
    }
    memcpy(destination, source, source_size);
    return 0;
}

int strncpy_s(char *destination, size_t destination_size,
              const char *source, size_t count)
{
    size_t copy_size;
    if (destination == NULL || destination_size == 0 || source == NULL) return EINVAL;
    copy_size = strlen(source);
    if (copy_size > count) copy_size = count;
    if (copy_size >= destination_size) {
        destination[0] = '\0';
        return ERANGE;
    }
    memcpy(destination, source, copy_size);
    destination[copy_size] = '\0';
    return 0;
}

int sscanf_s(const char *buffer, const char *format, ...)
{
    unsigned long long value = 0;
    unsigned long long *value_out;
    char *trailing_out;
    const char *cursor = buffer;
    va_list arguments;

    if (buffer == NULL || format == NULL || strcmp(format, "%llu%c") != 0) return 0;
    if (*cursor < '0' || *cursor > '9') return 0;
    while (*cursor >= '0' && *cursor <= '9') {
        unsigned int digit = (unsigned int)(*cursor - '0');
        if (value > (ULLONG_MAX - digit) / 10) return 0;
        value = value * 10 + digit;
        cursor++;
    }

    va_start(arguments, format);
    value_out = va_arg(arguments, unsigned long long *);
    trailing_out = va_arg(arguments, char *);
    (void) va_arg(arguments, int);
    va_end(arguments);
    *value_out = value;
    if (*cursor == '\0') return 1;
    *trailing_out = *cursor;
    return 2;
}

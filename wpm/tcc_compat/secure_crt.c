/** @file secure_crt.c @brief TinyCC secure CRT implementations. */
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef EACCES
#define EACCES 13
#endif

int _strnicmp(const char *left, const char *right, size_t count)
{
    while (count-- > 0) {
        int left_char = tolower((unsigned char)*left++);
        int right_char = tolower((unsigned char)*right++);
        if (left_char != right_char) return left_char - right_char;
        if (left_char == 0) return 0;
    }
    return 0;
}

int _stricmp(const char *left, const char *right)
{
    return _strnicmp(left, right, (size_t)-1);
}

static int append_character(char *destination, size_t size, size_t *length, char value)
{
    if (*length + 1 >= size) return 0;
    destination[(*length)++] = value;
    destination[*length] = '\0';
    return 1;
}

static int append_string(char *destination, size_t size, size_t *length, const char *value)
{
    if (value == NULL) value = "(null)";
    while (*value != '\0') {
        if (!append_character(destination, size, length, *value++)) return 0;
    }
    return 1;
}

static int append_unsigned(char *destination, size_t size, size_t *length,
                           unsigned long long value, unsigned int base,
                           unsigned int width, char padding)
{
    char digits[32];
    unsigned int count = 0;
    do {
        unsigned int digit = (unsigned int)(value % base);
        digits[count++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= base;
    } while (value != 0);
    while (count < width) {
        if (!append_character(destination, size, length, padding)) return 0;
        width--;
    }
    while (count > 0) {
        if (!append_character(destination, size, length, digits[--count])) return 0;
    }
    return 1;
}

int snprintf(char *destination, size_t destination_size, const char *format, ...)
{
    size_t length = 0;
    va_list arguments;
    if (destination == NULL || destination_size == 0 || format == NULL) return -1;
    destination[0] = '\0';
    va_start(arguments, format);
    while (*format != '\0') {
        unsigned int width = 0;
        unsigned int long_count = 0;
        char padding = ' ';
        char conversion;
        if (*format != '%') {
            if (!append_character(destination, destination_size, &length, *format++)) goto overflow;
            continue;
        }
        format++;
        if (*format == '%') {
            if (!append_character(destination, destination_size, &length, *format++)) goto overflow;
            continue;
        }
        if (*format == '0') {
            padding = '0';
            format++;
        }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (unsigned int)(*format++ - '0');
        }
        while (*format == 'l') {
            long_count++;
            format++;
        }
        conversion = *format++;
        if (conversion == 's') {
            if (!append_string(destination, destination_size, &length,
                               va_arg(arguments, const char *))) goto overflow;
        } else if (conversion == 'c') {
            if (!append_character(destination, destination_size, &length,
                                  (char)va_arg(arguments, int))) goto overflow;
        } else if (conversion == 'd') {
            long long signed_value = long_count >= 2 ? va_arg(arguments, long long) :
                (long_count == 1 ? va_arg(arguments, long) : va_arg(arguments, int));
            unsigned long long magnitude;
            if (signed_value < 0) {
                if (!append_character(destination, destination_size, &length, '-')) goto overflow;
                magnitude = (unsigned long long)(-(signed_value + 1)) + 1;
            } else {
                magnitude = (unsigned long long)signed_value;
            }
            if (!append_unsigned(destination, destination_size, &length,
                                 magnitude, 10, width, padding)) goto overflow;
        } else if (conversion == 'u' || conversion == 'x') {
            unsigned long long value = long_count >= 2 ? va_arg(arguments, unsigned long long) :
                (long_count == 1 ? va_arg(arguments, unsigned long) : va_arg(arguments, unsigned int));
            if (!append_unsigned(destination, destination_size, &length, value,
                                 conversion == 'x' ? 16 : 10, width, padding)) goto overflow;
        } else {
            goto overflow;
        }
    }
    va_end(arguments);
    return (int)length;

overflow:
    va_end(arguments);
    destination[destination_size - 1] = '\0';
    return -1;
}

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

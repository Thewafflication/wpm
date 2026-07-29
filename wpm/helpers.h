/** @file helpers.h @brief Shared helper declarations. */
#ifndef WPM_HELPERS_H
#define WPM_HELPERS_H

#include <stdio.h>

/**
 * Test whether a filesystem path names a readable file.
 * @param[in] path Path to test.
 * @return Nonzero when the file can be opened; otherwise zero.
 */
int file_exists(const char* path);

/**
 * Open a file through the compiler-appropriate CRT interface.
 * @param[in] path Path to open.
 * @param[in] mode Standard C file-open mode.
 * @return An owned stream on success; NULL on failure.
 */
FILE* wpm_fopen(const char* path, const char* mode);

/**
 * Read a nonempty environment variable into a caller-owned buffer.
 * @param[in] name Variable name.
 * @param[out] result Destination buffer.
 * @param[in] result_size Capacity of @p result in bytes.
 * @return Nonzero when present and fully copied; otherwise zero.
 */
int wpm_get_environment_variable(const char* name, char* result, size_t result_size);

#endif

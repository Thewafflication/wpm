/** @file hashing.h @brief Open-file digest interface. */
#ifndef WPM_HASHING_H
#define WPM_HASHING_H
#include <stddef.h>

/** Hash a readable Windows file handle as BLAKE2b-256, returning hex text.
 * Reads from the beginning, checks the complete file size, and leaves the
 * handle open at end-of-file. Performs no writes. */
int wpm_hash_open_file(void *input, char *hex, size_t capacity);

#endif

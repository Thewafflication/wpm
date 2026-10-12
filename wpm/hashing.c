/** @file hashing.c @brief Checked open-file BLAKE2b-256 hashing. */
#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include "sodium.h"
#include "hashing.h"

int wpm_hash_open_file(void *input, char *hex, size_t capacity) {
    crypto_generichash_state hash;
    unsigned char buffer[65536], digest[32];
    DWORD count;
    LARGE_INTEGER expected;
    uint64_t received = 0;
    if (!hex || capacity < 65) {
        return 0;
    }
    if (SetFilePointer(input, 0, NULL, FILE_BEGIN) ==
            INVALID_SET_FILE_POINTER ||
        !GetFileSizeEx(input, &expected) || expected.QuadPart < 0 ||
        sodium_init() < 0 || crypto_generichash_init(&hash, NULL, 0, 32)) {
        return 0;
    }
    for (;;) {
        if (!ReadFile(input, buffer, sizeof(buffer), &count, NULL)) {
            return 0;
        }
        if (!count) {
            break;
        }
        if (crypto_generichash_update(&hash, buffer, count)) {
            return 0;
        }
        if (received > (uint64_t)expected.QuadPart ||
            count > (uint64_t)expected.QuadPart - received) {
            return 0;
        }
        received += count;
    }
    return received == (uint64_t)expected.QuadPart &&
           !crypto_generichash_final(&hash, digest, sizeof(digest)) &&
           sodium_bin2hex(hex, capacity, digest, sizeof(digest)) != NULL;
}

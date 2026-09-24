/* Exercise upstream dispatch with synthetic feature masks, then compare every
 * executable kernel with the scalar oracle through the public streaming API.
 * Including the upstream implementation here exposes its private selector for
 * tests only; WPM links the ordinary upstream translation unit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "sodium.h"
#include "blake2.h"

static int test_features;
static int test_has_ssse3(void) { return (test_features & 1) != 0; }
static int test_has_sse41(void) { return (test_features & 2) != 0; }
static int test_has_avx2(void) { return (test_features & 4) != 0; }
#define sodium_runtime_has_ssse3 test_has_ssse3
#define sodium_runtime_has_sse41 test_has_sse41
#define sodium_runtime_has_avx2 test_has_avx2
#include "blake2b-ref.c"
#undef sodium_runtime_has_ssse3
#undef sodium_runtime_has_sse41
#undef sodium_runtime_has_avx2

static unsigned char input[1024 * 1024 + 1];
static unsigned char key[64];

static int digest(size_t size, size_t step, int keyed, size_t output_size,
    unsigned char* output) {
    crypto_generichash_state state;
    size_t offset = 0;
    if (crypto_generichash_init(&state, keyed ? key : NULL,
        keyed ? sizeof(key) : 0, output_size) != 0) return 0;
    while (offset < size) {
        size_t count = size - offset;
        if (count > step) count = step;
        /* Deliberately unaligned input, including block and read boundaries. */
        if (crypto_generichash_update(&state, input + 1 + offset, count) != 0) return 0;
        offset += count;
    }
    return crypto_generichash_final(&state, output, output_size) == 0;
}

static int check_backend(const char* name, blake2b_compress_fn backend) {
    static const size_t sizes[] = {0, 1, 127, 128, 129, 255, 256, 257, 65535, 65536, 65537, 1048576};
    static const size_t steps[] = {1, 127, 128, 129, 8192, 65536};
    unsigned char expected[64], actual[64];
    size_t i, j, output_size;
    int keyed;
    for (keyed = 0; keyed < 2; ++keyed) {
        for (output_size = 32; output_size <= 64; output_size += 32) {
            for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
                blake2b_compress = blake2b_compress_ref;
                if (!digest(sizes[i], 65536, keyed, output_size, expected)) return 0;
                blake2b_compress = backend;
                for (j = 0; j < sizeof(steps) / sizeof(steps[0]); ++j) {
                    if (!digest(sizes[i], steps[j], keyed, output_size, actual) ||
                        memcmp(expected, actual, output_size) != 0) {
                        printf("FAIL %s: size=%lu step=%lu keyed=%d output=%lu\n",
                            name, (unsigned long)sizes[i], (unsigned long)steps[j],
                            keyed, (unsigned long)output_size);
                        return 0;
                    }
                }
            }
        }
    }
    printf("PASS %s: streaming, keyed, empty, boundary and unaligned inputs\n", name);
    return 1;
}

static int benchmark(const char* name, blake2b_compress_fn backend) {
    LARGE_INTEGER frequency, start, finish;
    crypto_generichash_state state;
    unsigned char result[32];
    int i;
    blake2b_compress = backend;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&start)) return 0;
    if (crypto_generichash_init(&state, NULL, 0, sizeof(result)) != 0) return 0;
    for (i = 0; i < 64; ++i) {
        if (crypto_generichash_update(&state, input + 1, 1024 * 1024) != 0) return 0;
    }
    if (crypto_generichash_final(&state, result, sizeof(result)) != 0 ||
        !QueryPerformanceCounter(&finish)) return 0;
    printf("%s: %.1f MiB/s (64 MiB, memory only)\n", name,
        64.0 * (double)frequency.QuadPart / (double)(finish.QuadPart - start.QuadPart));
    return 1;
}

int main(int argc, char** argv) {
    const char* names[4] = {"scalar", "ssse3", "sse4.1", "avx2"};
    blake2b_compress_fn backends[4] = {blake2b_compress_ref, NULL, NULL, NULL};
    int available[4] = {1, 0, 0, 0};
    unsigned char empty[32];
    char hex[65];
    int i, mask;
    int run_benchmark = argc == 2 && strcmp(argv[1], "--benchmark") == 0;
    if (sodium_init() < 0) return 1;
#ifdef WPM_TEST_SIMD
    backends[1] = blake2b_compress_ssse3;
    backends[2] = blake2b_compress_sse41;
    backends[3] = blake2b_compress_avx2;
    available[1] = sodium_runtime_has_ssse3();
    available[2] = sodium_runtime_has_sse41();
    available[3] = sodium_runtime_has_avx2();
#endif
    for (mask = 0; mask < 8; ++mask) {
        blake2b_compress_fn expected = blake2b_compress_ref;
        test_features = mask;
#ifdef WPM_TEST_SIMD
        if (mask & 4) expected = backends[3];
        else if (mask & 2) expected = backends[2];
        else if (mask & 1) expected = backends[1];
#endif
        blake2b_pick_best_implementation();
        /* Never execute synthetic capabilities on unsupported hardware. */
        if (blake2b_compress != expected) { puts("FAIL dispatch priority/fallback"); return 1; }
    }
    puts("PASS dispatch priority and scalar fallback for all feature masks");
    test_features = available[1] | (available[2] << 1) | (available[3] << 2);
    blake2b_pick_best_implementation();
    printf("Host selection: %s\n", available[3] ? names[3] : available[2] ? names[2] : available[1] ? names[1] : names[0]);
    if (crypto_generichash(empty, sizeof(empty), (const unsigned char*)"", 0, NULL, 0) != 0) return 1;
    sodium_bin2hex(hex, sizeof(hex), empty, sizeof(empty));
    if (strcmp(hex, "0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8") != 0) {
        puts("FAIL BLAKE2b-256 empty known-answer vector"); return 1;
    }
    for (i = 0; i < (int)sizeof(input); ++i) input[i] = (unsigned char)(i * 37 + i / 256);
    for (i = 0; i < (int)sizeof(key); ++i) key[i] = (unsigned char)i;
    for (i = 0; i < 4; ++i) {
        if (!available[i]) { printf("SKIP %s: unavailable on this build/CPU/OS\n", names[i]); continue; }
        if (run_benchmark ? !benchmark(names[i], backends[i]) : !check_backend(names[i], backends[i])) return 1;
    }
    return 0;
}

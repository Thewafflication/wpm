/* Compare both dispatch paths through the public zlib-ng API. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wcrt/cpu.h>
#include "zlib-ng.h"

static int use_sse2;
static int test_cpu_has_features(unsigned long flags)
{
    return use_sse2 && flags == WCRT_CPU_SSE2;
}
#define wcrt_cpu_has_features test_cpu_has_features
#include "../cmake/wpm_zlib_dispatch.c"
#undef wcrt_cpu_has_features

static unsigned char input[262145];
static unsigned char packed[2][300000];
static unsigned char output[262147];
static Pos scalar_head[HASH_SIZE + 1], simd_head[HASH_SIZE + 1];
static Pos scalar_prev[32769], simd_prev[32769];

int main(int argc, char **argv)
{
    static const size_t sizes[] = { 0, 1, 15, 16, 17, 255, 256, 257,
        32767, 32768, 32769, 65535, 65536, 262144 };
    static const int levels[] = { 1, 2, 6, 9 };
    unsigned long seed = 1;
    size_t i, n, l;
    int pattern, mode, decode;
    int supported = wcrt_cpu_has_features(WCRT_CPU_SSE2);
    if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
        for (i = 0; i < sizeof(input); i++) input[i] = (unsigned char)(i % 251);
        for (mode = 0; mode <= supported; mode++) {
            clock_t started;
            double encode_time, decode_time;
            size_t packed_size = sizeof(packed[0]);
            use_sse2 = mode;
            started = clock();
            for (i = 0; i < 128; i++) {
                packed_size = sizeof(packed[0]);
                if (zng_compress2(packed[0], &packed_size, input, sizeof(input), 2) != Z_OK) return 1;
            }
            encode_time = (double)(clock() - started) / CLOCKS_PER_SEC;
            started = clock();
            for (i = 0; i < 128; i++) {
                size_t output_size = sizeof(output);
                if (zng_uncompress(output, &output_size, packed[0], packed_size) != Z_OK ||
                    output_size != sizeof(input) || memcmp(output, input, sizeof(input)) != 0) return 1;
            }
            decode_time = (double)(clock() - started) / CLOCKS_PER_SEC;
            printf("%s: compress %.3f s, extract %.3f s (32 MiB, level 2, in memory)\n",
                mode ? "SSE2" : "scalar", encode_time, decode_time);
        }
        return 0;
    }
    if (supported) {
        deflate_state state;
        memset(&state, 0, sizeof(state));
        for (i = 0; i <= HASH_SIZE; i++) scalar_head[i] = simd_head[i] = (Pos)i;
        for (i = 0; i <= 32768; i++) scalar_prev[i] = simd_prev[i] = (Pos)(65535 - i);
        state.w_size = 32768;
        /* Deliberately offset by one Pos to test non-vector-aligned tables. */
        state.head = scalar_head + 1;
        state.prev = scalar_prev + 1;
        use_sse2 = 0;
        slide_hash_c(&state);
        state.head = simd_head + 1;
        state.prev = simd_prev + 1;
        use_sse2 = 1;
        slide_hash_c(&state);
        if (memcmp(scalar_head, simd_head, sizeof(scalar_head)) != 0 ||
            memcmp(scalar_prev, simd_prev, sizeof(scalar_prev)) != 0) return 1;
    }
    /* Exercise overlap distances and short output capacities with canaries. */
    for (mode = 0; mode <= supported; mode++) {
        unsigned left, distance;
        use_sse2 = mode;
        for (left = 0; left <= 64; left++) {
            for (distance = 1; distance <= 32; distance++) {
                unsigned char expected[128], actual[128];
                unsigned j;
                for (j = 0; j < 128; j++) actual[j] = expected[j] = (unsigned char)j;
                for (j = 0; j < left; j++) expected[33 + j] = expected[33 + j - distance];
                if (chunkmemset_safe_c(actual + 33, actual + 33 - distance, 64, left) !=
                    actual + 33 + left || memcmp(actual, expected, 128) != 0) return 1;
            }
        }
    }
    for (pattern = 0; pattern < 3; pattern++) {
        for (i = 0; i < sizeof(input); i++) {
            seed = seed * 1664525UL + 1013904223UL;
            input[i] = pattern == 0 ? (unsigned char)(seed >> 24) :
                pattern == 1 ? (unsigned char)(i % 19) : (unsigned char)'a';
        }
        for (n = 0; n < sizeof(sizes) / sizeof(sizes[0]); n++) {
            for (l = 0; l < sizeof(levels) / sizeof(levels[0]); l++) {
                size_t packed_size[2];
                for (mode = 0; mode <= supported; mode++) {
                    use_sse2 = mode;
                    packed_size[mode] = sizeof(packed[mode]);
                    if (zng_compress2(packed[mode], &packed_size[mode], input + 1,
                        sizes[n], levels[l]) != Z_OK) return 1;
                    for (decode = 0; decode <= supported; decode++) {
                        size_t output_size = sizeof(output) - 2;
                        use_sse2 = decode;
                        memset(output, 0xa5, sizeof(output));
                        if (zng_uncompress(output + 1, &output_size, packed[mode],
                            packed_size[mode]) != Z_OK || output_size != sizes[n] ||
                            memcmp(input + 1, output + 1, sizes[n]) != 0 ||
                            output[0] != 0xa5 || output[sizeof(output) - 1] != 0xa5) {
                            fprintf(stderr, "Round trip failed: pattern=%d size=%u level=%d encode=%d decode=%d\n",
                                pattern, (unsigned)sizes[n], levels[l], mode, decode);
                            return 1;
                        }
                    }
                }
                if (supported && (packed_size[0] != packed_size[1] ||
                    memcmp(packed[0], packed[1], packed_size[0]) != 0)) {
                    fprintf(stderr, "Scalar/SSE2 compressed bytes differ\n");
                    return 1;
                }
            }
        }
    }
    printf("zlib dispatch round trips passed (scalar%s)\n", supported ? " and SSE2" : " only");
    return 0;
}

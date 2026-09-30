/* Benchmark the actual private indexed-file verifier. No production switches.
 * Progress interception measures caller notifications, not worker read events. */
#include <stdio.h>
#include <windows.h>
#include "progress.h"
#include "helpers.h"

static LARGE_INTEGER bench_frequency, bench_start, bench_last;
static double bench_first_ms, bench_max_gap_ms;
static unsigned long bench_adds, bench_renders;
static unsigned long long bench_bytes;
static int bench_buffered;

static FILE* benchmark_fopen(const char* path, const char* mode) {
    FILE* file = wpm_fopen(path, mode);
    if (file && bench_buffered && setvbuf(file, NULL, _IOFBF, 64 * 1024) != 0) {
        fclose(file);
        return NULL;
    }
    return file;
}

static void benchmark_progress_add(wpm_progress* progress, unsigned long long bytes) {
    LARGE_INTEGER now;
    double gap;
    unsigned long previous_render = progress->last_update_ms;
    QueryPerformanceCounter(&now);
    gap = 1000.0 * (double)(now.QuadPart - bench_last.QuadPart) / bench_frequency.QuadPart;
    if (gap > bench_max_gap_ms) bench_max_gap_ms = gap;
    if (!bench_adds) bench_first_ms =
        1000.0 * (double)(now.QuadPart - bench_start.QuadPart) / bench_frequency.QuadPart;
    bench_last = now;
    ++bench_adds;
    bench_bytes += bytes;
    wpm_progress_add(progress, bytes);
    if (progress->last_update_ms != previous_render) ++bench_renders;
}

#define wpm_progress_add benchmark_progress_add
#define wpm_fopen benchmark_fopen
#include "../wpm/archive.c"
#undef wpm_progress_add
#undef wpm_fopen

int main(int argc, char** argv) {
    LARGE_INTEGER finish;
    double elapsed, tail;
    int valid;
    const char* backend = "scalar";
    if (argc != 2 && !(argc == 3 && strcmp(argv[2], "--buffered") == 0)) {
        fprintf(stderr, "Usage: wpm-validation-benchmark <indexed-directory> [--buffered]\n");
        return 2;
    }
    bench_buffered = argc == 3;
    if (!ensure_sodium_ready() || !QueryPerformanceFrequency(&bench_frequency)) return 2;
#ifdef WPM_BLAKE2B_SIMD
    backend = sodium_runtime_has_avx2() ? "avx2" :
        sodium_runtime_has_sse41() ? "sse4.1" : sodium_runtime_has_ssse3() ? "ssse3" : "scalar";
#endif
    QueryPerformanceCounter(&bench_start);
    bench_last = bench_start;
    valid = verify_package_index(argv[1], "benchmark");
    QueryPerformanceCounter(&finish);
    elapsed = 1000.0 * (double)(finish.QuadPart - bench_start.QuadPart) / bench_frequency.QuadPart;
    tail = 1000.0 * (double)(finish.QuadPart - bench_last.QuadPart) / bench_frequency.QuadPart;
    if (tail > bench_max_gap_ms) bench_max_gap_ms = tail;
    printf("BENCH {\"backend\":\"%s\",\"buffered\":%d,\"valid\":%d,\"elapsed_ms\":%.6f,"
        "\"first_progress_ms\":%.6f,\"max_progress_gap_ms\":%.6f,"
        "\"progress_adds\":%lu,\"intermediate_renders\":%lu,\"bytes\":%llu}\n",
        backend, bench_buffered, valid, elapsed, bench_first_ms, bench_max_gap_ms,
        bench_adds, bench_renders, bench_bytes);
    return valid && bench_adds ? 0 : 1;
}

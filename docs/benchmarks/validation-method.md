# Indexed-file validation benchmark

This benchmark compares the shipped scalar and CPU-dispatched BLAKE2b builds
using WPM's actual indexed-file validation implementation. It also measures
how long the caller goes without receiving progress notifications. It is an
opt-in local performance experiment, not a timing threshold in CTest.

## Workloads

The requested sizes are interpreted as binary units:

| Case | Files | Bytes per file | Total bytes |
| --- | ---: | ---: | ---: |
| 1000 x 1 KiB | 1,000 | 1,024 | 1,024,000 |
| 100 x 100 KiB | 100 | 102,400 | 10,240,000 |
| 10 x 10 MiB | 10 | 10,485,760 | 104,857,600 |
| 1 x 1 GiB | 1 | 1,073,741,824 | 1,073,741,824 |

For zero-based file number `i`, byte `j` of its repeated 65,536-byte block is
`(j * 37 + floor(j / 256) + i) mod 256`. Files are ordinary written files,
not sparse files. Python's `hashlib.blake2b(digest_size=32)` independently
generates each expected digest in `.wpm/index.csv` while writing the file.
The pattern is compressible, but compression is outside this benchmark.
The four cases have different total sizes; compare backends within a case,
not elapsed time alone between cases.

## What is timed

The test translation unit includes `wpm/archive.c` to invoke its private
`verify_package_index` directly without adding a production CLI option.
It uses the same WCRT streams, 64 KiB reads, BLAKE2b-256 streaming calls,
up-to-four-worker pool, batches of 16 files, and progress presentation as WPM.
The 64 KiB figure is the application's request size, not a guarantee about
underlying OS reads.

`QueryPerformanceCounter` brackets that function. The interval includes index
size measurement and parsing, thread-pool creation/destruction, file opens,
reads, hash and size comparisons, completeness enumeration, and normal progress
output. It excludes executable startup, sodium initialization, fixture creation,
extraction, Ed25519 authentication, install scripts, and cleanup. A fixture-only
`.wpm/signature.json` containing `{}` triggers the existing completeness branch;
it is not a valid signature, and these directories are not installable packages.

The benchmark wraps only the caller's `wpm_progress_add` calls, recording the
time of the first notification, largest gap between notifications (including
start-to-first and last-to-return), notification count, and intermediate render
count. QPC instrumentation adds a small constant cost per file notification to
both builds. Notifications are **not** worker read events or measured terminal
frames. The runner captures stdout, so the normal redirected-output throttle
of 2,000 ms applies; a native interactive console uses 100 ms. Start and finish
renders are excluded from the intermediate-render count.

The Python runner also records `GetProcessIoCounters` read-operation and
read-byte totals for the child process. These cover its entire lifetime,
including startup and metadata reads, rather than only the timed function.
They are logical process I/O, not physical SSD traffic or cache misses.

## Reproduce on Windows x64

Use the normal WPM TinyCC/WCRT/Clang build prerequisites. From the repository
root, run this PowerShell block. Separate binary and library output directories
prevent the two configurations from overwriting one another or normal builds.

```powershell
$repo = (Get-Location).Path.Replace('\', '/')
foreach ($variant in @('scalar', 'simd')) {
    $simd = if ($variant -eq 'simd') { 'ON' } else { 'OFF' }
    $build = "out/build/validation-$variant"
    cmake --preset x64-release -B $build `
        -DWPM_BUILD_VALIDATION_BENCHMARK=ON `
        "-DWPM_BLAKE2B_SIMD=$simd" `
        -DWPM_RUN_TESTS_AFTER_BUILD=OFF -DWPM_BUILD_TEST_REPORTS=OFF `
        "-DWPM_BIN_DIR=$repo/$build/bin" `
        "-DWPM_THIRD_PARTY_BIN_DIR=$repo/$build/lib"
    if ($LASTEXITCODE) { throw 'Configure failed' }
    cmake --build $build --target wpm-validation-benchmark wpm-blake2b-test -j 6
    if ($LASTEXITCODE) { throw 'Build failed' }
    ctest --test-dir $build -R '^blake2b-simd$' --output-on-failure
    if ($LASTEXITCODE) { throw 'Correctness test failed' }
}
$run = 'out/benchmarks/validation-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
python tests/benchmark-validation.py `
    --scalar out/build/validation-scalar/wpm/wpm-validation-benchmark.exe `
    --simd out/build/validation-simd/wpm/wpm-validation-benchmark.exe `
    --work-dir "$run-small" --cases 1000x1KiB 100x100KiB --rounds 10 --warmups 2
if ($LASTEXITCODE) { throw 'Benchmark failed' }
python tests/benchmark-validation.py `
    --scalar out/build/validation-scalar/wpm/wpm-validation-benchmark.exe `
    --simd out/build/validation-simd/wpm/wpm-validation-benchmark.exe `
    --work-dir "$run-large" --cases 10x10MiB 1x1GiB --rounds 1 --warmups 0
if ($LASTEXITCODE) { throw 'Benchmark failed' }
```

On the WCRT DLL measured in the local report, byte-at-a-time reads make the
large cases exceptionally slow. The commands above use one run per backend
for those cases to limit the expense. After obtaining effective bulk reads,
increase their repetitions and warmups to establish variability.

The work directory must not already exist. Allow about 1.2 GiB for generated
payloads in addition to build outputs. The runner retains its fixtures and
writes `samples.csv`, `summary.json`, and `metadata.json` beside them. Metadata
records executable SHA-256 hashes, UTC timestamps, Python/OS versions, run
parameters, and negative-control outcomes. Record CPU, memory, storage, compiler
versions, source revision and local modifications with any published report.

For expensive cases, `--cases 10x10MiB 1x1GiB --rounds 1 --warmups 0` runs
only the larger workloads once per backend. Such results are exploratory
single samples, not medians with established variability. The per-child
timeout defaults to 3,600 seconds and can be changed with `--timeout`.
Samples are checkpointed after every completed measured run.

`--buffered` is an explicitly experimental control. It intercepts the
benchmark translation unit's file opens and requests 64 KiB full stdio
buffering with `setvbuf`; it does not alter production WPM or replace its
read implementation. This option is not assumed to improve a runtime that
does not implement effective buffered reads. Keep its results separate.

Before timing, each build must accept a valid control and reject same-size
content corruption, truncation, and an unindexed extra file. Every measured
run must succeed, account for exactly the expected bytes/files, and select the
requested scalar or available SIMD backend; a scalar fallback in the SIMD
executable fails the comparison rather than masquerading as acceleration.

Each case gets two untimed warmups per backend and ten measured runs per
backend by default. Order alternates scalar/SIMD then SIMD/scalar, with each run in a fresh
process and no concurrent benchmark processes. This is buffered, warm-cache
validation: the runner does not flush or evict the Windows file cache.
It reports median, minimum and maximum elapsed time, aggregate MiB/s, the ratio
of scalar to SIMD medians, and progress gaps. Preserve outliers and report wide
ranges rather than discarding inconvenient samples.

## Interpretation limits

These results compare the actual build configurations, including TinyCC scalar
code versus Clang `-O3` SIMD kernels; they do not isolate instruction-set gains
with the compiler held constant. Background work, scheduling, antivirus,
frequency changes, cache state and filesystem costs can dominate small files.
Do not infer cold-storage throughput, installation time, other architectures,
or universal speedups from this experiment. A progress gap identifies absent
caller notifications; determining an exact worker-level stall requires further
profiling. Console smoothness also needs interactive testing after any progress
implementation change.

## References

- [BLAKE2b acceleration](../blake2b-simd.md)
- [2026-09-29 local report](validation-2026-09-29.md)
- [WCRT 1.3.1 rerun](validation-wcrt-1.3.1-2026-09-30.md)
- [ADR-0015](../adr-0015-validation-performance-and-progress.md)

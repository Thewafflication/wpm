## Problem

WCRT 1.3.0 turns bulk binary `fread` calls into one Windows `ReadFile` operation per byte. This dominates WPM's BLAKE2b file validation even though the application requests 64 KiB at a time. Explicit `setvbuf(..., _IOFBF, 65536)` did not remove the bottleneck.

This is a throughput issue in the runtime's read path. WPM's separate choice to update progress only after a batch completes amplifies the visible pauses; fixing that UI behavior will not remove the I/O cost described here.

## Source evidence

Verified against release **1.3.0** and current `master`, both resolving to `37b125069c5b0552aac6faa5ba7eefac6a072e91` when inspected:

- [`fread`, src/stdio.c:784–800](https://github.com/Thewafflication/wcrt/blob/37b125069c5b0552aac6faa5ba7eefac6a072e91/src/stdio.c#L784-L800) loops over `size * count` and calls `fgetc` for every byte.
- [`fgetc`, src/stdio.c:627–653](https://github.com/Thewafflication/wcrt/blob/37b125069c5b0552aac6faa5ba7eefac6a072e91/src/stdio.c#L627-L653) calls `__wcrt_file_read(stream, &character, 1, &transferred)`.
- [`__wcrt_file_read`, src/platform/windows/file.c:274–285](https://github.com/Thewafflication/wcrt/blob/37b125069c5b0552aac6faa5ba7eefac6a072e91/src/platform/windows/file.c#L274-L285) passes that request size directly to Windows `ReadFile`.
- [`setvbuf`, src/stdio.c:610–620](https://github.com/Thewafflication/wcrt/blob/37b125069c5b0552aac6faa5ba7eefac6a072e91/src/stdio.c#L610-L620) stores buffer configuration, but the read path above does not use it for bulk refills.

Disassembly of the locally tested DLL matches this call chain. DLL SHA-256:
`bb77c9a17a25a7162eb17e052a07082ee5aed98de6cdf5d5b73deb90c42e2b93`.

## Measured impact

Local Windows x64 Release builds of WPM at `7ae159858221841da33e58bef3d604dd18d63e7d`, toggling only BLAKE2b SIMD availability. The host selects AVX2 in the SIMD build. Validation uses the existing verifier, up to four workers, 16-file batches, 64 KiB application buffers, digest/size checks, and index completeness checks. No extraction, installation, file generation, process startup, or sodium initialization is included in the validation timer.

| Payload | Scalar validation | AVX2 validation | Samples per backend |
| --- | ---: | ---: | ---: |
| 1,000 × 1 KiB | 2.834 s median (2.431–7.795) | 3.021 s median (2.463–8.304) | 10 |
| 100 × 100 KiB | 14.607 s median (13.191–18.209) | 14.089 s median (13.083–18.110) | 10 |
| 10 × 10 MiB | 368.459 s | 245.136 s | 1 |
| 1 × 1 GiB | 3,336.435 s (55.6 min), passed | Did not complete within the 3,600 s process timeout | 1 attempt |

The expensive large cases used one scalar-first attempt per backend, with no extra warmups after fixture generation. Small cases had two warmups per backend and alternated execution order. No OS cache eviction, antivirus exclusions, or background-process isolation was used. These are not controlled estimates of pure SIMD speedup; ranges and the large-case single samples need that qualification.

Windows `GetProcessIoCounters` provides stronger evidence for this issue than timing alone:

| Completed run | Payload bytes | Process read operations | Process read bytes |
| --- | ---: | ---: | ---: |
| 10 × 10 MiB, scalar | 104,857,600 | 104,860,614 | 104,867,456 |
| 10 × 10 MiB, AVX2 | 104,857,600 | 104,860,614 | 104,866,993 |
| 1 × 1 GiB, scalar | 1,073,741,824 | 1,073,742,216 | 1,073,749,067 |

These counters cover the whole child process, including startup and metadata, and represent logical I/O, not physical SSD traffic. Approximately one read operation occurs per payload byte.

A separate valid 100 × 100 KiB AVX2 run that requested `setvbuf(file, NULL, _IOFBF, 65536)` at each open took **14.695 s**, within the default-path range. This was a single diagnostic control, not a repeated buffering comparison.

## Reproduction pattern

Generate a modest binary file outside WCRT so fixture creation is not part of the read measurement, for example with Python:

```python
from pathlib import Path
block = bytes((j * 37 + j // 256) % 256 for j in range(65536))
with Path("payload.bin").open("wb") as f:
    for _ in range(160):  # 10 MiB; no need to start with 1 GiB
        f.write(block)
```

In an executable linked to WCRT 1.3.0, exercise the same binary-read pattern used by WPM:

```c
unsigned char buffer[64 * 1024];
size_t count;
unsigned long long total = 0;
FILE *file = fopen("payload.bin", "rb");
/* Check fopen, and optionally request _IOFBF with setvbuf before reading. */
while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) {
    total += count;
}
/* Check ferror, fclose and total == 10485760. */
```

Measure read-operation and read-byte deltas with `GetProcessIoCounters` or Windows process performance counters. The application makes approximately 160 nonempty `fread` calls, but the current implementation issues approximately 10,485,760 underlying one-byte reads. The table above was measured with WPM's real validation path; this shorter loop is the isolated reproduction pattern, not a separately timed dataset in that table.

## Expected behavior / suggested acceptance checks

Provide effective bulk binary reads and/or stream-buffer refills so read-operation count scales with requested chunks, rather than payload bytes. Preserve complete-element return values, partial EOF handling, `ferror`/`feof`, `ungetc`, stream orientation, seeking, text-mode translation, and supported Windows versions.

Suggested regression coverage: default binary `fread` and explicit `_IOFBF`, empty files, a short final chunk, element sizes greater than one, pushed-back bytes, injected read failures, and concurrent distinct streams. Include an operation-count assertion or instrumented backend check instead of a fragile wall-clock threshold. Then rerun WPM's scalar/AVX2 comparison after this bottleneck is removed.

## Environment

- Intel Core i7-12700H, 14 cores / 20 logical processors; approximately 31.7 GiB visible RAM.
- Windows 11 Pro for Workstations, build 26200; Balanced power policy.
- Samsung MZAL41T0HBLB-00BL2 NVMe SSD, NTFS.
- WCRT 1.3.0; TinyCC `0.9.28-rc.1448+72495402`; Clang 22.1.3 for BLAKE2b SIMD kernels only.
- All completed retained WPM samples passed digest, size and completeness validation. Both builds rejected same-size corruption, truncation and unindexed-file controls and passed the existing BLAKE2b correctness test.

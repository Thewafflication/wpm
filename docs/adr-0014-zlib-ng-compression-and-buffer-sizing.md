# ADR-0014: zlib-ng Compression and Buffer Sizing

**Status:** Accepted

**Date:** 2026-08-26

**Relationship:** Supplements ADR-0009. It selects archive-codec defaults after
the removal of miniz without changing the package format or installation safety
model defined by ADR-0003.

## Context

WPM uses minizip-ng for ZIP and ZIP64 container handling and zlib-ng for raw
DEFLATE compression and decompression. Large executable packages exposed two
tuning questions: whether streaming buffers should grow beyond 1 MiB, and
whether compression should increase from zlib-ng level 1 after the initial
performance work.

Larger buffers can reduce calls between WPM, minizip-ng, zlib-ng, and Windows,
but they increase per-operation memory. Higher compression levels reduce
retained and transferred archive size, but package builds take longer. The
defaults need to balance those costs rather than maximize only one metric.

## Decision Drivers

- A package containing a 40 MiB executable should build in a few seconds.
- Installation and extraction should remain sub-second on the benchmark host.
- A modest build-time increase is acceptable for a meaningful archive-size
  reduction.
- Extra memory must produce a measurable benefit.
- Builders with different priorities need validated tuning controls.
- The benchmark method and its limitations must be reproducible.

## Test Method

The benchmark package contained `.wpm/package.txt` and one 41,943,040-byte
`payload.exe`. The payload was the first 40 MiB of
`tools/.cache/pandoc/pandoc.exe`, providing the same executable input for every
variant. Package metadata selected name `bench-buffer-level`, version `1.0.0`,
architecture `any`, and `debug=false`.

The benchmark used the x64 Release build produced by TinyCC
0.9.28-rc.1444+9a4be30f and WCRT 1.0.0, with zlib-ng 2.3.3 and minizip-ng 4.2.2.
It ran on Windows build 26200.9168 on a 12th Gen Intel Core i7-12700H with 20
logical processors. Absolute timings are host-specific.

Each timing point followed this sequence:

1. Configure the existing Release build with the selected
   `WPM_ZLIB_NG_COMPRESSION_LEVEL` and `WPM_ZLIB_NG_BUFFER_KIB` values.
2. Rebuild the `wpm` target.
3. Delete the prior output archive.
4. Measure `wpm build <source> <output> --no-index` five times.
5. Measure `wpm install <archive> --allow-unsigned` five times, assigning a new
   `WPM_DATA_DIR` to every run.
6. Sort each group of five elapsed times and report its median. Record the
   archive byte length after the final build.

`--no-index` excludes package-index hashing from the compression measurement.
The install measurement is end-to-end extraction, package validation, and
archive retention; it is not a microbenchmark of `inflate` alone. The archive
was deleted before each build and install roots were unique, but filesystem
cache, antivirus, and other host activity were not disabled. Variants ran in
ascending order, so small differences should be treated as noise unless they
also show a clear resource or trend difference.

The compression-level sweep held the buffer at 1 MiB and tested levels 1
through 5. Level 2 was then selected as the modest compression candidate. The
buffer sweep held level 2 and tested 32 KiB, 64 KiB, 256 KiB, 1 MiB, 4 MiB,
16 MiB, and 32 MiB.

Representative peak working-set measurements used one build and one install at
32 KiB, 1 MiB, 16 MiB, and 32 MiB. WPM was started as a separate hidden process
and its working set was polled every 10 milliseconds. These are approximate
single-run peaks, not five-run medians, and may miss shorter-lived allocations.

The essential reproduction commands for one variant are:

```powershell
cmake -S . -B out/build/perf2-x64-release `
  -DWPM_ZLIB_NG_BUFFER_KIB=1024 `
  -DWPM_ZLIB_NG_COMPRESSION_LEVEL=2
cmake --build out/build/perf2-x64-release --target wpm
bin/perf2-x64/Release/wpm.exe build <source> <output> --no-index
$env:WPM_DATA_DIR = <new-unique-directory>
bin/perf2-x64/Release/wpm.exe install <archive> --allow-unsigned
```

## Results

### Compression-level sweep at 1 MiB

| Level | Build median (s) | Install median (s) | Archive bytes |
|---:|---:|---:|---:|
| 1 | 1.3984 | 0.7064 | 13,649,635 |
| 2 | 2.0475 | 0.6727 | 11,324,321 |
| 3 | 2.8577 | 0.6506 | 10,455,427 |
| 4 | 3.3228 | 0.6410 | 10,009,947 |
| 5 | 3.4571 | 0.6048 | 9,620,803 |

Level 2 reduced the archive by 2,325,314 bytes, or 17.0 percent, relative to
level 1. Its median build time increased by 0.6491 seconds. Levels 3 through 5
continued reducing size but no longer represented a modest compression
increase for this input.

### Buffer sweep at level 2

| Buffer | Build median (s) | Install median (s) | Archive bytes |
|---:|---:|---:|---:|
| 32 KiB | 2.0482 | 0.8648 | 11,324,321 |
| 64 KiB | 2.0141 | 0.8019 | 11,324,321 |
| 256 KiB | 1.9816 | 0.6965 | 11,324,321 |
| 1 MiB | 1.9758 | 0.6753 | 11,324,321 |
| 4 MiB | 2.0216 | 0.6658 | 11,324,321 |
| 16 MiB | 2.0150 | 0.6535 | 11,324,321 |
| 32 MiB | 2.0555 | 0.6983 | 11,324,321 |

Buffer size did not change the DEFLATE output. Performance largely plateaued
between 256 KiB and 16 MiB. Compared with 1 MiB, the 16 MiB buffer saved 0.0218
seconds during installation, while 32 MiB was slower for both measured phases.

### Representative peak working set

| Buffer | Build peak (MiB) | Install peak (MiB) |
|---:|---:|---:|
| 32 KiB | 19.18 | 8.13 |
| 1 MiB | 20.01 | 10.00 |
| 16 MiB | 40.37 | 34.87 |
| 32 MiB | 90.12 | 50.86 |

The small timing improvement at 16 MiB did not justify approximately 25 MiB
more peak install memory. A 32 MiB buffer increased peak memory further and
regressed elapsed time.

## Considered Options

1. Retain level 1 and the 1 MiB buffer for minimum package-build latency.
2. Select level 2 and the 1 MiB buffer as the balanced default.
3. Select level 3 or higher to prioritize archive size.
4. Select a 16 MiB buffer to minimize the measured installation median.
5. Select a 32 MiB buffer to use more available machine memory.
6. Make both settings fixed and require source edits for future tuning.

## Decision

WPM SHALL default to zlib-ng compression level 2 and a 1 MiB streaming buffer
for compression and decompression.

The build SHALL expose `WPM_ZLIB_NG_COMPRESSION_LEVEL` as an integer from 1
through 9 and `WPM_ZLIB_NG_BUFFER_KIB` as an integer from 32 through 32768.
Invalid values SHALL fail CMake configuration. Release builds use the defaults
unless a builder deliberately overrides them.

Future default changes require a representative executable payload, at least
five timing runs per point, archive-size reporting, and memory evidence when a
larger buffer is proposed. Results must distinguish codec-only measurements
from end-to-end WPM commands.

## Rationale

Level 2 provides a material size reduction while keeping the 40 MiB executable
build near two seconds. Level 1 leaves more than 2.3 MiB of avoidable archive
size, while levels 3 through 5 add progressively more build latency.

The 1 MiB buffer is at the performance plateau, produced the lowest measured
build median in the buffer sweep, and keeps peak working set close to the small
buffer cases. The 16 MiB result is too small to distinguish from ordinary host
noise relative to its memory cost. The 32 MiB result demonstrates that using
more memory does not itself improve throughput.

## Consequences

Archives built from compressible inputs may be smaller than level-1 archives,
while package creation takes somewhat longer. Extraction may improve because
less compressed data is read and inflated.

The default does not minimize archive size or peak memory independently. Build
operators can choose another validated setting for controlled workloads without
editing source. Buffer changes do not alter archive bytes for the measured
input, but compression-level changes do.

The selected configuration passed all 15 x64 integration tests, the C99 lint,
and x86 and ARM64 Release builds. TC-0003 also verified that PowerShell could
expand the generated archive.

## References

- ADR-0003 and ADR-0009
- REQ-0003 and REQ-0004
- TC-0003 and TC-0004
- `docs/usage.md`
- `wpm/archive.c`


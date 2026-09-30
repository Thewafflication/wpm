# WPM 1.2.6 validation performance release

## Change and impact

WPM 1.2.6 requires and packages WCRT 1.3.1, which restores effective bulk
binary reads. WCRT 1.3.0 implemented `fread` through one-byte `fgetc` calls,
causing one Windows read operation per payload byte and masking BLAKE2b SIMD
throughput during package validation.

On the local x64 benchmark host, WCRT 1.3.1 reduced a 1 GiB scalar validation
from a 55.6-minute sample to a 20.8-second median. AVX2, which previously did
not complete within a 60-minute timeout, completed in a 4.6-second median.
Process read operations fell from approximately 1.074 billion to 16,775. For
100 MiB and 1 GiB payloads, AVX2 was 4.5–4.9 times faster than scalar.

The release adds an opt-in benchmark target and reproducible runner for four
file distributions: 1,000 × 1 KiB, 100 × 100 KiB, 10 × 10 MiB, and 1 × 1 GiB.
Raw samples, host/build metadata, correctness controls, and the before/after
reports are retained with the engineering documentation. The benchmark is not
part of ordinary timing-gated CI and does not add a production diagnostic
switch.

## Progress behavior

The runtime correction improves elapsed time but does not change progress
granularity. WPM currently reports validation bytes after a complete batch of
up to 16 files. A single 1 GiB file therefore provides no intermediate byte
notifications for approximately 4.6 seconds with AVX2 on the benchmark host,
and longer on scalar or slower systems. ADR-0015 retains caller-rendered worker
progress as a separate follow-up.

## Verification and release criteria

Both scalar and SIMD builds passed the BLAKE2b correctness test. The benchmark
accepted valid fixtures and rejected same-size corruption, truncation, and an
unindexed file. Ten measured runs per backend completed for every workload with
WCRT 1.3.1. The C99 lint and benchmark-runner syntax checks passed.

The tag-triggered Release workflow must pass x86, x64, and ARM64 Debug
verification, Release builds, signed package creation, previous-release upgrade
tests, and controlled documentation publication. Published packages must carry
the WCRT 1.3.1-or-newer sidecar selected by the release environment.

## References

- `docs/benchmarks/validation-wcrt-1.3.1-2026-09-30.md`
- `docs/benchmarks/validation-method.md`
- `docs/adr-0015-validation-performance-and-progress.md`
- [WCRT issue #5](https://github.com/Thewafflication/wcrt/issues/5)

# Local BLAKE2b validation performance, 2026-09-29

The completed small-file measurements do not show a reliable end-to-end SIMD
gain on this machine's current runtime. They expose a much larger issue:
the local WCRT 1.3.0 DLL implements `fread` as a per-byte `fgetc` loop with
one-byte underlying reads. This makes file I/O overhead dominate the hash
comparison. Progress also waits for a whole batch of files to finish, so a
faster hash kernel alone cannot make the progress bar smooth.

**Follow-up:** [WCRT issue #5](https://github.com/Thewafflication/wcrt/issues/5)
was filed on 2026-09-30 with the source-level cause, measurements and a small
reproduction pattern. Measurements began on 2026-09-29 local time.
The issue was fixed in WCRT 1.3.1; see the
[completed rerun](validation-wcrt-1.3.1-2026-09-30.md).

## Host and build

| Item | Configuration |
| --- | --- |
| CPU | Intel Core i7-12700H, 14 physical cores, 20 logical processors |
| RAM | 33,272,312 KiB visible, approximately 31.7 GiB |
| OS | Windows 11 Pro for Workstations, 10.0.26200 |
| Storage | Samsung MZAL41T0HBLB-00BL2, 1 TB NVMe SSD |
| Power policy | Balanced; unchanged for this experiment |
| WPM base revision | `7ae159858221841da33e58bef3d604dd18d63e7d` |
| Local changes | Opt-in benchmark target/harness, runner, and documentation; production verifier unchanged |
| Build | x64 Release, TinyCC `0.9.28-rc.1448+72495402`, WCRT 1.3.0 |
| SIMD compiler | Clang 22.1.3, upstream kernels at `-O3` |
| libsodium | 1.0.22, commit `77e1ce5d6dee871c49ef211222ba18ef0c486bda` |
| Comparison | `WPM_BLAKE2B_SIMD=OFF` versus `ON`; host dispatch selects AVX2 |
| Orchestration | Python 3.12.3; fresh child process per sample; captured stdout |

The scalar and SIMD builds use separate output/library directories and the same
runtime DLL. This is a comparison of WPM's actual configurations, including
different compilers for scalar and SIMD kernels, not an instruction-set-only
microbenchmark. It is not a comparison against an older released WPM binary.

## Procedure and scope

The [reproduction procedure](validation-method.md) documents the deterministic
pattern, exact byte counts, build commands and timing boundaries. All sizes
below use KiB/MiB/GiB, interpreting the requested KB/MB/GB as binary units.

The timer surrounds the existing indexed-file verifier: index reads, pool
setup, file opens, 64 KiB application read requests, digest and size checks,
progress output, and completeness enumeration. Generation, process startup,
sodium initialization, archive extraction, Ed25519 authentication, installation
and cleanup are excluded. Expected BLAKE2b-256 digests come independently from
Python `hashlib`. A synthetic signature-presence marker exercises completeness
checking without pretending the fixtures are signed packages.

The first two cases completed ten measured runs per backend after two warmups
per backend, alternating execution order. The unexpectedly high I/O cost led
to stopping that suite after the two completed cases and restarting the larger
cases with one measured run per backend and no additional warmup. Generated
files had just been written; Windows file cache was never evicted. The large
cases therefore have much weaker statistical evidence and fixed scalar-first
order. No completed small-file samples were discarded.

## Results

Times are validation-only. Small-case values are medians, with observed ranges
in parentheses. Large-case values are single samples. A speedup above 1 means
SIMD was faster; small differences inside broad overlapping ranges are not
evidence of a stable advantage.

| Workload | Total payload | Scalar seconds | AVX2 seconds | Scalar / AVX2 | Samples per backend |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1,000 x 1 KiB | 0.977 MiB | 2.834 (2.431–7.795) | 3.021 (2.463–8.304) | 0.94x | 10 |
| 100 x 100 KiB | 9.766 MiB | 14.607 (13.191–18.209) | 14.089 (13.083–18.110) | 1.04x | 10 |
| 10 x 10 MiB | 100 MiB | 368.459 | 245.136 | 1.50x | 1 |
| 1 x 1 GiB | 1,024 MiB | 3,336.435 (55.6 min) | 3,600 s process timeout | Not established | 1 attempt |

The small-file distributions overlap widely. The 1,000-file AVX2 median was
about 6.6% slower and the 100-file median about 3.5% faster; neither should be
treated as a universal SIMD regression or improvement.

The 100 MiB single-run ratio is 1.50x, but its 123-second difference cannot be
assigned entirely to SIMD: both runs are dominated by tiny reads, the run
order was fixed, and there is no repeatability estimate for this case.

The 1 GiB scalar run completed and validated successfully. AVX2 did not
complete within the runner's 3,600-second child-process timeout and was
terminated by the runner. That limit includes process startup, unlike the
completed validation timers. It is a censored result, not a measured AVX2
validation duration or proof of a hash-kernel regression. No final digest,
byte-accounting record or process I/O total was retained for the timed-out
attempt. The suite exited nonzero; its three completed large-case samples
were checkpointed, but the success-only `summary.json` was not generated.

## Why progress stutters

`verify_file_task` hashes a file without notifying progress. `verify_file_batch`
submits up to 16 files to at most four workers, waits for the entire batch, then
credits each file's full size in a burst. Rendering is rate-limited to 100 ms
in a native console and 2,000 ms with redirected stdout, but neither timer
drives rendering while the caller is blocked.

| Workload | Scalar median first notification | AVX2 median first notification | Scalar median longest gap | AVX2 median longest gap |
| --- | ---: | ---: | ---: | ---: |
| 1,000 x 1 KiB | 335 ms | 419 ms | 343 ms | 433 ms |
| 100 x 100 KiB | 2,056 ms | 2,267 ms | 2,750 ms | 2,537 ms |
| 10 x 10 MiB (single samples) | 368,443 ms | 245,125 ms | 368,443 ms | 245,125 ms |
| 1 x 1 GiB (single sample) | 3,336,431 ms | No completed record | 3,336,431 ms | No completed record |

The 1,000-file case has 63 batches; the 100-file case has seven. Ten large files
fit into one batch, and a single 1 GiB file obviously does too. In those cases
the implementation supplies no byte-progress notifications until all payload
hashing has finished. These are caller-notification measurements and source
analysis, not a captured interactive-console frame-rate test. They explain a
mechanism for the reported stutter but do not establish when it first appeared.

## Runtime I/O finding

During the 100 x 100 KiB case, Windows reported approximately 658,000–834,000
read operations per second and essentially the same number of bytes per
second. This is logical process I/O, not physical SSD throughput.

Disassembly of the exact copied WCRT DLL confirms that `fread` loops over
`size * count`, calls `fgetc` for every byte, and that `fgetc` passes a request
size of one to `__wcrt_file_read`. `setvbuf` stores configuration fields, but
this read path still reads one byte at a time. The DLL SHA-256 is
`bb77c9a17a25a7162eb17e052a07082ee5aed98de6cdf5d5b73deb90c42e2b93`.
The [saved disassembly](data/validation-2026-09-29/wcrt-stdio-disassembly.txt)
provides the binary evidence, scoped to this local DLL rather than all versions
of WCRT.

Process-lifetime counters from the completed large runs reinforce this:

| Run | Payload bytes | Read operations | Read bytes |
| --- | ---: | ---: | ---: |
| 100 MiB scalar | 104,857,600 | 104,860,614 | 104,867,456 |
| 100 MiB AVX2 | 104,857,600 | 104,860,614 | 104,866,993 |
| 1 GiB scalar | 1,073,741,824 | 1,073,742,216 | 1,073,749,067 |

After measurement, the same implementation was verified in the WCRT 1.3.0
release source and current `master`, both at
`37b125069c5b0552aac6faa5ba7eefac6a072e91`. The upstream issue links the exact
`fread`, `fgetc`, `setvbuf`, and Windows file-reader lines.

A benchmark-only `--buffered` control requested `_IOFBF` with 64 KiB at each
open. The 100 x 100 KiB AVX2 control still took **14.695 seconds**, verified all
10,240,000 bytes, and had a 2.538-second maximum notification gap. This single
control does not establish an exact buffering speed ratio, but it did not
remove the observed bottleneck. No production buffering change was made.

A subsequent single run of the existing `wpm-blake2b-test --benchmark` isolated
hashing over 64 MiB in memory: scalar **13.2 MiB/s**, SSSE3 **52.4 MiB/s**,
SSE4.1 **64.5 MiB/s**, and AVX2 **95.8 MiB/s**. This diagnostic shows the AVX2
kernel was about 7.3x faster in memory on this run; it is not a prediction of
file-validation speedup and uses a different workload from the tables above.

## Verification and limitations

Both builds passed the existing `blake2b-simd` CTest test. The SIMD build's test
covers host-supported scalar, SSSE3, SSE4.1 and AVX2 kernels and synthetic
dispatch combinations. Both benchmark configurations accepted valid fixtures
and rejected same-size corruption, truncation and an unindexed file before
measurement. Every retained successful sample checks the expected byte and
file counts in addition to every digest.
The runner also passed Python syntax validation, and the repository C99 lint
passed. The 1 GiB AVX2 timeout is the explicit incomplete result above, not a
successful validation or correctness failure.

This was a local workstation experiment, with no cache eviction, antivirus
exclusions, frequency pinning or background-process isolation. The cache state,
scheduler and Balanced power policy can contribute variance. Large single
samples cannot quantify repeatability or small percentage differences. The
current runtime's extreme read overhead masks hash-kernel throughput; these
numbers should not be extrapolated to a runtime with bulk reads, cold-disk
performance, installation time, x86 or ARM64. The benchmark adds QPC calls per
file progress notification equally to both variants.

## Follow-up and evidence

The proposed [ADR-0015](../adr-0015-validation-performance-and-progress.md)
links this report and separates two changes: effective bulk binary reads, and
caller-rendered progress while workers are still hashing. Keep SIMD dispatch;
rerun these workloads after fixing the I/O path before tuning worker counts.
Treat bytes processed and successful validation as separate states.

- [Small-case raw samples](data/validation-2026-09-29/small-samples.csv)
- [Completed large-case samples](data/validation-2026-09-29/large-samples.csv)
- [Large-run launch metadata and executable hashes](data/validation-2026-09-29/large-metadata.json)
- [Timeout outcome](data/validation-2026-09-29/large-outcome.json)
- [Sampled scalar process counters](data/validation-2026-09-29/progress-observations.csv)
- [Host and initial source hashes](data/validation-2026-09-29/machine.json)
- [Harness revision and run provenance](data/validation-2026-09-29/provenance.json)
- [Explicit buffering control](data/validation-2026-09-29/buffering-control.json)
- [Procedure and reproduction commands](validation-method.md)
- [Benchmark runner](../../tests/benchmark-validation.py)
- [Private-verifier harness](../../tests/validation-benchmark.c)
- [Filed WCRT issue #5](https://github.com/Thewafflication/wcrt/issues/5)

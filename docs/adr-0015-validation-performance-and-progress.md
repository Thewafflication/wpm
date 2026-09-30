# ADR-0015: Validation performance and progress reporting

**Status:** Proposed

**Date:** 2026-09-29

**Relationship:** Supplements [ADR-0009](adr-0009-package-installation-performance.md)
and the codec decisions in [ADR-0014](adr-0014-zlib-ng-compression-and-buffer-sizing.md).

## Context

CPU-dispatched BLAKE2b can shorten validation while progress still appears to
pause and jump. Throughput and progress responsiveness need separate evidence.
The current verifier reads in 64 KiB chunks on up to four workers, submits at
most 16 files at a time, waits for the complete batch, and only then credits
each file's bytes to progress. The presentation throttle cannot display bytes
that the caller has not yet reported. One large file therefore has no
intermediate byte notifications; ten files also fit entirely in one batch.

The [local benchmark report](benchmarks/validation-2026-09-29.md) compares scalar
and AVX2 on the requested file-size distributions. The
[reproducible method](benchmarks/validation-method.md) measures the existing
validation function directly, including file I/O, batching, progress and
completeness checks. This is broader than an in-memory hash benchmark and
narrower than an installation benchmark.

The tested local WCRT 1.3.0 DLL also turns `fread` into a per-byte `fgetc`
loop. Process I/O counters and disassembly show one-byte underlying reads.
WPM's 64 KiB application buffer therefore does not imply a 64 KiB OS read.
An explicit `setvbuf` control did not remove this bottleneck. This limits what
end-to-end timings can reveal about faster hash kernels on this runtime.

WCRT 1.3.1 fixes that read path. The
[2026-09-30 rerun](benchmarks/validation-wcrt-1.3.1-2026-09-30.md) reduces the
1 GiB scalar median from the earlier 55.6-minute sample to 20.8 seconds and
completes AVX2 in a 4.6-second median. Process read operations fall from roughly
one per byte to roughly one per 64 KiB payload chunk plus metadata reads. With
the I/O bottleneck removed, AVX2 is 4.5–4.9 times faster for the 100 MiB and
1 GiB workloads. Progress still receives no within-file updates.

## Decision drivers

- Preserve file-size, digest and completeness checks before execution.
- Evaluate many-small-file overhead separately from large-file hash throughput.
- Keep portable scalar fallback and runtime CPU/OS feature detection.
- Quantify both elapsed time and time without progress notifications.
- Keep performance experiments reproducible without production debug switches.

## Proposed decision

1. Retain runtime BLAKE2b dispatch and scalar fallback. Do not disable SIMD
   solely to make a progress bar appear smoother.
2. Use the four generated workloads as an opt-in performance baseline. Retain
   raw samples, host/build metadata, correctness controls, cache policy and
   timing scope with each report. Do not enforce host-dependent time limits
   in ordinary CI.
3. Treat progress responsiveness as a separate follow-up. Workers should
   publish byte counts during reads, and the caller should refresh progress
   while waiting, using the existing interactive cadence. Workers must not
   write to the console. Bytes processed must not be presented as successful
   validation until size and digest checks pass.
4. Use WCRT 1.3.1 or later when validation performance matters. Its effective
   bulk binary reads preserve the required error and size semantics and reduce
   process read operations from payload-byte scale to application-chunk scale.
   Merely increasing the application buffer or calling `setvbuf` was
   insufficient with WCRT 1.3.0.
5. With byte-at-a-time I/O addressed, profile small-file overhead and compare
   a serial control with the current pool before changing worker or batch
   counts. Do not attribute all elapsed time or variability to the hash kernel.

## Considered alternatives

| Alternative | Assessment |
| --- | --- |
| Return to scalar hashing | Gives up acceleration without removing batch-level progress gaps. |
| Lower the render interval alone | Cannot help while the caller is blocked waiting for a batch. |
| Reduce the batch size alone | May make small-file progress more granular but cannot show progress within one large file; requires overhead measurements. |
| Have each worker print progress | Risks interleaved output and couples hashing to console synchronization. |
| Caller renders worker byte counters | Addresses within-file visibility while retaining a single owner of console output; requires a safe wait/poll mechanism. |

## Consequences and acceptance criteria

The benchmark target is off by default and does not change production behavior.
The progress redesign above is a proposal, not implemented by this benchmark.
The bulk-read/runtime change was completed in WCRT 1.3.1 and validated by the
rerun. The unsuccessful WCRT 1.3.0 buffering experiment remains test-only.
It needs an XP-compatible synchronization approach and verification that errors
cannot become successful completion merely because all bytes were read.

For that follow-up, rerun the same throughput benchmark, record callback gaps,
and test a real interactive console. Long validations should allow updates
near the existing 100 ms interactive cadence while the worker is active,
subject to scheduler delays, with no per-chunk console I/O. Test a single large
file, one slow file among fast files, corruption, read errors and fallback
execution. Investigate material throughput regressions against the retained
baseline instead of trading measured performance for cosmetic updates blindly.

## References

- [WCRT issue #5: bulk fread causes one-byte ReadFile operations](https://github.com/Thewafflication/wcrt/issues/5)
- [Benchmark procedure](benchmarks/validation-method.md)
- [Local measurements and limitations](benchmarks/validation-2026-09-29.md)
- [WCRT 1.3.1 benchmark rerun](benchmarks/validation-wcrt-1.3.1-2026-09-30.md)
- [BLAKE2b runtime acceleration](blake2b-simd.md)
- [Package index validation requirements](req-0005-package-index-signature-verification.md)
- `wpm/archive.c`: `verify_file_task`, `verify_file_batch`, `verify_package_index`
- `wpm/progress.c`: interactive and redirected output throttles

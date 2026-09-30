# BLAKE2b validation after WCRT 1.3.1, 2026-09-30

WCRT 1.3.1 removes the byte-at-a-time `fread` bottleneck measured with WCRT
1.3.0. All four validation workloads now complete normally, including ten
measured scalar and AVX2 runs over the 1 GiB case. Large-file validation is
4.5–4.9 times faster with AVX2 than scalar on this host.

The runtime fix does not change WPM's progress granularity. Validation still
credits bytes only after an entire batch finishes, so a single 1 GiB file shows
no useful intermediate progress for a median 4.6 seconds with AVX2 or 20.7
seconds with scalar.

## Method

This rerun follows the [indexed-file validation benchmark](validation-method.md)
and uses the same host, deterministic fixtures, timing boundary, negative
controls, and validation code as the [WCRT 1.3.0 report](validation-2026-09-29.md).
Each backend received two untimed warmups and ten measured runs per workload.
Order alternated scalar/AVX2 and AVX2/scalar. Windows file cache was not evicted.

Both configurations were rebuilt from scratch with an explicit
`WPM_WCRT_ROOT=C:/Program Files/WCRT/1.3.1`. The copied runtime DLLs exactly
matched the installed x64 DLL:

`b4323740cb1e75ae7968dc323005115a161013e525b548c02f98d0806e6d4c15`

The scalar build used `WPM_BLAKE2B_SIMD=OFF`; the SIMD build used `ON` and
selected AVX2. Both passed the existing `blake2b-simd` CTest test. Before
measurement, both accepted a valid fixture and rejected same-size corruption,
truncation, and an unindexed extra file.

## Results

Elapsed values are medians of ten runs, followed by the observed range.
Throughput is aggregate payload MiB per second. A speedup above 1 means AVX2
was faster.

| Workload | Scalar elapsed | AVX2 elapsed | Scalar throughput | AVX2 throughput | AVX2 speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1,000 × 1 KiB | 0.874 s (0.831–1.127) | 1.039 s (0.834–1.533) | 1.12 MiB/s | 0.94 MiB/s | 0.84× |
| 100 × 100 KiB | 0.186 s (0.163–0.396) | 0.112 s (0.098–0.304) | 52.57 MiB/s | 87.46 MiB/s | 1.66× |
| 10 × 10 MiB | 1.113 s (1.020–1.251) | 0.228 s (0.197–0.594) | 89.86 MiB/s | 437.67 MiB/s | 4.87× |
| 1 × 1 GiB | 20.753 s (17.762–26.216) | 4.570 s (4.146–5.679) | 49.34 MiB/s | 224.09 MiB/s | 4.54× |

The 1,000-file case is dominated by per-file and index overhead; its scalar and
AVX2 ranges overlap, so the 0.84× median ratio is not evidence that the scalar
hash is intrinsically faster. SIMD becomes valuable as bytes per file increase.

## Change from WCRT 1.3.0

The old report used the same validation implementation but could only afford
ten repetitions for the two small cases and one attempt for the larger cases.
The new medians are compared with the old medians where available and old
single samples otherwise.

| Workload | Backend | WCRT 1.3.0 | WCRT 1.3.1 median | Runtime improvement |
| --- | --- | ---: | ---: | ---: |
| 1,000 × 1 KiB | Scalar | 2.834 s | 0.874 s | 3.24× |
| 1,000 × 1 KiB | AVX2 | 3.021 s | 1.039 s | 2.91× |
| 100 × 100 KiB | Scalar | 14.607 s | 0.186 s | 78.64× |
| 100 × 100 KiB | AVX2 | 14.089 s | 0.112 s | 126.18× |
| 10 × 10 MiB | Scalar | 368.459 s | 1.113 s | 331.08× |
| 10 × 10 MiB | AVX2 | 245.136 s | 0.228 s | 1,072.89× |
| 1 × 1 GiB | Scalar | 3,336.435 s | 20.753 s | 160.77× |
| 1 × 1 GiB | AVX2 | Timed out after 3,600 s | 4.570 s | More than 788× |

The old large-case rows are single samples and the old AVX2 1 GiB value is a
censored whole-process timeout, so the last four ratios should be read as scale
of impact rather than precise cross-version estimates.

## Read-operation evidence

`GetProcessIoCounters` covers the entire child process, including startup and
metadata reads. Counts were identical across all ten runs for a workload and
backend and identical between scalar and AVX2.

| Workload | Payload bytes | WCRT 1.3.1 read operations | WCRT 1.3.0 completed-run operations |
| --- | ---: | ---: | ---: |
| 1,000 × 1 KiB | 1,024,000 | 282,093 | Not captured |
| 100 × 100 KiB | 10,240,000 | 28,993 | Not captured |
| 10 × 10 MiB | 104,857,600 | 4,613 | 104,860,614 |
| 1 × 1 GiB | 1,073,741,824 | 16,775 | 1,073,742,216 |

For the 1 GiB file, 16,384 application payload reads are expected at 64 KiB
each; the remaining process reads cover startup, index parsing, EOF checks, and
other metadata. The 1.3.1 count therefore scales with chunks rather than bytes.
The many-small-file cases still perform substantial index and per-file work,
which explains why their operation counts do not approach only payload chunks.

## Progress behavior

These measurements wrap caller notifications from `wpm_progress_add`; they do
not capture interactive console frames. Because the verifier waits for a full
batch before reporting each file, a workload that fits in one batch has no
within-batch byte updates.

| Workload | Scalar median longest notification gap | AVX2 median longest notification gap |
| --- | ---: | ---: |
| 1,000 × 1 KiB | 245 ms | 300 ms |
| 100 × 100 KiB | 48 ms | 36 ms |
| 10 × 10 MiB | 1.105 s | 221 ms |
| 1 × 1 GiB | 20.747 s | 4.565 s |

The WCRT fix makes validation vastly faster, but the user's original progress
observation remains valid. A single large file appears frozen until hashing is
complete. The progress redesign proposed in
[ADR-0015](../adr-0015-validation-performance-and-progress.md) remains useful,
especially for scalar fallback and slower storage or CPUs.

## Evidence

- [Raw samples](data/validation-wcrt131-2026-09-30/samples.csv)
- [Computed summary](data/validation-wcrt131-2026-09-30/summary.json)
- [Run metadata, controls, and executable hashes](data/validation-wcrt131-2026-09-30/metadata.json)
- [Runtime identity and correctness checks](data/validation-wcrt131-2026-09-30/runtime.json)
- [WCRT issue #5](https://github.com/Thewafflication/wcrt/issues/5)


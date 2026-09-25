# SSE2 compression and extraction

On x86 and x64, WPM dispatches zlib-ng hash-table sliding, fast inflate, and
overlapping copy operations to SSE2 when WCRT reports usable SSE2. Each entry
checks WCRT's thread-safe CPU query, with no mutable WPM dispatch state. Other
zlib-ng functions remain generic. ARM64 and CPUs without usable SSE2 use scalar
code. ZIP format, compression settings, and package hashes are unchanged.

`WPM_ZLIB_SSE2` defaults to ON. Set it OFF for a scalar zlib-ng build. TinyCC
compiles the upstream kernels using its SSE2 headers (tested with
`0.9.28-rc.1448+72495402`). No global SSE2 compiler flag is applied. The scoped
`immintrin.h` shim supplies only `emmintrin.h` to upstream SSE2 inflate code.
Upstream submodule sources are unchanged. Generic entry points are renamed at
compile time and wrapped with runtime feature checks.

The hash sliding kernel uses unsigned saturated subtraction on eight 16-bit
positions per iteration. The wrapper retains scalar sliding for custom allocator
tables without 16-byte alignment. TinyCC package 1448 addressed the intrinsic
overhead reported in [issue #4](https://github.com/Thewafflication/tcc_package/issues/4),
so WPM uses the upstream intrinsic implementation without a private assembly loop.

`zlib-sse2` tests force scalar and SSE2 dispatch independently, executing SSE2
only when the real CPU/OS supports it. They compare compressed bytes and cross-
decode both paths at levels 1, 2, 6, and 9, with random/repetitive inputs, unaligned
buffers, and block/window boundaries. Overlapping-copy tests exercise distances
1–32 and capacities 0–64 with surrounding canaries. The package regression suite
also exercises raw DEFLATE streaming, extraction, verification, and malformed input.

Run `out/build/x64-release/wpm/wpm-zlib-test.exe --benchmark` for a non-gating
32 MiB in-memory benchmark at level 2. A local i7-12700H x64 run using TinyCC
`0.9.28-rc.1448+72495402` and the upstream SSE2 kernel measured:

| Path | Compression | Extraction |
| --- | ---: | ---: |
| Scalar | 0.625 s | 0.250 s |
| SSE2 | 0.391 s | 0.109 s |

These are illustrative local measurements on repetitive data, not package-level
or universal speedup claims. Disk access, file mix, hashing, and compression
ratio affect overall performance. BLAKE2b acceleration is configured separately
by `WPM_BLAKE2B_SIMD`; see [its documentation](blake2b-simd.md).

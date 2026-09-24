# BLAKE2b runtime acceleration

WPM uses the pinned libsodium SSSE3, SSE4.1, and AVX2 compression kernels on
x86/x64. Digests, index format, and verification policy are unchanged. The
default remains safe on processors without these instructions: libsodium's
selector prefers AVX2, SSE4.1, SSSE3, then its scalar implementation. Its runtime
detector checks CPUID and requires AVX, XSAVE, OSXSAVE and XCR0 SSE/AVX state
before reporting AVX2. Initialization completes before worker threads start;
the selected function pointer is not changed during verification.

## Build boundary

TinyCC does not ship the intrinsic headers needed by these kernels. Clang
compiles only their upstream source files at O3. It emits ELF objects because
TinyCC's linker consumes that format, even for Windows output. The x64 entry
points explicitly use the Microsoft ABI; x86 uses cdecl with stack realignment.
No SIMD value or runtime object crosses that boundary, only pointers to the
upstream hash state and input bytes. The build rejects undefined symbols using
LLVM's `llvm-nm`, so no additional CRT dependency can enter these kernels.
`llvm-objcopy` moves their constant pools into the code section, retaining ELF
alignment within the page-aligned PE section. This avoids observed loss of
32-byte constant alignment when TinyCC merges PE read-only data. The rest of
libsodium and WPM still use TinyCC/WCRT.
No upstream submodule source is modified.

Feature definitions are restricted to libsodium's BLAKE2b selector and CPU
detector. Other crypto backends are not accidentally enabled. A small TinyCC
XGETBV hook avoids the GCC clobber spelling in the upstream assembly fallback.
No whole-program AVX/SSE target flags are used.

`WPM_BLAKE2B_SIMD` defaults to ON for x86/x64. These builds require Clang,
discovered automatically or specified with `WPM_SIMD_CLANG`. OFF removes the
Clang/LLVM dependency and optimized objects. The Clang installation must include
`llvm-objcopy` and `llvm-nm`. ARM64 remains scalar.

## Validation and benchmarking

The `blake2b-simd` CTest test uses the actual upstream selector with synthetic
feature queries to verify priority and fallback without executing unsupported
instructions. It then runs each backend supported by the real CPU/OS through
the public streaming API, comparing with scalar BLAKE2b. Coverage includes
32/64-byte digests, keyed/unkeyed inputs, an independent empty-message vector,
unaligned buffers, empty files, block boundaries and read-buffer boundaries.
Package regression tests cover threaded verification and signature failures.

The test-only translation unit includes upstream `blake2b-ref.c` so its private
selector can be inspected and forced. Production WPM uses the ordinary upstream
translation unit with real runtime feature detection.

Run `out/build/x64-release/wpm/wpm-blake2b-test.exe --benchmark` after building
to measure each supported backend over 64 MiB in memory. This isolates hash
throughput; it does not measure disk access, decompression, or package-level
speedup. The benchmark flag is confined to the test executable, and timing is
not a test pass/fail condition.

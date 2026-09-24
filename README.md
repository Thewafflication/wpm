# WPM

WPM is the Waughtal Package Manager, a command-line tool for building,
installing, removing, and upgrading packages.

## WPM 1.0.0

WPM 1.0.0 is the first stable release of the Waughtal Package Manager for
Windows x86, x64, and ARM64. It provides signed package build, installation,
removal, HTTPS and local-filesystem repository use, version-aware upgrades,
and safe WPM self-upgrade.

Use the latest-release installer below to install or upgrade WPM. See the
[support policy](docs/support-policy.md) for the scope of best-effort support,
and the [2.0 UX roadmap](docs/roadmap-2.0.md) for planned improvements.

## Install

From an Administrator Command Prompt or PowerShell window, download and run the
PowerShell 2.0-compatible bootstrap installer. It performs a machine-wide
installation by default:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$p=Join-Path $env:TEMP 'wpm-install.ps1'; $c=New-Object Net.WebClient; try { try { [Net.ServicePointManager]::SecurityProtocol=3072 } catch {}; $c.Headers.Add('User-Agent','WPM-Bootstrap'); $c.DownloadFile('https://github.com/Thewafflication/wpm/releases/latest/download/install.ps1',$p); & $p } finally { $c.Dispose(); Remove-Item $p -Force -ErrorAction SilentlyContinue }"
```

For a per-user installation, append `-User` after `& $p` in that command.
The script itself supports Windows PowerShell 2.0 and uses only facilities
available on Windows XP. However, an unmodified XP HTTPS stack cannot connect
to GitHub's TLS 1.2 endpoints. On XP, place a TLS 1.2-capable `curl.exe` on
`PATH`, then run these commands from an Administrator Command Prompt:

```bat
curl.exe -fL --tlsv1.2 https://github.com/Thewafflication/wpm/releases/latest/download/install.ps1 -o "%TEMP%\wpm-install.ps1"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%TEMP%\wpm-install.ps1"
del "%TEMP%\wpm-install.ps1"
```

The bootstrapper selects the native x86, x64, or ARM64 package from the latest
release index, downloads its release public key, validates the signed package
with the downloaded WPM executable in an isolated temporary trust store, and
then runs the packaged `setup.cmd`. Its initial trust anchor is GitHub HTTPS;
the public-key identifier is documented below for independent verification.

See the [usage guide](docs/usage.md) for commands, examples, and the WPM
package layout.

See the [1.0 release roadmap](docs/roadmap-1.0.md) for the 1.0 scope and
release criteria.

See the [2.0 UX roadmap](docs/roadmap-2.0.md) for the next major release.

See [compatibility tracking](docs/compatibility-tracking.md) for platforms
under investigation that are not yet supported releases.

See the [quality testing program](docs/quality-testing.md) for long-running and
fault-injection testing that complements regular CI.

See the [WSP adoption record](docs/wsp-adoption.md) for the pinned engineering
process baseline, selected profiles, and integration status.

WPM follows WSP's shared logging contract in both the C executable and its
PowerShell test harness. Windows executables use a native-handle sink to avoid
crossing incompatible CRT `FILE *` ABIs. Operational command output is appended
to the `audit\wpm.log` file under WPM's data directory. Set `WPM_LOG_FILE` to
select another application log. Set `WPM_LOG_LEVEL` to `normal` to retain
informational, result, warning, and error records while excluding `Verbose:`
detail, or to `verbose` (the default) to retain all operational detail. Unknown
log levels do not disable command execution, but WPM warns that it could not
initialize its operational log. Set `WPM_TEST_LOG_FILE` to retain timestamped
test-run summaries.

See the [release-documentation procedure](documentation/README.md) for PDF
construction, verification, visual review, checksums, and publication control.

See the [project process](docs/project-process.md) for planning, review,
verification, release, support, and improvement controls.

See the [support policy](docs/support-policy.md) for supported releases and
issue-reporting guidance.

Report suspected vulnerabilities through the private process in the
[security policy](SECURITY.md).

WPM uses third-party open-source software. See
[Third-Party Notices](THIRD_PARTY_NOTICES.md) for attribution and licenses.

## Building

All supported Windows builds use TinyCC and link their C library calls to WCRT.
The standard x86, x64, and ARM64 presets find the newest package beneath
`%ProgramFiles%\WCRT`, or use
`WPM_WCRT_ROOT` when that CMake or environment variable is set. The selected
WCRT 1.3.0-or-newer package provides shared headers, its bounded POSIX
compatibility declarations, and architecture-specific
targets beneath its `x86`, `x64`, and `arm64` directories. Microsoft C compiler
builds are not supported.

Indexed package verification uses up to four WCRT worker threads, each with
its own file stream. Work is bounded in batches; progress and errors are
reported in index order. Verification falls back to synchronous work if
worker-pool resources are unavailable.

On x86/x64, BLAKE2b selects AVX2, SSE4.1, SSSE3, or the scalar fallback at
runtime. AVX2 requires both CPU support and OS support for saving SSE/AVX state.
Selection happens during libsodium initialization before verification workers
start. `--verbose` reports the selected implementation.

The x86/x64 build requires Clang with `llvm-objcopy` and `llvm-nm` for these
three compression kernels only;
TinyCC still compiles WPM and links the executable against WCRT. CMake finds
Clang in PATH, LLVM, or Visual Studio, or accepts `-DWPM_SIMD_CLANG=<path>`.
Use `-DWPM_BLAKE2B_SIMD=OFF` for a scalar-only build without Clang. ARM64 keeps
the scalar implementation. All implementations preserve existing package hashes.

```powershell
cmake --preset x64-release
cmake --build --preset build-x64-release
```

Replace `x64` with `x86` or `arm64` as needed. ARM64 tests must run on an ARM64
Windows host. WCRT is linked statically so `wpm.exe` remains self-contained for
installation and self-upgrade handoffs.

## Official release signing key

Official WPM packages are signed with the durable public key in
[`release_keys/wpm-release.public`](release_keys/wpm-release.public). Verify
the key identifier before trusting it:

```text
6c21ebda96b16bfa64700d28fcdeb20dc4b19de448a723a970ac09b720f04b1d
```

After obtaining the key from a trusted project source, add it once with:

```text
wpm trust add wpm-release.public
```

WPM does not automatically trust keys distributed by package repositories.

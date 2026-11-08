# WPM 1.3.4 - installer file-user discovery

## Change and impact

Adds `wpm file-processes <file...> [--kill] [--dry-run] [--legacy]` so install
scripts can find applications using exact destination files before overwriting
them. Listing is the default. Explicit kill checks process identity, protects
installer ancestors, services and critical processes, and waits for exit.

Restart Manager is used on current Windows. Windows 2000/XP automatically use
the legacy loaded-module and disk-file-handle scanner. `--legacy` selects that
backend for diagnostics. Native x86/x64 fixture tests exercise unshared data
files and loaded DLLs. Actual Windows 2000/XP execution remains unverified;
the native smoke procedure is in `docs/file-processes-legacy.md`. This does not
expand the published platform support policy. Legacy access/architecture gaps
produce warnings; install scripts must check actual copy failures. Forceful
termination can lose unsaved data and does not restart applications.

Existing install defaults, signing/trust rules, package formats, schemas, and
upgrade selection are unchanged. Scripts opt in by calling the utility.

## Verification and release readiness

The user authorized commit, push and release on 2026-11-07. Version 1.3.4 is the
next compatible stable increment after 1.3.3. The source is the release commit
on `codex/file-processes`; the tag records its exact identity. The controlled
WSP pin remains `c2dc862e22ccd251be2e32ffba9401496091e598`.

Current-host x86 Release and x64 Debug builds, both discovery backends, native
file/DLL holders, dry-run, TC-0013, C99/style, traceability and x86 PE checks
passed during implementation. Real legacy execution and ARM64 runtime evidence
must not be inferred from those results.

The earlier full-run blockers are addressed before commit: pre-existing Python
caches are preserved under `out/preserved-wsp-caches-20261107`, outside the
controlled WSP worktree, and the I/O-heavy recovery-record fixture has a bounded
300-second total budget. Its assertions, fixture size and individual 30-second
command deadlines are unchanged; direct execution had already passed while
the previous 120-second suite budget expired. No test is skipped or waived.

Commit requires the normal lint, build and complete fresh x64 CTest hooks.
Publication requires the tag-triggered Release workflow's x86/x64/ARM64 Debug
verification, Release builds, signed packages, prior-release upgrade checks,
and controlled documentation publication. The workflow retains architecture
evidence and produces three signed package archives, repository index, release
key, bootstrap scripts, engineering PDF and checksums. CI failure blocks
publication; no exception to these gates is approved.

The existing previous-release packages remain available for recovery. Report
script failures with their retained log and check copy exit codes before
claiming an application upgrade succeeded. The normal security and support
policies continue to apply.

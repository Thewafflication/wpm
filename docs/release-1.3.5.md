# WPM 1.3.5 - installer file-user discovery

## Scope and authorization

This is the corrected publication of the installer file-user feature and
documentation improvements described in `docs/release-1.3.4.md`. The user
authorized commit, push and release on 2026-11-07. The failed 1.3.4 tag is
preserved; no assets were published for it. The next compatible release is
1.3.5, from the final `codex/file-processes` commit identified by its tag.

`wpm file-processes <file...> [--kill] [--dry-run] [--legacy]` provides explicit
installer discovery and termination, with Restart Manager and a Windows
2000/XP fallback. Actual Windows 2000/XP execution remains unverified and the
formal supported-platform policy remains unchanged. Native current-host x86,
x64 and ARM64 verification covers the feature. Scripts must handle access and
architecture gaps and check actual copy errors; forceful termination can lose
unsaved data. Existing signing, package formats, upgrade defaults and the
controlled WSP pin remain unchanged.

## CI correction and verification

The 1.3.4 documentation job found two executable matches on PATH (Chocolatey's
shim and the installed Pandoc binary). PowerShell returned an array, and the
adapter attempted to invoke both paths as one command. Selecting the first
application match fixes discovery while retaining the explicit executable
override. Verification reproduces duplicate PATH entries without that override
and checks the default adapter and full PDF build.

The renderer wraps long code, paths, identifiers and Markdown table columns,
and parses controlled TeX test specifications as readable chapters. The local
documentation candidate was rendered/reviewed and passed identity, checksum
and independent text-boundary checks. Final CI identity/provenance and package
verification are required before completion; evidence is retained with the
release workflow and publication record.

The normal lint, build and complete fresh x64 CTest commit hooks remain
mandatory. The tag-triggered Release workflow must pass x86/x64/ARM64 Debug
verification, Release builds, signed packaging, previous-release upgrade
checks and controlled PDF publication. No gate is waived. Expected assets are
three signed package archives, the repository index, release public key,
bootstrap scripts, engineering PDF and its SHA-256 checksum. The previous
published release remains available for recovery. Native legacy smoke testing
remains a documented follow-up rather than a verified compatibility claim.

# WPM 1.3.6 - confirmed recursive installer copies

## Scope and authorization

The user authorized push and release of the recursive copy follow-up.
`wpm copy <source> <destination> --recursive` (or `-r`) merges directory
contents, including empty subdirectories, into the exact destination and
retains destination-only files. Single-file copying is also supported.

The default `[y/N]` prompt authorizes stopping applications using destination
files and copying/overwriting. Negative, empty and EOF input cancel before
termination or copying. `--yes`/`-y` authorizes unattended execution;
`--dry-run` previews without mutation. Existing modern/legacy file-user
backends and process protections are reused. Termination may lose unsaved
work; copies can partially complete on failure. Native Windows 2000/XP
execution remains unverified and formal supported-platform policy is unchanged.

REQ-0013.041 through REQ-0013.043, TC-0013 and the companion copy integration
tests cover the change. See `docs/change-recursive-copy.md` for detailed scope
and current-host verification. Existing signing, package formats, WSP pin and
upgrade defaults remain unchanged.

## Verification and publication criteria

Current-host x64 Debug passed 60/60 tests in 366.55 seconds. Both default and
forced-legacy copy integrations passed on x64 and x86. The x86 build passed
the XP PE check (OS/subsystem 4.0, 128 kernel32 imports). Project style, C99,
traceability/validator and WSP adoption checks passed.

The normal commit hooks must pass again. The immutable release tag identifies
the final source revision. Tag-triggered Release CI must pass x86/x64/ARM64
verification, signed release packaging, previous-release upgrade tests and
controlled documentation verification/provenance before publication. Expected
assets are three signed architecture packages, index, release public key,
bootstrap scripts, engineering PDF and SHA-256 checksum. Published bytes,
package signatures and PDF source identity must be checked before completion.

The engineering PDF is rendered and visually reviewed before the release tag.
Exact body matches may reuse the prior approved rendering; every changed body
must be inspected. Final CI/published PDF identity, checksum and page boundaries
are verified against the tagged source. No validation gate is waived.

The local candidate passed structure/identity/checksum checks with 360 pages,
1,076 links and zero independent text-boundary exceptions. All page bodies were
reviewed: 326 matched the approved 1.3.5 rendering exactly, and all 34 changed
bodies were inspected, including the copy usage and requirements at higher
resolution. No clipped text, broken tables or section-transition defects were
observed. Rendering/comparison evidence is retained under
`out/release-1.3.6-review`; the final CI artifact still requires tagged identity
and published-byte verification.

# WPM 2.0 CP-01B Log-Configuration Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Add configurable operational-log verbosity

**Owner and date:** WPM maintainers, 2026-09-01

## Scope

This increment adds `WPM_LOG_LEVEL=normal|verbose` to the existing configurable
operational-log destination. `normal` excludes diagnostic `Verbose:` records while
retaining informational, result, warning, and error evidence. `verbose` preserves
the previous all-record behavior and remains the default.

Verification exposed and this increment corrects a pre-existing adapter defect:
operational logging was compiled out of Windows builds because the WSP logger's
`FILE *` cannot safely cross the TinyCC/WCRT boundary. The documented default and
`WPM_LOG_FILE` destinations therefore produced no Windows log. Windows builds now
use an equivalent native `HANDLE` adapter, while other builds retain the WSP logger.
Both logging settings use the same Windows environment boundary as `WPM_DATA_DIR`.
Archive and repository verbose messages are emitted as one classified record rather
than three unclassified output fragments.

The cross-CRT integration gap was tracked upstream as
[WSP issue 3](https://github.com/Thewafflication/wsp/issues/3) and resolved in
WSP 1.3.0. WPM now supplies a native `HANDLE` byte sink through that ABI-safe
contract; WSP owns record framing, timestamping, severity filtering, and sink
status while WPM retains ownership of the Windows write and close operations.

An unknown level prevents persistent logging and produces the existing operational-
log initialization warning without preventing the requested command. Package,
repository, signature, installation, and audit formats are unchanged.

## Risk and verification

The principal risk is suppressing required failure evidence at a lower verbosity.
TC-0031 runs a failing verification with a custom log path at both levels, proves
the error is retained in each log, proves only verbose mode retains diagnostic
detail, checks timestamp/severity structure, and covers invalid-level recovery.
TC-0012's read-only verification assertion is narrowed to installation audit
records so the newly active operational log does not masquerade as mutation.

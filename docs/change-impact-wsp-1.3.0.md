# WSP 1.3.0 Adoption Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Upgrade the pinned Waughtal Software Process baseline from commit
`2198ccab08f969a789448767fe7017b774369adc` to release `1.3.0` at commit
`8666277d0a30575515f5e46524e5b71be4be0c7d`

**Owner and date:** WPM maintainers, 2026-09-01

## Scope and baseline review

The upgrade reviews the cumulative WSP 1.1.0, 1.2.0, and 1.3.0 changes. It
updates WPM's gitlink, adoption record, controlled 2.0 planning baseline, and
TC-0023 exact-pin verification together. Historical adoption rows retain their
original commit identities.

WSP 1.2.0 added selectable UX/UI, information-for-users, and C/C++ static-
analysis profiles. This increment records them but does not select them;
adoption requires dedicated scope, allocation, evidence, and tailoring work.
The selected security profile gained native-build-hardening requirements
`WSP-SEC-0015` and `WSP-SEC-0016`; both are Deferred until the WPM 2.0 quality
and release increment establishes architecture-specific controls and retained
final-binary evidence.

The common baseline gained upstream-workaround control `WSP-PROC-0011` and
native standard-input controls `WSP-TEST-0019` through `WSP-TEST-0021`.
`WSP-PROC-0011` and `WSP-TEST-0020` are Applicable. The remaining input
coverage is Deferred because TC-0013 proves native redirected affirmative and
negative branches but does not yet prove EOF-before-data, injectable input
failure, or a separate genuine-console matrix entry. The adoption record
contains the completion conditions and compensating evidence.

## Implementation and compatibility impact

WSP 1.3.0 resolves the cross-CRT logging defect tracked by WSP issue 3. WPM now
links the pinned WSP logger on Windows and installs an ABI-safe byte sink. WPM
continues to own its native append `HANDLE` and close operation; WSP owns
timestamp and severity framing, threshold filtering, and sink lifecycle. This
removes the duplicated Windows record formatter without changing the public
CLI, environment variables, log destination, message classification, package
format, repository format, or signing boundary.

WPM keeps its native console and color adapter and disables WSP terminal
detection with the documented `WSP_LOG_NO_TTY` definition. This avoids WCRT's
absent `_isatty` export and is behaviorally neutral because WSP's console sink
is configured off before any WPM record is emitted.

The canonical WSP record format uses an ISO 8601 UTC timestamp followed by a
fixed-width severity tag. TC-0031 is updated to assert this contract rather
than the superseded local timestamp brackets. The output remains plain,
line-oriented, and append-only at the configured destination.

## Risk and verification

The principal implementation risks are a cross-runtime stream regression,
failure to close the native handle, severity-filter drift, or loss of failure
evidence. The byte callback exchanges only caller-owned bytes and length; it
passes no CRT object across the boundary. `wsp_log_close` invokes WPM's close
callback exactly once.

Verification includes the WSP adoption validator, traceability validator,
TC-0023 exact-pin and negative-mutation coverage, TC-0031 normal/verbose/error
logging coverage, TC-0032 failure-path log guidance, C99 lint, the x64 build,
and the local x64 CTest suite. GitHub Actions remains responsible for native
x86 and ARM64 execution after push.

The documentation workflow installs the PDF verifier dependencies from the
pinned `wsp/tools/pdf/requirements.txt` before running the expanded WSP common-
tool self-tests. This keeps the adopting workflow synchronized with the exact
submodule baseline instead of duplicating one package version.

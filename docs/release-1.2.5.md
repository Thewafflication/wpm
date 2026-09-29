# WPM 1.2.5 compatibility correction

## Change and impact

The maintainer requested issues, fixes, commit/push, and release for corporate
Windows 11 download failures and missing XP update byte counts (issues #1 and
#2). Version 1.2.4 changed the default transport from Windows URLMon to bundled
TLS. The work-PC log shows remote refresh and package retrieval failures with
local repositories still usable; the prior release worked. It does not contain
the TLS error, so proxy, enterprise CA, and endpoint-policy causes cannot be
distinguished from that log alone.

Automatic selection restores URLMon on Windows 8 and newer and retains bundled
TLS on older systems. Explicit choices remain authoritative. A private CA with
auto selects bundled TLS, while URLMon plus a private CA fails closed. Transport
selection occurs before connecting; failed verification never changes trust
stores. This restores the previous Windows trust/proxy boundary on modern hosts.
Package signature policy, repository formats, and bootstrap behavior are unchanged.

TLS errors now use the operational logger. Progress writes use native Windows
handles rather than mixing buffered stdio with cursor movement, and completion
always retains a byte count. Long labels are shortened to leave room for counts;
URLMon also obtains final size from the completed file when callbacks are absent.
The precise XP rendering failure cannot be reproduced on this Windows 11 host;
the change addresses the output path and short-download completion behavior.

## Verification and release criteria

The HTTPS probe covers legacy/modern selection, explicit overrides, invalid
choices, and CA conflicts. Production CLI checks cover auto selection, private
CA selection, failed-CA logging, and invalid URLMon/CA combinations. TLS fixtures
retain positive framing/redirect cases and negative certificate, downgrade,
truncation, and malformed-response checks. Progress checks cover a short index,
unknown length, partial failure, counts above 4 GiB, and maximum 64-bit counts
with long labels through the interactive completion path.

Run x86/x64 CTest suites, C99 lint, and live GitHub refreshes through both
backends. Retain local command evidence in out/fix-*.log. The tag-triggered Release
workflow must pass architecture verification, signed packaging, prior-release
upgrades, and documentation publication before the release is considered complete.
CI retains architecture-specific evidence and publishes the three packages.

## Limitations and follow-up

Actual work-PC network and XP-console confirmation remains outstanding; do not
represent modern-host fixtures or import checks as those runtime confirmations.
A reporter has confirmed that 1.2.4's bundled transport works on XP. Broader legacy
OS validation remains best effort. For 1.2.4 recovery on the corporate PC, select
WPM_HTTPS_BACKEND=urlmon in the shell before update and self-upgrade, provided no
explicit private CA policy is configured. See bundled-https.md for commands.

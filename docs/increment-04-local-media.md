# I-04 local media: compatible failure-handling delivery

Status: partial increment delivery in 1.2.13. Broader removable/optical media
identity and replacement evidence remains open; I-04 is not declared complete.

## Delivered behavior

Local and UNC copies obtain the size from an opened read handle, hold the source
against concurrent write/delete sharing, and verify the copied byte count before
publishing. Read, write, flush, and publication failures discard the temporary
copy and preserve any previous cache. Failures identify the source, copy phase,
Windows error, and a retry action: reconnect the original media at the configured
path, check access, and rerun. No automatic drive relocation or trust change is
introduced. Existing valid 1.x cache and offline behavior is preserved.

## Evidence

`tests/local-media-integration.py` uses a real Windows file handle to deny access
to an index during refresh, then checks exact cache preservation. It disconnects
a repository by renaming its directory and verifies that named installation
fails without executing its script or substituting an old local index. It also
removes a selected archive, checks retry guidance and temporary-file cleanup,
then restores it and installs a trusted signed package from read-only files.
Source content must remain unchanged. The new test runs in the full native
x86/x64/ARM64 verification suite and the component verification target.

`tests/tc-0024-local-repositories.ps1` continues to cover relative-root
canonicalization, source preservation, and unsafe locator rejection. The linked
schema-1 layout delivered in 1.2.12 remains additive and is tested separately.

## Remaining work

Controlled removable and optical media demonstrations, source identity across
media substitution, and deterministic partial-read fault injection remain open.
UNC authentication/disconnection and opted-in HTTP transport evidence belongs
to I-05. Breaking changes stay reserved for 2.x; compatible slices ship on 1.x.

## Optional confirmation source identity (1.2.19)

Plans capture the opened archive's Windows volume serial and file identifier,
include them in the approval digest, and recheck the selected path before
approval, execution replay, and package scripts. Replacing an archive with a
byte-identical copy after preview therefore invalidates the plan. Existing
content fingerprints and state/trust checks remain in force. Default 1.x
commands retain their behavior; these checks apply to the explicit `--confirm`
workflow and read-only plan construction.

The operation integration fixture performs a real Windows rename and replacement
while the preview is waiting for approval, confirms equal archive bytes, and
requires rejection before installation. A stable replacement file can then be
planned and approved normally. Dry-run snapshots verify no new writes.
This deterministic file identity test does not close the remaining physical
removable/optical media demonstrations or platform-specific SMB identity work.

## Acquisition and cache publication diagnostics (1.2.20)

Named installation now reports invalid package URLs only for invalid index
locators/identities. Missing local archives retain the source-specific copy
error; optional confirmation names the unavailable selected package and asks
for media reconnection, access checks, and a recomputed plan. Default command
behavior and existing unsafe-locator rejection remain unchanged.

A failed atomic index publication reports its Windows error, affected cache
path, and a retry action to close locking programs or correct write access.
The loopback fixture holds a real read handle that denies replacement, verifies
exact cache bytes and modification time, requires candidate cleanup and cached
graph fallback, then unlocks and proves retry publication succeeds.

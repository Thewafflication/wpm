# I-02: Shared Operation Plans

**Content type:** Controlled increment record

**Owner:** WPM maintainers

**Status:** Implemented; completion requires commit gates and native release matrix

## Scope and impact

Install, remove, selected upgrade, upgrade-all, and self-upgrade share immutable
plan collection and rendering. Payload index hashes, ZIP CRC/path validation,
and known-key signature/trust checks run without extraction. Each plan includes
identity, architecture, version transition, source/input, trust decision,
controlled roots, package script, retained archive effects, and completion.
Self-upgrade distinguishes scheduling from completed deployment.

Approval binds to a BLAKE2b digest over plan fields, archive fingerprints, and
package/trust/configuration state. Execution resolves candidates again and
rejects mismatches before staging; script entry rechecks archive/state. Inputs
remain open without write sharing. Duplicate execution authorization is denied.
The controlled state is refreshed after each successfully completed plan item.
These checks do not promise atomicity against arbitrary external deployment
changes or rollback of package-owned scripts.

All lifecycle commands accept `-y`/`--yes`; otherwise only `y` or `yes` approves.
Empty input/EOF defaults to cancellation. Invalidated approval is an error.
Trust additions occur explicitly through `wpm trust add`, not inside planning.
The unattended change is released as a 2.0 prerelease. I-02 retains normal
repository/cache preparation and logging; I-03 removes those capabilities from
dry-run and establishes explicit preparation authorization boundaries.

## Verification allocation

- REQ-0015.001/.002/.003/.006: operation integration runs actual install/remove,
  checks complete plan rendering, EOF/empty/negative input, explicit approval,
  and config changes between preview and approval without script execution.
- TC-0013 exercises selected/all upgrades, cancellation/acceptance, continued
  processing, failed scripts, and independently validated self-upgrade handoff.
- TC-0005/TC-0012 exercise tampered/missing/truncated content, malformed indexes,
  signed/unsigned/unknown/revoked trust partitions with read-only validation.
- Current lifecycle test fixtures explicitly authorize unattended deployment;
  stdin confirmation fixtures retain interactive approval checks.

Completion requires the required lint/build/full tests and native x86/x64/ARM64
release matrix. Parent REQ-0015 remains Proposed until composite dry-run and
later recovery allocations are reconciled. New package, index, or signature
formats are not introduced by this increment.

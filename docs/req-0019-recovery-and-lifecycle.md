# REQ-0019: Recovery and Cleanup

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 6

## Scope

Applies to inspection, retry, cleanup, uninstall guidance, backup, and restore
for interrupted installation, failed package scripts, failed self-upgrades,
stale operational artifacts, and legacy package records.

## Requirement

**REQ-0019.001**
An interrupted install, failed package script, or failed self-upgrade shall
produce or retain a recovery record that identifies the package identity,
operation, selected source, completed and failed phase, relevant non-secret
paths, validation and signer state, script exit status when available,
retained artifacts, and the next safe inspect, retry, or cleanup command.

**REQ-0019.002**
WPM shall provide a read-only command to inspect a failed operation from its
audit or recovery record. Inspection shall detect missing, changed, malformed,
or untrusted retained inputs and shall not present a retry as safe when its
preconditions no longer hold.

**REQ-0019.003**
WPM shall provide a supported retry path for eligible failed operations. Before
reuse, retry shall recompute the operation plan and revalidate package identity,
archive integrity, signature, trust, source, architecture, version, paths, and
current installed state. It shall not blindly replay a package script or
self-upgrade handoff and shall require confirmation under REQ-0015 when the
recomputed mutation remains consequential.

**REQ-0019.004**
WPM shall provide a supported cleanup path for stale cache entries, abandoned
staging directories, failed-operation artifacts, and legacy package records.
Cleanup shall classify active, recoverable, evidence-retained, and removable
state conservatively; display a complete plan; support dry-run and safe
confirmation; and remain idempotent.

**REQ-0019.005**
Cleanup shall not delete active installations, current package records,
required trust/configuration state, pending self-upgrade inputs, the only
artifact required for an eligible recovery, or preserved failure evidence
within its required retention. Unsafe roots, reparse points, races, locked
files, and changed state shall fail closed without broadening the deletion set.

**REQ-0019.006**
WPM removal and product-uninstall guidance shall distinguish WPM executable
and runtime files, machine and user mutable data, installed package records,
retained archives, cache/staging content, audit/recovery evidence, trust and
repository configuration, and user configuration. It shall state what is
removed, retained, recoverable, or requires an explicit cleanup choice.

**REQ-0019.007**
Documentation shall define backup and restore expectations for
`%ProgramData%\WPM`, explicit `WPM_DATA_DIR` roots, portable/user-scoped data,
and configuration. It shall identify quiescence, permissions, protected trust
material, pending operations, required validation after restore, supported
whole-state restoration, and unsupported partial-restore risks.

**REQ-0019.008**
Recovery, retry, cleanup, and restore behavior shall be verified on x86, x64,
and ARM64. Original failing results and diagnostics shall be retained and
linked to corrective changes and successful reruns; a later pass shall not
overwrite or reclassify the original failure.

**REQ-0019.009**
WPM shall not claim universal rollback of arbitrary package-script effects.
When deployed state may have changed outside WPM's controlled records, recovery
output shall identify that limitation and direct the user to package-specific
or administrator recovery guidance.

## Rationale

WPM controls validation, staging, records, and retained artifacts but cannot
reverse every action performed by a package script. Durable recovery identity,
revalidation, conservative cleanup, and honest boundaries make failures
inspectable without creating a false transaction guarantee.

## Verification

**Method:** Automated test, fault injection, security inspection, and
architecture-matrix demonstration

**References:** Planned TC-0019; `docs/traceability-2.0.md`

Planned verification injects failure at every lifecycle phase, tampers with or
removes retained inputs, repeats retry and cleanup, exercises locked and
reparse-point state, proves active-state preservation, restores controlled
fixtures, and preserves linked failure/rerun evidence on x86, x64, and ARM64.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 6.
- **Depends on:** REQ-0004, REQ-0007, REQ-0008, REQ-0012, REQ-0013,
  REQ-0014, REQ-0015, REQ-0018, WSP-TEST-0009, WSP-SEC-0011, and the
  accepted no-universal-rollback decision in `docs/dfs.md`.
- **Conflicts with:** None identified.

## Change Impact

This requirement adds durable recovery schema and new commands with
potentially destructive cleanup behavior. It affects audits, retained
artifacts, self-upgrade, installed-state inspection, support, evidence
retention, and DFS threats WPM-THR-009 and WPM-THR-012. ADR and security review
must define record compatibility, atomicity, authorization, deletion roots,
races, and migration of legacy state before implementation.

## Tailoring

None. A recovery operation that cannot be safely automated may provide an
inspect-only result and controlled manual guidance, but shall not claim retry
or cleanup success.

## Implementation Record

Planned allocation is a versioned recovery-record reader/writer, retry planner,
and root-contained cleanup classifier shared with diagnose/show and the
operation-plan layer. TC-0019 will supply fault, deletion-boundary, restore,
and architecture evidence. No implementation is claimed by this proposed
baseline.

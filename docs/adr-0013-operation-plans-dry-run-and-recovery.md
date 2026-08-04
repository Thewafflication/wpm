# ADR-0013: Operation Plans, Dry Run, and Recovery

**Status:** Accepted

**Date:** 2026-08-04

**Relationship:** Supplements ADR-0003, ADR-0006, and ADR-0009. It defines WPM's
transaction boundary and durable recovery identity without claiming rollback
of package-script effects.

## Context

WPM validates and stages packages but delegates deployment to arbitrary scripts.
WPM 2.0 must show a complete plan, guarantee dry-run purity, recover interrupted
WPM-controlled phases, and clean abandoned state conservatively. Calling this a
general transaction would falsely imply rollback of external script effects.

## Decision Drivers

- Confirmation must authorize an exact immutable mutation set.
- Dry-run purity must be enforced below individual command handlers.
- WPM-owned state needs recoverable prepare/commit boundaries.
- Retained recovery data must never become a blind replay mechanism.
- Cleanup must not escape controlled roots or delete the only recovery evidence.
- Legacy installed archives and 1.x records must remain usable.

## Considered Options

1. Immutable plans, capability-separated execution, and versioned recovery records.
2. A presentation-only dry-run flag in each command.
3. A write-ahead journal that promises rollback of package scripts.
4. Retry by replaying stored commands and paths.

## Decision

Resolution SHALL produce an immutable operation plan before confirmation. The
plan contains resolved package and source identities, architecture and versions,
input fingerprints, affected controlled roots, ordered mutation intents, scripts
that may execute, retained artifacts, and deferred handoff work. A canonical
plan digest binds confirmation, execution, audit, and recovery.

Immediately before the first mutation and at each identity-sensitive phase,
WPM rechecks plan preconditions. Any candidate, source, trust, path, content, or
mutation-set change invalidates the plan and requires recomputation and new
confirmation when applicable.

The execution layer receives either read-only capabilities or mutation
capabilities. `--dry-run` selects only the read-only set below command dispatch;
a code path that requests mutation fails the dry run. Dry run may parse and
validate inputs in place and compute a plan in memory, but it creates no cache,
staging, log, audit, recovery, trust, configuration, repository, retained
artifact, process, or handoff state and invokes no package script. Commands
whose validation requires mutation reject `--dry-run` explicitly.

## WPM Transaction Boundary

For WPM-owned files, each mutation uses prepare, validate, commit, and cleanup.
Prepared files live only beneath an operation-specific controlled root and
become authoritative through atomic same-volume replacement where Windows and
the target filesystem provide it. Multi-file operations are not claimed to be
globally atomic; their phase and authoritative files are captured by the
recovery record.

Package-script, MSI, service, driver, registry, and arbitrary deployment effects
are outside this transaction boundary. WPM records when execution may have
crossed that boundary and never describes such state as rolled back merely
because its own files were restored.

## Recovery Record

Before the first durable mutation, WPM atomically writes a versioned recovery
record. The initial schema identifier is `wpm.recovery.v1`. It contains:

- operation ID, type, timestamps, plan digest, and controlled status;
- package, architecture, old/new version, and redacted source identity;
- current, completed, and failed phase plus script exit status when available;
- public signer/validation state and input fingerprints;
- allow-listed controlled paths and retained artifact roles and digests;
- whether external script state may have changed;
- retry eligibility and safe inspect, retry, or cleanup actions.

Required unknown values are explicit `null`, not guessed. Records contain no
arguments or environment snapshots that could carry credentials. Updates use
atomic replacement; failure to create the initial record blocks mutation.
Later record-write failure preserves the last valid record, stops further WPM
mutation where safe, returns failure, and identifies that recovery evidence may
be incomplete.

Recovery inspection is read-only. Retry treats the record only as identity and
diagnostic input: it rereads current state, revalidates all retained content and
trust, recomputes a new plan, and obtains new confirmation. It never executes a
stored command line, blindly reuses a path, or assumes a prior signature remains
authorized.

## Cleanup Boundary

Cleanup begins with a read-only inventory and classifies each object as active,
recoverable, evidence-retained, removable, or unknown. Unknown and changed
objects are retained. Deletion is limited to canonical descendants of explicit
WPM cache, staging, recovery, and legacy-record roots; root objects themselves,
active package records, trust/configuration, pending handoff inputs, the sole
eligible recovery artifact, and retained evidence are excluded.

Before each deletion WPM rechecks identity and classification. Reparse points,
device paths, unexpected hard-link or file type conditions, races, and locked
objects fail the affected item closed without widening scope. Cleanup remains
plan-driven, confirmed, dry-runnable, and idempotent.

## Compatibility and Migration

Package archives, version-1 repository metadata, and existing installed-package
records are unchanged. Recovery records are additive. Legacy operations without
one remain inspectable only from available audit and installed state; WPM does
not synthesize a safe retry. Unsupported or future recovery schema versions are
preserved and reported, not deleted or interpreted as current.

## Security Consequences

Capability separation makes dry-run purity reviewable and reduces accidental
writes. Recovery records improve diagnosis but disclose operational identity, so
filesystem access and output redaction remain required. Cleanup is a destructive
privileged boundary and requires stricter path and race handling than ordinary
diagnostics.

## Explicit Non-Goals

- WPM does not promise ACID transactions across package scripts or Windows.
- Dry run does not predict undocumented script effects because scripts do not run.
- Recovery records are not executable journals and do not make retry automatically
  safe.
- Cleanup does not remove arbitrary software deployed by package scripts.
- Backup and restore do not support merging partial state from unrelated baselines.

## Consequences

Mutating commands share a planner, capability gate, and recovery writer instead
of duplicating confirmation and cleanup logic. Some validation must be refactored
to accept read-only inputs. Fault injection, before/after state snapshots,
retained failure evidence, deletion-boundary tests, and native architecture
recovery evidence become mandatory before runtime requirements are accepted.

## References

- REQ-0015, REQ-0017, REQ-0019, and planned TC-0015, TC-0017, and TC-0019
- ADR-0003, ADR-0006, ADR-0009, and ADR-0012
- `docs/dfs.md`


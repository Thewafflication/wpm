# REQ-0015: Safer Package Changes

**Content type:** Project requirements

**Status:** Proposed

**Source:** WPM 2.0 roadmap, Milestone 2

## Scope

Applies to install, remove, upgrade, self-upgrade, and other commands whose
documented operation can mutate package, cache, trust, repository,
configuration, audit, staging, retained-artifact, or deployment state.

## Requirement

**REQ-0015.001**
Before asking for confirmation, WPM shall display a complete operation plan
for install, remove, and upgrade. The plan shall identify each package name,
architecture, current and selected versions where applicable, source or
repository, affected paths when practical, scripts that may execute, retained
artifacts, and work deferred until WPM exits.

**REQ-0015.002**
The operation plan and subsequent progress shall distinguish content that is
to be or has been downloaded, copied, verified, staged, installed, removed,
retained, or scheduled for completion by another process. A final result shall
not describe scheduled or partially completed work as installed, removed, or
upgraded.

**REQ-0015.003**
Consequential confirmation shall default to rejection on any response other
than the documented affirmative input, including empty input and end-of-file.
`-y` and `--yes` shall be the consistent non-interactive confirmation bypass
for supported commands and shall not suppress the displayed or machine-readable
plan.

**REQ-0015.004**
Supported mutating commands shall accept `--dry-run` and shall produce the same
resolved plan and validation diagnostics available without committing the
operation. A dry run shall not modify installed payloads, package records,
cache, staging, trust, repository configuration, general configuration,
operational-log, audit/recovery state, retained artifacts, or self-upgrade
handoff state and shall not invoke package scripts.

**REQ-0015.005**
WPM shall reject `--dry-run` on a command for which it cannot guarantee the
documented no-mutation boundary. It shall reject contradictory or misleading
combinations rather than silently ignoring `--dry-run`, confirmation, or
non-interactive options.

**REQ-0015.006**
Plan computation shall fail closed when package identity, source, trust,
affected state, or mutation classification is ambiguous. A plan shall not
authorize a different candidate, architecture, version, source, or mutation
set without being recomputed and, when confirmation applies, approved again.

## Rationale

Package scripts can make broad system changes and cannot be universally rolled
back. Showing the exact intended change, using a safe confirmation default,
and enforcing dry-run beneath command presentation reduce unintended
operations and make automation reviewable.

## Verification

**Method:** Automated test, fault injection, inspection, and demonstration

**References:** Planned TC-0015; `docs/traceability-2.0.md`

Planned verification compares filesystem, configuration, trust, cache,
staging, audit, and process state before and after dry runs; exercises
affirmative, negative, empty, EOF, and bypass confirmation; and proves that a
changed or ambiguous plan cannot execute without recomputation.

## Relationships

- **Derived from:** `docs/roadmap-2.0.md` Milestone 2.
- **Governed by:** ADR-0013's immutable plan, capability, transaction, and
  recovery boundaries.
- **Depends on:** REQ-0004, REQ-0008, REQ-0012, REQ-0013, REQ-0014,
  WSP-SEC-0007, and WSP-SEC-0011.
- **Conflicts with:** None. REQ-0013's existing `upgrade --all` plan and safe
  confirmation are retained and generalized.

## Change Impact

This requirement affects command parsing, candidate resolution, mutation
classification, confirmation, package-script invocation, self-upgrade handoff,
and tests that inspect state. It does not require transactionality or universal
rollback. Security review must prove that validation performed during a dry run
does not grant trust or persist attacker-controlled input.

## Tailoring

None. A command may omit dry-run only through an approved requirement change
that explains why a no-mutation guarantee is infeasible.

## Implementation Record

Planned allocation is a shared immutable operation-plan model and a mutation
guard below command dispatch, with command-specific adapters for install,
remove, upgrade, trust, repository, and configuration operations. TC-0015 will
provide state-difference and process-invocation evidence. ADR-0013 explicitly
excludes persistent operational logging from dry run and package-script effects
from WPM's transaction boundary. No implementation is claimed by this proposed
baseline.

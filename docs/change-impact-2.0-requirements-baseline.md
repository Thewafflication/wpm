# WPM 2.0 Baseline Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Proposed

**Change:** Establish REQ-0014 through REQ-0022 and
`docs/traceability-2.0.md`

**Owner and date:** WPM maintainers, 2026-08-04

## Changed controlled artifacts

This change converts every unimplemented obligation in
`docs/roadmap-2.0.md` into identified proposed requirements, allocates each
subordinate obligation to planned verification, extends static traceability
validation, updates the controlled documentation manifest, and records the
applicable proposed WSP dispositions. It does not implement product behavior,
accept the proposed requirements, or claim verification evidence.

## Requirements and WSP dispositions

- REQ-0014 through REQ-0022 are new and use the next unused stable parent
  identifiers after the accepted 1.x baseline.
- Each normative obligation has a stable subordinate identifier and one row in
  the 2.0 traceability matrix.
- Planned TC-0014 through TC-0022 do not assert that test specifications,
  runners, executions, or passing evidence exist.
- WSP-REQM-0001 through WSP-REQM-0005 gain proposed 2.0 baseline evidence.
  WSP-REQM-0010, release-process, test-evidence, C-style, and release-trust
  dispositions remain deferred until their completion evidence exists.

## Architecture, interfaces, formats, and migration

No source, command, API, ABI, package, signature, repository-index,
installed-record, recovery-record, or configuration format changes in this
baseline change. REQ-0016 identifies a future conflict with REQ-0011's
HTTPS-only restriction; implementation is blocked until an approved ADR and
requirement change reconcile that contract. Other new durable decisions are
explicitly left for the architecture/DFS change package.

## Implementation and dependencies

Implementation allocation is recorded in each requirement's Implementation
Record. The intended sequence is output/help, safe operation planning,
transport-neutral repositories, repository authoring, diagnostics, recovery,
quality testing, portability/reference documentation, and release closure.
The pinned `wsp/` submodule and third-party dependencies are unchanged.

## Tests, matrix, evidence, and retained prior results

The existing 1.0 traceability and TC-0001 through TC-0013 remain authoritative
and unchanged. Normal static validation permits `Planned` rows only for
Proposed requirements. `-ReleaseBaseline 2.0` rejects every non-Verified row,
missing evidence, absent controlled test artifact, or unregistered automated
test. No previous execution evidence is reclassified or overwritten.

## Compatibility and supported platforms

The baseline preserves x86, x64, and ARM64 as the release matrix and makes no
Windows 2000 support commitment. Proposed implementation includes new local,
SMB, and opted-in HTTP behavior, but the accepted and released 1.x HTTPS-only
behavior remains unchanged until a reviewed implementation change lands.

## Security assets, threats, boundaries, controls, and residual risk

The proposed requirements affect repository transport, HTTP warnings,
removable media, repository signing, operation plans, dry-run purity,
machine-readable output, recovery records, cleanup deletion, test corpora, and
release evidence. These touch DFS threats WPM-THR-001, WPM-THR-003,
WPM-THR-004, WPM-THR-006, WPM-THR-009, WPM-THR-010, WPM-THR-011, and
WPM-THR-012. No trust boundary changes in this documentation-only change; ADR
and DFS updates are prerequisites to affected implementation.

## Documentation, notices, support, and users

The controlled engineering PDF gains this analysis, the nine requirement
documents, the 2.0 traceability matrix, and the work plan. User-facing usage,
support, migration, release-note, and recovery documentation remain planned
and are not represented as current executable behavior.

## Released versions, rollback, and recovery

Released 1.x baselines are unaffected. This change is reversible by normal
version-control review, but stable identifiers shall not be reused if the
proposal is later rejected or superseded. No runtime rollback or recovery
action applies.

## Schedule, coordination, assumptions, and uncertainty

Architecture and DFS decisions, test allocation, implementation, matrix
execution, and release records are separate dependent changes. The principal
uncertainties are command/schema details, supported HTTP opt-in persistence,
repository index-signing compatibility, recovery-record compatibility, and
availability of native SMB, optical-media, and ARM64 evidence environments.

## Overall risk and recommendation

Documentation-change risk is low; downstream implementation and compatibility
risk is high and is made visible by Proposed status and explicit release gates.
Approve the baseline for planning only. Do not accept a requirement or report
a roadmap item complete until its architecture, implementation, controlled
verification, evidence, review, and WSP disposition are complete.

## Required approvals

Requirement owner and maintainer approval are required to accept this proposed
baseline. Architecture and security owners must approve later ADR/DFS changes.
The release approver must independently confirm the exact verified baseline
before WPM 2.0.0 publication.

# WPM 2.0 Architecture and Security Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Accepted

**Change:** Establish ADR-0010 through ADR-0013, update the DFS, resolve the
REQ-0011/REQ-0016 transport contradiction, and add REQ-0023/TC-0023 static
baseline controls

**Owner and date:** WPM maintainers, 2026-08-04

## Objective, scope, and exclusions

This change makes the minimum durable decisions needed before implementing the
proposed WPM 2.0 output, repository, authoring, dry-run, machine-output, recovery,
and cleanup behavior. It updates controlled architecture/security documentation
and adds static consistency verification. It does not implement those runtime
features, accept REQ-0014 through REQ-0022, change a 1.x executable or format,
publish an artifact, or modify the pinned `wsp/` submodule.

Completion for this change means every accepted ADR has a recorded disposition,
the affected requirements point to the governing decisions, the accepted 1.x
transport restriction has an explicit 2.0 supersession rule, DFS coverage is
complete for the new boundaries, and TC-0023 passes. Runtime completion remains
Planned in `docs/traceability-2.0.md`.

## Accepted ADR review

- **ADR-0001 package trust - Compatible; supplemented by ADR-0012.**
  REQ-0017 uses a repository-scoped index-signer role that does not authorize
  package signers or store private keys.
- **ADR-0002 repository trust - Compatible; supplemented by ADR-0012.**
  REQ-0016/0017 retain repositories as distribution only; index-signer pinning
  is independent and cannot bootstrap package trust.
- **ADR-0003 package/install semantics - Compatible; supplemented by ADR-0013.**
  REQ-0015/0019 add WPM-owned planning and recovery without claiming rollback or
  ownership of package-script effects.
- **ADR-0004 SemVer/dependency constraints - Compatible unchanged.**
  REQ-0016/0018 reuse deterministic SemVer and selection; no version syntax or
  solver change is authorized.
- **ADR-0005 repository architecture - Partially superseded by ADR-0011;
  supplemented by ADR-0012.** REQ-0016 replaces the 2.0 provider set/locator
  syntax and adds HTTP policy; the common version-1 structure, cache, priority,
  and transport/trust separation remain. REQ-0017 defines authoring and
  index-signing authority.
- **ADR-0006 deployment coordination - Compatible; supplemented by ADR-0013.**
  REQ-0015 binds confirmation to an immutable plan; REQ-0019 records partial
  deployment without adding a SAT solver or universal rollback.
- **ADR-0007 automated verification - Compatible; supplemented by ADR-0010 and
  TC-0023.** REQ-0014/0018 share typed results and machine fixtures; REQ-0023
  adds controlled static architecture verification without replacing runtime
  tests.
- **ADR-0008 metadata-only inspection - Compatible unchanged.** REQ-0018
  continues bounded metadata-only installed-state queries and requires no
  payload extraction or second installed-state authority.
- **ADR-0009 installation performance - Compatible; supplemented by ADR-0013.**
  Staging and verification remain; dry-run and recovery do not authorize direct
  installation or infer script destinations.

No accepted ADR is silently rewritten. ADR-0011 states exactly which ADR-0005
substance it supersedes for 2.0, and ADR-0010, ADR-0012, and ADR-0013 identify
their supplemental relationships.

## Requirements and contradiction resolution

- REQ-0014 and REQ-0018 now allocate presentation and machine serialization to
  ADR-0010.
- REQ-0015 and REQ-0019 now allocate immutable plans, read-only capabilities,
  recovery records, and cleanup to ADR-0013.
- REQ-0016 explicitly supersedes only REQ-0011.001's HTTPS-only repository
  restriction and REQ-0011.004's HTTPS-only package-location restriction for
  the 2.0 baseline. REQ-0011 remains the accepted 1.x contract and is unchanged.
- REQ-0016 is governed by ADR-0011; REQ-0017 is governed by ADR-0012 and
  ADR-0013. The version-1 index and package schemas remain unchanged.
- REQ-0023 supplies stable identifiers for this documentation behavior before
  its static runner and CTest registration are added.

REQ-0014 through REQ-0022 remain Proposed. Architecture acceptance is a
prerequisite, not objective evidence of runtime implementation or verification.

## Interfaces, formats, and migration

The accepted architecture defines future internal boundaries and additive 2.0
contracts:

- typed command events and `wpm.output.v1` JSON envelopes;
- typed repository locators and a narrow read-only source interface;
- a separate local repository writer and repository-scoped index-signer pin;
- immutable operation plans, read-only/mutation capability separation, and
  additive `wpm.recovery.v1` records.

The version-1 `index.json`, packages, signatures, installed archives, and 1.x
repository configuration remain readable. Existing HTTPS repository entries
migrate with HTTP disabled and no pinned index signer. That legacy mode retains
mandatory package validation and reports its lower index assurance. Recovery
records are additive; WPM must not synthesize a retry from legacy state.

No API or ABI exists outside the executable. Future modules are to remain narrow:
presentation renderers consume events, repository readers do not author, local
writers do not become transports, and recovery records do not execute work.

## Implementation and dependency impact

No product source or third-party dependency changes in this change. Future
implementation affects the CLI dispatcher, logger integration, repository
configuration and cache, filesystem/HTTP readers, local authoring, signing,
operation planning, audit/recovery state, and cleanup. It must remain portable C
for the declared baseline and keep Windows path/filesystem behavior valid on
x86, x64, and ARM64.

The pinned WSP gitlink remains
`2198ccab08f969a789448767fe7017b774369adc`. TC-0023 checks this identity and the
cross-document contract. A future WSP update must change adoption history and
the test baseline together through a separate approved impact analysis.

## Verification and evidence impact

TC-0023 is a controlled static test for this documentation slice. Its automated
runner checks accepted-ADR dispositions, supplemental/superseding relationships,
explicit non-goals, affected requirement back-references, DFS coverage,
documentation-manifest inclusion, and the WSP gitlink. Negative fixtures prove
that representative omissions fail.

TC-0014 through TC-0022 and every runtime row remain Planned. The subsequent
test-allocation baseline adds controlled specifications and runner contracts
without product execution or CTest registration. Local TC-0023 and
traceability passes are development evidence only; the exact merge baseline
must rerun in CI. Native SMB, removable/optical media, HTTP, signing,
destructive cleanup, recovery fault injection, and x86/x64/ARM64 runtime
evidence cannot be claimed by either static change.

## Security impact

### Assets and boundaries

DFS coverage adds repository-index signing keys and signer policy, authored
repositories and temporary replacements, command/machine output, immutable
plans, recovery records, cleanup classifications, and removable-media identity.
New boundaries cover event-to-renderer redaction, locator-to-source access,
untrusted archive-to-authoring root, private key-to-index signature,
plan-to-mutation capabilities, recovery-record-to-retry, and
cleanup-classification-to-deletion.

### Threats and controls

The DFS extends repository compromise and secret-disclosure analysis and adds
explicit threats for insecure HTTP/downgrade, removable-media substitution,
authoring escape/race, index-signer confusion, output injection/serialization,
dry-run mutation, unsafe recovery replay, and cleanup escape/evidence deletion.
Controls include repository-scoped opt-in and signer pinning, transport-neutral
validation, atomic cache/authoring replacement, pre-commit identity checks,
central redaction, read-only capability enforcement, versioned non-executable
recovery records, and conservative root-contained cleanup.

### Residual risk

HTTP still lacks confidentiality and transport freshness; removable and SMB
sources can disappear; authorized index signers can hide/replay entries;
filesystem atomicity and reparse behavior vary; local administrators can alter
records; and arbitrary scripts remain outside universal rollback. These risks
are visible and do not authorize bypass of package validation or release gates.

## Documentation, support, and users

The engineering-documentation manifest gains this analysis, four ADRs,
REQ-0023, and TC-0023. User-facing usage, migration, recovery, backup/restore,
HTTP warning, authoring, and machine-schema documentation remain implementation
deliverables under REQ-0014 through REQ-0022; this change must not present the
future command forms as currently available.

## Released versions, rollback, and recovery

Released 1.x behavior and formats are unaffected. This documentation change is
reversible through version control, but ADR and requirement identifiers are not
reused. No deployed-system rollback or data migration occurs in this slice.

## Schedule, coordination, and external evidence

Architecture and security owners approve this record with the ADR/DFS change.
Feature owners must implement and verify REQ-0014 through REQ-0019 in dependency
order. Release approval remains blocked until proposed requirements are accepted,
all planned rows have objective evidence, and environmental/native matrix gates
pass. Required external evidence includes real SMB authentication/disconnect,
removable-media loss/replacement, read-only optical media, real HTTP/TLS policy,
protected private-key signing, cleanup races/reparse points, and native x86,
x64, and ARM64 recovery execution.

## Overall risk and recommendation

Documentation and static-test risk is low. Downstream security and compatibility
risk remains high because transport, signing, destructive cleanup, and recovery
cross privileged boundaries. Accept these decisions as implementation
constraints and proceed only through the identified proposed requirements and
controlled tests. Do not report any runtime roadmap item complete from this
change.

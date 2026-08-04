# WPM 2.0 Test Allocation Baseline Change Impact

**Content type:** Controlled change-impact analysis

**Status:** Proposed

**Change:** Establish controlled TC-0014 through TC-0022 specifications,
non-claiming runner contracts, execution profiles, evidence paths, release
gates, and complete planned-test validation under REQ-0023.006

**Owner and date:** WPM maintainers, 2026-08-04

## Changed controlled artifact

This change implements CP-00C of `docs/work-plan-2.0.md`. It controls test
design and delivery allocation for REQ-0014 through REQ-0022 and strengthens
the accepted static-consistency requirement and validator. It does not
implement proposed product behavior, execute a runtime test, create release
evidence, accept a proposed requirement, close a roadmap item, or modify the
pinned `wsp/` submodule.

Completion for this change means each new 2.0 subordinate requirement appears
in exactly one controlled TC plan; every plan uses intentional test-design
techniques, defines the five execution profiles, objective expected results,
evidence paths, and release gates; unimplemented runners cannot report Pass;
and positive/negative static validation detects drift.

## Requirements and WSP dispositions

- REQ-0014 through REQ-0022 retain Proposed status and their existing stable
  subordinate identifiers. Their verification and Implementation Records now
  reference controlled TC-0014 through TC-0022 artifacts rather than bare
  planned identifiers.
- REQ-0023 gains REQ-0023.006 before validator implementation. It requires
  proposed/accepted 2.0 specifications and runner plans to be complete and
  prevents unimplemented runners from becoming passing product tests.
- WSP-TEST-0001 through WSP-TEST-0008, WSP-TEST-0011, WSP-TEST-0013, and
  WSP-TEST-0015 are applied through controlled allocation, the pinned template,
  safe Blocked status, automated validation, matrix planning, and technique
  selection. No WSP disposition changes.
- WSP-TEST-0009, WSP-TEST-0016, and WSP-TEST-0018 remain Deferred. Planned
  retention paths, Debug artifacts, and backtrace gates are not completion
  evidence.

## Architecture, interfaces, formats, and migration

No product architecture, command, API, ABI, package, signature, version-1
repository-index, installed-record, configuration, or recovery-record format
changes. `wpm.test-plan.v1` is an internal project test-planning description,
not a WPM product or public machine-output schema. Planned runner contracts
will be replaced or extended by executable assertions in their implementation
slices without changing their stable TC identifiers.

## Implementation and dependencies

New project-owned LaTeX specifications and PowerShell runner contracts are
added for TC-0014 through TC-0022. A common helper builds deterministic plan
descriptions and returns controlled Blocked output for execution requests.
Traceability validation becomes the enforcement point for one specification,
one runner, complete subordinate allocation, five profiles, objective expected
results, evidence paths, and gates.

Dependency review found two corrections: CP-03 depends on CP-02 because
REQ-0016 depends on the shared REQ-0015 operation-plan/dry-run boundary; CP-08
depends on CP-07 because REQ-0021 depends on REQ-0020 and needs the accepted
quality-harness boundary. Other delivery dependencies remain unchanged.

## Tests, matrix, evidence, and retained prior results

TC-0014 through TC-0022 specify Fast, PlatformMatrix, Quality,
ManualRealEnvironment, and ReleaseGate profiles. Planned evidence uses
`Testing/Evidence/2.0/<source-revision>/<TC>/<profile>/<architecture>/`.
Descriptions, placeholder paths, and Blocked runner output are not execution
evidence. No prior result is reclassified or overwritten.

Normal traceability/validator tests are development checks for the controlled
allocation only. The `-ReleaseBaseline 2.0` gate remains intentionally failing
until requirements are Accepted, product runners are registered, every row is
Verified, and exact retained evidence exists. Native x86/x64/ARM64, SMB,
HTTP/TLS, USB, optical, protected-key, recovery, destructive cleanup, quality,
signing, scanning, and release-approval evidence remains future work.

CI subsequently exposed a validator completion-status defect: all controlled
positive and negative fixtures passed, but the top-level script inherited exit
code 1 from the last expected failing child validation. The corrective change
updates REQ-0023.006 and TC-0023 before implementation and makes successful
top-level completion explicitly return zero after fixture cleanup. Unexpected
assertions and cleanup failures still terminate nonzero. This changes only the
test-infrastructure verdict; it does not reclassify any requirement, trace row,
runtime result, or retained evidence.

## Compatibility and supported platforms

The runtime support matrix remains x86, x64, and ARM64 on supported Windows.
The added plan-description runners require PowerShell 5.1-compatible syntax
and do not invoke WPM or mutate platform state. No 1.x test, format, behavior,
or support claim changes.

## Security assets, threats, boundaries, controls, and residual risk

The change handles only public test-plan metadata and test-only paths. Runner
descriptions contain no command arguments, credentials, keys, environment
snapshots, repository locators, or user data. Execution requests stop before
product interaction and emit a fixed secret-free Blocked record. Future test
implementations must use disposable roots/test-only keys, redact protected
inputs, preserve the DFS boundaries, and retain hostile corpus data safely.

Residual risk is that a future implementation could leave the descriptive plan
accurate while executable assertions drift. The release gate compensates by
requiring Accepted requirements, implemented CTest allocation, review of the
controlled specification/runner relationship, and exact evidence. Static
allocation alone never establishes runtime security or compatibility.

## Documentation, notices, support, and users

The engineering-documentation manifest gains this impact analysis and the nine
TC specifications. The test strategy and work plan gain execution-profile,
evidence, gate, identifier, and dependency detail. Requirement and traceability
wording is reconciled. User-facing commands and usage documentation are not
changed because no proposed behavior exists yet.

## Released versions, rollback, and recovery

Released 1.x baselines and artifacts are unaffected. This change is reversible
through version control, but REQ/TC identifiers remain reserved. No deployed
rollback, data migration, recovery action, tag, signing, or publication applies.

## Schedule or coordination impact

CP-01 through CP-09 implement the controlled runner profiles in dependency
order. Managed environments are still required for native architectures,
console input, SMB, removable/optical media, HTTP/TLS policy, protected signing,
administrator restore/cleanup, security/release review, and publication
approval. Absence of any required environment blocks the corresponding gate.

## New assumptions or uncertainty

The common evidence hierarchy is assumed to map into immutable CI artifacts
without changing its relative structure. Exact CI runner images, toolchain
versions, protected environments, corpus limits, and release-record locations
remain implementation/release decisions. Evidence may refine test cases but
cannot silently weaken a requirement or gate.

## Overall risk and recommendation

Documentation/test-infrastructure risk is low; downstream runtime, destructive,
transport, signing, and release risk remains high and explicitly Planned.
Approve the allocation baseline only after the positive and negative validators
pass and review confirms coverage. Do not accept REQ-0014 through REQ-0022 or
claim CP/roadmap completion from this change.

## Required approvals

Requirement owner, verifier, and maintainer approval are required. Architecture
or security-owner approval is required if review changes an accepted ADR/DFS
boundary. Release approval remains separate and blocked on objective evidence.

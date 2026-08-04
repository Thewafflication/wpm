# REQ-0023: Architecture and Security Consistency

**Content type:** Project requirements

**Status:** Accepted

**Source:** WPM project process, WSP architecture/security change control, and
the WPM 2.0 architecture change-impact analysis

## Scope

Applies to the controlled WPM 2.0 requirements, accepted ADRs, DFS, change-impact
records, traceability, documentation manifest, and automated static validation.
It verifies the engineering baseline; it does not claim that proposed 2.0
runtime behavior exists.

## Requirement

**REQ-0023.001**
Every accepted ADR that predates the WPM 2.0 requirements shall have an explicit
review disposition of compatible, supplemented, or partially superseded, with
the affected 2.0 requirements and rationale identified.

**REQ-0023.002**
Each 2.0 supplemental or superseding ADR shall identify the governed accepted
decision, compatibility and migration effect, security consequences, explicit
non-goals, requirements, and verification allocation.

**REQ-0023.003**
The DFS shall identify protected assets, trust boundaries, threats, controls,
verification, and residual risks for HTTP opt-in, removable/read-only media,
repository authoring and index signing, operation plans and dry-run, machine
output, recovery records, and destructive cleanup.

**REQ-0023.004**
Affected 2.0 requirement documents shall reference their governing ADRs and
shall explicitly resolve any accepted 1.x requirement restriction that the 2.0
baseline supersedes without modifying the accepted 1.x requirement text.

**REQ-0023.005**
An automated static runner shall reject a missing required architecture or DFS
artifact, missing bidirectional reference, missing explicit non-goal, incomplete
accepted-ADR disposition, missing documentation-manifest entry, or modification
of the pinned `wsp/` gitlink from the adopted baseline.

**REQ-0023.006**
Automated traceability validation shall require exactly one controlled test
specification and executable runner contract for every Proposed or Accepted
WPM 2.0 requirement. It shall reject a runner plan that omits an identified
subordinate requirement, any Fast, PlatformMatrix, Quality,
ManualRealEnvironment, or ReleaseGate profile, an objective expected result,
an evidence path, or a release-gate allocation. A runner for unimplemented
Proposed behavior shall report Blocked and shall not be registered or reported
as passing product verification.

## Rationale

Architecture and security controls span several proposed feature requirements.
A narrow static requirement prevents those records from drifting before feature
implementation while keeping runtime verification states honestly Planned.

## Verification

**Method:** Automated static test and inspection

**References:** TC-0023 and `tests/tc-0023-architecture-security-consistency.ps1`

Verified by the controlled test specification, architecture runner, and
traceability-validator tests, including negative fixtures for missing
disposition, reference, non-goal, DFS coverage, manifest entry, WSP gitlink
identity, planned specification/runner, requirement allocation, profile,
expected result, evidence path, and gate.

## Relationships

- **Derived from:** `docs/project-process.md`, `docs/ts-0001-test-strategy.md`,
  WSP-REQM-0005, WSP-PROC-0004, WSP-SEC-0002, and WSP-SEC-0012.
- **Depends on:** REQ-0014 through REQ-0022, ADR-0001 through ADR-0013,
  `docs/dfs.md`, and `docs/ts-0001-test-strategy.md`.
- **Conflicts with:** None. It adds static baseline verification and changes no
  1.x product format or behavior.

## Change Impact

This requirement adds documentation and static tests only. It affects the
documentation manifest, planned-runner inventory, 2.0 traceability, CTest
registration rules, and validation time. It does not change product source,
command behavior, data formats, security permissions, supported architectures,
dependencies, or release state.

## Tailoring

None. Changes to the expected WSP gitlink require an approved WSP adoption
update before the validator baseline changes.

## Implementation Record

Allocated to the four 2.0 ADRs, the updated DFS and requirement relationships,
the architecture and test-allocation change-impact records, TC-0023, its
PowerShell runner, and the positive/negative traceability validator. A passing
local execution verifies the documentation/test-plan baseline only; CI remains
required evidence for the controlled merge baseline.
